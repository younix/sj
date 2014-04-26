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

#define _XOPEN_SOURCE 700
#define _BSD_SOURCE

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/queue.h>

#include <mxml.h>

#include "bxml/bxml.h"

struct contact {
	char *name;
	int fd;		/* fd to fifo for input */
	int out;	/* fd to output text file */
	LIST_ENTRY(contact) next;
};

struct context {
	int fd_in;
	int fd_out;
	struct bxml_ctx *bxml;
	char *jid;
	char *id;
	char *dir;
	LIST_HEAD(listhead, contact) roster;
};

#define NULL_CONTEXT {		\
	STDIN_FILENO,		\
	STDOUT_FILENO,		\
	NULL,			\
	NULL,			\
	NULL,			\
	".",			\
	LIST_HEAD_INITIALIZER() \
}

void
free_contact(struct contact *c)
{
	if (c == NULL) return;
	if (c->fd != -1) close(c->fd);
	if (c->out != -1) close(c->out);
	free(c->name);
	free(c);
}

static struct contact *
add_contact(struct context *ctx, const char *jid)
{
	char path[PATH_MAX];
	char *slash = NULL;
	struct contact *c = NULL;

	if ((c = calloc(1, sizeof *c)) == NULL) goto err;
	c->out = c->fd = -1;	/* to detect a none vaild file descriptor */
	if ((c->name = strdup(jid)) == NULL) goto err;

	/* just handle the bare jabber id without resources */
	if ((slash = strchr(c->name, '/')) != NULL)
		*slash = '\0';

	/* prepare the folder */
	if (snprintf(path, sizeof path, "%s/%s", ctx->dir, c->name) == 0)
		goto err;
	if (mkdir(path, S_IRWXU) == -1 && errno != EEXIST) goto err;

	/* prepare and open the "in" file */
	if (snprintf(path, sizeof path, "%s/%s/in", ctx->dir, c->name) == 0)
		goto err;
	if (mkfifo(path, S_IRUSR|S_IWUSR) == -1 && errno != EEXIST) goto err;
	if ((c->fd = open(path, O_RDONLY|O_NONBLOCK, 0)) == -1) goto err;

	/* prepare and open the "out" file */
	if (snprintf(path, sizeof path, "%s/%s/out", ctx->dir, c->name) == 0)
		goto err;
	if ((c->out = open(path, O_WRONLY|O_APPEND|O_CREAT, S_IRUSR|S_IWUSR))
	    == -1) goto err;

	LIST_INSERT_HEAD(&ctx->roster, c, next);

	errno = 0;
	return c;
 err:
	free_contact(c);
	if (errno != 0)
		perror(__func__);
	return NULL;
}

static void
msg_send(struct context *ctx, const char *msg, const char *to)
{
	printf(
	    "<message from='%s' to='%s' type='chat' id='%s'>"
		"<active xmlns='http://jabber.org/protocol/chatstates'/>"
		"<body>%s</body>"
	    "</message>\n", ctx->jid, to, ctx->id, msg);
}

static bool
send_message(struct context *ctx, struct contact *con)
{
	char buf[BUFSIZ];
	ssize_t size = 0;

	if ((size = read(con->fd, buf, BUFSIZ-1)) < 0)
		return false;

	buf[size] = '\0';
	msg_send(ctx, buf, con->name);

	return true;
}

