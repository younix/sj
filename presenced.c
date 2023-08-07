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

#include <sys/queue.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>

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

#include <mxml.h>

#include "bxml/bxml.h"

struct contact {
	char *jid;		/* buddies jabber ID */
	char path[PATH_MAX];	/* path to buddy specific status file */
	char *mystatus;		/* buddy specific status message */
	LIST_ENTRY(contact) next;
};

struct context {
	int fd_in;
	struct bxml_ctx *bxml;
	char *dir;
	char out_file[PATH_MAX];
	LIST_HEAD(listhead, contact) roster;
};

#define NULL_CONTEXT {		\
	STDIN_FILENO,		\
	NULL,			\
	".",			\
	{0},			\
	LIST_HEAD_INITIALIZER()	\
}

static void
send_presence(const struct context *ctx, const struct contact *c)
{
	FILE *fh = NULL;

	if (ctx == NULL || c == NULL)
		return;

	if (c->mystatus == NULL)
		return;

	if ((fh = fopen(ctx->out_file, "w")) == NULL)
		goto err;

	if (fprintf(fh,
		"<presence to='%s'>"
			"<status>%s</status>"
			"<priority>1</priority>"
		"</presence>", c->jid, c->mystatus) == -1)
		goto err;

	if (fclose(fh) == EOF)
		goto err;
	return;
 err:
	if (errno != 0)
		perror(__func__);
}

static void
check_contact(struct context *ctx, struct contact *c)
{
	FILE *fh;
	char buf[BUFSIZ];

	if (c == NULL)
		return;

	if ((fh = fopen(c->path, "r")) == NULL) {
		if (errno == ENOENT) {
			errno = 0;
			if (c->mystatus != NULL) {
				free(c->mystatus);
				c->mystatus = NULL;
				send_presence(ctx, c);
			}
			return;
		}
		goto err;
	}

	if (fgets(buf, sizeof buf, fh) == NULL) {
		/* distinguish between empty file and error */
		if (feof(fh))
			buf[0] = '\0';
		else
			goto err;
	}

	if (c->mystatus != NULL) {
		if (strncmp(buf, c->mystatus, sizeof buf) != 0) {
			free(c->mystatus);
			c->mystatus = strdup(buf);
			send_presence(ctx, c);
		}
	} else {
		c->mystatus = strdup(buf);
		send_presence(ctx, c);
	}

	if (fclose(fh) == EOF)
		goto err;
	return;
 err:
	if (errno != 0)
		perror(__func__);
}

static void
free_contact(struct contact *c)
{
	if (c == NULL) return;
	free(c->jid);
	free(c);
}

static struct contact *
add_contact(struct context *ctx, char *jid)
{
	char *slash = NULL;
	struct contact *c = NULL;

	/* just handle the bare jabber id without resources */
	if ((slash = strchr(jid, '/')) != NULL)
		*slash = '\0';

	/* return contact if it exists already */
	LIST_FOREACH(c, &ctx->roster, next)
		if (strcmp(c->jid, jid) == 0)
			return c;

	/* create a new contact */
	if ((c = calloc(1, sizeof *c)) == NULL) goto err;
	if ((c->jid = strdup(jid)) == NULL) goto err;

	/* prepare path of "mystatus" file */
	if (snprintf(c->path, sizeof c->path, "%s/%s/mystatus",
	    ctx->dir, c->jid) == 0)
		goto err;

	LIST_INSERT_HEAD(&ctx->roster, c, next);

	return c;
 err:
	free_contact(c);
	if (errno != 0)
		perror(__func__);
	return NULL;
}

static void
check_roster(struct context *ctx)
{
	DIR *dirp;
	struct dirent *dp;
	struct contact *c;

	if (ctx->dir == NULL) return;
	if ((dirp = opendir(ctx->dir)) == NULL) goto err;

	while ((dp = readdir(dirp)) != NULL) {
		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0 ||
		    dp->d_type != DT_DIR) continue;

		if ((c = add_contact(ctx, dp->d_name)) != NULL)
			check_contact(ctx, c);
	}

	closedir(dirp);
	return;
 err:
	if (errno != 0)
		perror(__func__);
	return;
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
	char *slash = NULL;
	char path[PATH_MAX];
	int fd;
	bool is_online = false;

	if (tree == NULL) tree = mxmlLoadString(NULL, base, MXML_NO_CALLBACK);
	if (tree == NULL) err(EXIT_FAILURE, "%s: no xml tree found", __func__);
	mxmlLoadString(tree, tag, MXML_NO_CALLBACK);

	if ((node = mxmlGetNextSibling(mxmlGetFirstChild(tree))) == NULL) goto err;
	if ((tag_name = mxmlGetElement(node)) == NULL) goto err;
	if (strcmp("presence", tag_name) != 0)
		goto err;

	if ((from = mxmlElementGetAttr(node, "from")) == NULL)
		goto err;

	/* The presence of the 'type' attribute indicates offline.
	   The lack of it indicates online. */
	if (mxmlElementGetAttr(node, "type"))
		is_online = false;
	else
		is_online = true;

	/* cut off resourcepart from jabber ID */
	if ((slash = strchr(from, '/')) != NULL)
		*slash = '\0';

	if (mkdir(ctx->dir, S_IRUSR|S_IWUSR|S_IXUSR) == -1) {
		if (errno != EEXIST) err(EXIT_FAILURE, "mkdir");
		errno = 0;
	}

	snprintf(path, sizeof path, "%s/%s", ctx->dir, from);
	if (mkdir(path, S_IRUSR|S_IWUSR|S_IXUSR) == -1) {
		if (errno != EEXIST) err(EXIT_FAILURE, "mkdir");
		errno = 0;
	}

	snprintf(path, sizeof path, "%s/%s/status", ctx->dir, from);

	if ((fd = open(path, O_WRONLY|O_TRUNC|O_CREAT, S_IRUSR|S_IWUSR)) == -1)
		goto err;

	if (is_online) {
		mxml_node_t *show = NULL;
		const char *status;
		if ((show = mxmlFindElement(node, tree, "show", NULL, NULL,
		                           MXML_DESCEND_FIRST)) != NULL)
			status = mxmlGetText(show, NULL);
		else
			status = "online";
		if (write(fd, status, strlen(status)) == -1) goto err;
	}
	/* write nothing; make fd an empty file */
	if (close(fd) == -1) goto err;

 err:
	if (errno != 0)
		perror(__func__);
	mxmlDelete(node);
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

	snprintf(ctx.out_file, sizeof ctx.out_file, "%s/in", ctx.dir);

	check_roster(&ctx);

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
