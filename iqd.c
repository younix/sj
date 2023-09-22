/*
 * Copyright (c) 2014 Jan Klemkow <j.klemkow@wemelug.de>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/queue.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <assert.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <mxml.h>

#include "bxml/bxml.h"

struct context {
	int fd_in;
	struct bxml_ctx *bxml;
	char *dir;
};

#define NULL_CONTEXT {		\
	STDIN_FILENO,		\
	NULL,			\
	"."			\
}

void
sigalarm(int sig)
{
	assert(sig == SIGALRM);
}

static void
recv_iq(char *tag, void *data)
{
	struct context *ctx = data;
	/* HACK: we need this, cause mxml can't parse tags by itself */
	static mxml_node_t *tree = NULL;
	mxml_node_t *node = NULL;
	const char *base = "<?xml ?>";
	const char *tag_name = NULL;
	const char *tag_type = NULL;
	const char *tag_id = NULL;
	const char *tag_ns = NULL;
	char path[PATH_MAX];
	int fd;

	if (tree == NULL) tree = mxmlLoadString(tree, base, MXML_NO_CALLBACK);  
	if (tree == NULL) err(EXIT_FAILURE, "unable to load xml base tag");

	mxmlLoadString(tree, tag, MXML_NO_CALLBACK);
	if ((node = mxmlGetFirstChild(tree)) == NULL)
		goto err;

	if ((tag_name = mxmlGetElement(node)) == NULL) goto err;
	if (strcmp("iq", tag_name) != 0) goto err;

	if ((tag_type = mxmlElementGetAttr(node, "type")) == NULL)
		goto err;

	/* handle get/set */
	if (strcmp(tag_type, "get") == 0 || strcmp(tag_type, "set") == 0) {
		struct stat sb;

		mxml_node_t *child = mxmlFindElement(node, tree, NULL,
		    "xmlns", NULL, MXML_DESCEND);

		if ((tag_ns = mxmlElementGetAttr(child, "xmlns")) == NULL)
			goto err;

		/* TODO: deal with this kind of namespaces */
		if (strncmp(tag_ns, "http://jabber.org/protocol/", 27) == 0)
			return;

		/* filter namespaces with '..' to avoid exploitation */
		if (strstr(tag_ns, "..") != NULL)
			return;

		snprintf(path, sizeof path, "%s/ext/%s", ctx->dir, tag_ns);
		if (stat(path, &sb) == -1) {
			if (errno == ENOENT)
				goto err;
			goto err;
		}

		if (S_ISREG(sb.st_mode) && sb.st_mode & S_IXUSR) {
			/* pipe tag to 3th party extension program */
			char cmd[BUFSIZ];
			FILE *fh;

			snprintf(cmd, sizeof cmd, "exec %s -d '%s'", path,
			    ctx->dir);

			if ((fh = popen(cmd, "w")) == NULL) goto err;
			if (fwrite(tag, strlen(tag), 1, fh) == 0) goto err;
			if (pclose(fh) == -1) goto err;
		} else {
			/* just write the tag into the non-executable file */
			goto output;
		}

		goto out;
	}

	/* just handle results */
	if (strcmp(tag_type, "result") != 0)
		return;

	if ((tag_id = mxmlElementGetAttr(node, "id")) == NULL)
		goto err;

	snprintf(path, sizeof path, "%s/%s", ctx->dir, tag_id);
 output:
	if ((fd = open(path, O_WRONLY|O_APPEND|O_CREAT|O_NONBLOCK,
	    S_IRUSR|S_IWUSR)) == -1) {
		if (errno == ENXIO) {
			static int block = 0;
			if (block++ < 10) {
				usleep(100000);
				goto output;
			}
			goto out;
		}
		goto err;
	}
	if (write(fd, tag, strlen(tag)) == -1) goto err;
	if (close(fd) == -1) goto err;
 err:
	if (errno != 0)
		perror(__func__);
 out:
	errno = 0;
	mxmlDelete(node);
}

static void
usage(void)
{
	fprintf(stderr, "usage: iqd -d DIR\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	struct context ctx = NULL_CONTEXT;
	int ch;

	while ((ch = getopt(argc, argv, "d:i:")) != -1) {
		switch (ch) {
		case 'i':
			ctx.fd_in = strtol(optarg, NULL, 0);
			break;
		case 'd':
			ctx.dir = strdup(optarg);
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (signal(SIGALRM, sigalarm) == SIG_ERR)
		err(EXIT_FAILURE, "signal");

	/* initialize block parser and set callback function */
	ctx.bxml = bxml_ctx_init(recv_iq, &ctx);

	for (;;) {
		int max_fd = 0;
		ssize_t n;
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(ctx.fd_in, &readfds);
		max_fd = ctx.fd_in;

		/* wait for input */
		if (select(max_fd+1, &readfds, NULL, NULL, NULL) < 0)
			goto err;

		/* check for input from server */
		if (FD_ISSET(ctx.fd_in, &readfds)) {
			char buf[BUFSIZ];
			if ((n = read(ctx.fd_in, buf, BUFSIZ)) < 0) goto err;
			if (n == 0) break;	/* connection closed */
			bxml_add_buf(ctx.bxml, buf, n);
		}
	}
	return EXIT_SUCCESS;
 err:
	if (errno != 0)
		perror(NULL);
	return EXIT_FAILURE;
}