static void
recv_message(char *tag, void *data)
{
	struct context *ctx = data;
	struct contact *c = NULL;
	/* HACK: we need this, cause mxml can't parse tags by itself */
	static mxml_node_t *tree = NULL;
	const char *base = "<?xml ?><stream:stream></stream:stream>";
	const char *tag_name = NULL;
	const char *from = NULL;
	char prompt[BUFSIZ];
	time_t timestamp = time(NULL);

	if (tree == NULL) tree = mxmlLoadString(NULL, base, MXML_NO_CALLBACK);
	if (tree == NULL) err(EXIT_FAILURE, "%s: no xml tree found", __func__);
	mxmlLoadString(tree, tag, MXML_NO_CALLBACK);

	if (tree->child->next == NULL) goto err;
	if ((tag_name = mxmlGetElement(tree->child->next)) == NULL) goto err;
	if (strcmp("message", tag_name) != 0)
		goto err;

	if ((from = mxmlElementGetAttr(tree->child->next, "from")) == NULL)
		goto err;

	/* prepare prompt "YYYY-MM-DD HH:MM <FROM> " */
	strftime(prompt, sizeof prompt, "%F %R <", localtime(&timestamp));
	strlcat(prompt, from, sizeof prompt);
	strlcat(prompt, "> ", sizeof prompt);

	/* try to find contact for this message in roster */
	LIST_FOREACH(c, &ctx->roster, next)
		if (strncmp(c->name, from, strlen(c->name)) == 0)
			break;

	/* if message comes from an unknown JID, create a contact */
	if (c == LIST_END(&ctx->roster))
		c = add_contact(ctx, from);

	if (tree->child->next->child == NULL) goto err;
	for (mxml_node_t *node = tree->child->next->child; node != NULL;
	    node = node->next) {
		if ((tag_name = mxmlGetElement(node)) == NULL) continue;
		if (strcmp(tag_name, "body") != 0) continue;
		if (node->child == NULL) continue;

		write(c->out, prompt, strlen(prompt));
		/* concatinate all text peaces */
		for (mxml_node_t *txt = node->child; txt != NULL;
		    txt = mxmlGetNextSibling(txt)) {
			int space = 0;
			const char *t = mxmlGetText(txt, &space);
			if (space == 1)
				write(c->out, " ", 1);
			if (write(c->out, t, strlen(t)) == -1) goto err;
		}
		write(c->out, "\n", 1);
		break;	/* we just look for the first body at the moment */
	}
 err:
	if (errno != 0)
		perror(__func__);
	mxmlDelete(tree->child->next);
}

static bool
build_roster(struct context *ctx)
{
	DIR *dirp;
	struct dirent *dp;

	if (ctx->dir == NULL) return false;
	if ((dirp = opendir(ctx->dir)) == NULL) goto err;

	while ((dp = readdir(dirp)) != NULL) {
		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0 ||
		    dp->d_type != DT_DIR) continue;

		add_contact(ctx, dp->d_name);
	}

	closedir(dirp);
	return true;
 err:
	if (errno != 0)
		perror(__func__);
	return false;
}

static void
usage(void)
{
	fprintf(stderr, "usage: messaged -j JID -d DIR\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	struct context ctx = NULL_CONTEXT;
	struct contact *c = NULL;
	int ch;

	while ((ch = getopt(argc, argv, "j:d:o:i:")) != -1) {
		switch (ch) {
		case 'i':
			ctx.fd_in = strtol(optarg, NULL, 0);
			break;
		case 'o':
			ctx.fd_out = strtol(optarg, NULL, 0);
			break;
		case 'j':
			ctx.jid = strdup(optarg);
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

	if (ctx.jid == NULL)
		usage();

	if (asprintf(&ctx.id, "messaged-%d", getpid()) < 0) goto err;
	ctx.bxml = bxml_ctx_init(recv_message, &ctx);

	/* check roster directory */
	build_roster(&ctx);

	for (;;) {
		int sel, max_fd = 0;
		ssize_t n;
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(ctx.fd_in, &readfds);
		max_fd = ctx.fd_in;

		/* add all fd's from in-files to read list*/
		LIST_FOREACH(c, &ctx.roster, next) {
			FD_SET(c->fd, &readfds);
			if (max_fd < c->fd)
				max_fd = c->fd;
		}

		/* wait for input */
		if ((sel = select(max_fd+1, &readfds, NULL, NULL, NULL)) < 0)
			goto err;

		/* check for input from server */
		if (FD_ISSET(ctx.fd_in, &readfds)) {
			char buf[BUFSIZ];
			if ((n = read(ctx.fd_in, buf, BUFSIZ)) < 0) goto err;
			bxml_add_buf(ctx.bxml, buf, n);
			sel--;
		}

		/* check for input form in-files */
		LIST_FOREACH(c, &ctx.roster, next)
			if (FD_ISSET(c->fd, &readfds))
				send_message(&ctx, c);
	}

 err:
	if (errno != 0)
		perror(NULL);
	return EXIT_FAILURE;
}
