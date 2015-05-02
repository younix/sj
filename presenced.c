/*
 * Copyright (c) 2015 Jan Klemkow <j.klemkow@wemelug.de>
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

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/queue.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>

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

static void
recv_presence(char *tag, void *data)
{
	struct context *ctx = data;
	/* HACK: we need this, cause mxml can't parse tags by itself */
	static mxml_node_t *tree = NULL;
	static mxml_node_t *node = NULL;
	const char *base = "<?xml ?><stream:stream></stream:stream>";
	const char *tag_name = NULL;
	const char *from = NULL;
	char path[PATH_MAX];
	int fd;

	if (tree == NULL) tree = mxmlLoadString(NULL, base, MXML_NO_CALLBACK);
	if (tree == NULL) err(EXIT_FAILURE, "%s: no xml tree found", __func__);
	mxmlLoadString(tree, tag, MXML_NO_CALLBACK);

	if (tree->child->next == NULL) goto err;
	node = tree->child->next;
	if ((tag_name = mxmlGetElement(node)) == NULL) goto err;
	if (strcmp("presence", tag_name) != 0)
		goto err;

	if ((from = mxmlElementGetAttr(tree->child->next, "from")) == NULL)
		goto err;

	if (mkdir(ctx->dir, S_IRUSR|S_IWUSR|S_IXUSR) == -1)
		if (errno != EEXIST) err(EXIT_FAILURE, "mkdir");

	snprintf(path, sizeof path, "%s/%s", ctx->dir, from);
	if (mkdir(path, S_IRUSR|S_IWUSR|S_IXUSR) == -1)
		if (errno != EEXIST) err(EXIT_FAILURE, "mkdir");

	snprintf(path, sizeof path, "%s/%s/status", ctx->dir, from);
	if ((fd = open(path, O_WRONLY|O_TRUNC|O_CREAT, S_IRUSR|S_IWUSR)) == -1)
		goto err;
	/* write text of status-tag into this file */
	if (write(fd, tag, strlen(tag)) == -1) goto err;
	if (close(fd) == -1) goto err;
 err:
	if (errno != 0)
		perror(__func__);
	mxmlDelete(tree->child->next);
}

static void
usage(void)
{
	fprintf(stderr, "usage: presenced -d DIR\n");
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

	/* initialize block parser and set callback function */
	ctx.bxml = bxml_ctx_init(recv_presence, &ctx);

	for (;;) {
		int max_fd = 0;
		ssize_t n;
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(ctx.fd_in, &readfds);
		max_fd = ctx.fd_in;

		/* wait for input */
		if (select(max_fd+1, &readfds, NULL, NULL, NULL) < 0)
			err(EXIT_FAILURE, "select");

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
