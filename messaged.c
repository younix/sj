/*
 * Copyright (c) 2014-2015 Jan Klemkow <j.klemkow@wemelug.de>
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
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef USE_LIBBSD
#	include <bsd/string.h>
#else
#	include <string.h>
#endif

#include <time.h>
#include <unistd.h>

#include <mxml.h>

#include "bxml/bxml.h"

struct contact {
	char *name;
	char in_path[PATH_MAX];
	int fd;		/* fd to fifo for input */
	int out;	/* fd to output text file */
	LIST_ENTRY(contact) next;
};

struct context {
	int fd_in;
	char *out_file;
	struct bxml_ctx *bxml;
	char *jid;
	char *id;
	char *dir;
	LIST_HEAD(listhead, contact) roster;
};

#define NULL_CONTEXT {		\
	STDIN_FILENO,		\
	NULL,			\
	NULL,			\
	NULL,			\
	NULL,			\
	".",			\
	LIST_HEAD_INITIALIZER() \
}

struct context *global_ctx;

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
	if (snprintf(c->in_path, sizeof c->in_path, "%s/%s/in",
	    ctx->dir, c->name) == 0)
		goto err;
	if (mkfifo(c->in_path, S_IRUSR|S_IWUSR) == -1 && errno != EEXIST)
		goto err;
	if ((c->fd = open(c->in_path, O_RDONLY|O_NONBLOCK, 0)) == -1) goto err;

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

static char *
prepare_prompt(char *prompt, size_t size, const char *user)
{
	time_t timestamp = time(NULL);

	if (prompt == NULL || size == 0)
		return NULL;

	/* prepare prompt "YYYY-MM-DD HH:MM <FROM> " */
	strftime(prompt, size, "%F %R <", localtime(&timestamp));
	strlcat(prompt, user, size);
	strlcat(prompt, "> ", size);

	return prompt;
}

static void
msg_send(struct context *ctx, const char *msg, const char *to)
{
	FILE *fh = NULL;

	if ((fh = fopen(ctx->out_file, "w")) == NULL) goto err;
	fprintf(fh,
	    "<message from='%s' to='%s' type='chat' id='%s'>"
		"<active xmlns='http://jabber.org/protocol/chatstates'/>"
		"<body>%s</body>"
	    "</message>\n", ctx->jid, to, ctx->id, msg);
	fclose(fh);
 err:
	if (errno != 0)
		perror(__func__);
}

char *
escape_tag(const char *string)
{
	char *new;
	char ch[2] = {'\0', '\0'};
	size_t length = 1;	/* One byte for the null character */

	/* allocate the amount of space that we'll need */
	for (size_t i = 0; string[i]; i++) {
		/* For every <, we need 3 more bytes */
		if (string[i] == '<') length += 4;
		/* For every &, we need 4 more bytes */
		else if (string[i] == '&') length += 5;
		else length++;
	}

	if ((new = calloc(length, sizeof *new)) == NULL)
		err(EXIT_FAILURE, "malloc");

	for (; string[0]; string++) {
		/* TODO: We should do official XML escaping here. */
		switch (string[0]) {
		case '<':
			strlcat(new, "&lt;", length);
			break;
		case '&':
			strlcat(new, "&lt;", length);
			break;
		default:
			ch[0] = string[0];
			strlcat(new, ch, length);
		}
	}

	return new;
}

static bool
send_message(struct context *ctx, struct contact *con)
{
	char prompt[BUFSIZ];
	char buf[BUFSIZ];
	char *escaped = NULL;
	ssize_t size = 0;

	if ((size = read(con->fd, buf, sizeof(buf) - 1)) < 0)
		return false;

	if (close(con->fd) == -1) return false;
	if ((con->fd = open(con->in_path, O_RDONLY|O_NONBLOCK, 0)) == -1)
		return false;

	if (size == 0)
		return true;

	buf[size] = '\0';
	/* Trim trailing control characters. */
	while (iscntrl(buf[size - 1])) {
		buf[size - 1] = '\0';
		size--;
	}
	/* These characters must be escaped. */
	if ((ssize_t)strcspn(buf, "<&") != size) {
		/* Get a new string. */
		escaped = escape_tag(buf);
	}
	msg_send(ctx, escaped ? escaped : buf, con->name);
	free(escaped);

	/* Write message to the out file, letting the user see its own messages. */
	prepare_prompt(prompt, sizeof prompt, ctx->jid);
	if (write(con->out, prompt, strlen(prompt)) == -1) return false;
	if (write(con->out, buf, size) == -1) return false;
	if (write(con->out, "\n", 1) == -1) return false;

	return true;
}

static void
recv_message(char *tag, void *data)
{
	struct context *ctx = data;
	struct contact *c = NULL;
	/* HACK: we need this, cause mxml can't parse tags by itself */
	static mxml_node_t *tree = NULL;
	mxml_node_t *node = NULL;
	mxml_node_t *body = NULL;
	const char *base = "<?xml ?>";
	const char *tag_name = NULL;
	const char *from = NULL;
	char prompt[BUFSIZ];

	if (tree == NULL) tree = mxmlLoadString(NULL, base, MXML_NO_CALLBACK);
	if (tree == NULL) err(EXIT_FAILURE, "unable to load xml base");

	mxmlLoadString(tree, tag, MXML_NO_CALLBACK);
	if ((node = mxmlGetFirstChild(tree)) == NULL)
		goto err;

	if ((tag_name = mxmlGetElement(node)) == NULL) goto err;
	if (strcmp("message", tag_name) != 0) goto err;
	if ((from = mxmlElementGetAttr(node, "from")) == NULL) goto err;

	/* try to find contact for this message in roster */
	LIST_FOREACH(c, &ctx->roster, next)
		if (strncmp(c->name, from, strlen(c->name)) == 0)
			break;

	/* if message comes from an unknown JID, create a contact */
	if (c == NULL)
		c = add_contact(ctx, from);

	body = mxmlFindElement(tree, tree, "body", NULL, NULL,
	    MXML_DESCEND);
	if (body == NULL)
		goto err;

	prepare_prompt(prompt, sizeof prompt, from);
	write(c->out, prompt, strlen(prompt));

	/* concatinate all text peaces */
	for (mxml_node_t *txt = mxmlGetFirstChild(body); txt != NULL;
	    txt = mxmlGetNextSibling(txt)) {
		int space = 0;
		const char *t = mxmlGetText(txt, &space);
		if (space == 1)
			write(c->out, " ", 1);
		if (write(c->out, t, strlen(t)) == -1) goto err;
	}
	write(c->out, "\n", 1);
 err:
	if (errno != 0)
		perror(__func__);
	mxmlDelete(node);
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
		    strchr(dp->d_name, '@') == NULL ||
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
signal_handler(int sig)
{
	if (sig == SIGHUP) {
		build_roster(global_ctx);
	}
}

static void
usage(void)
{
	fprintf(stderr, "usage: messaged -j jid -d dir\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	struct context ctx = NULL_CONTEXT;
	int ch;

	while ((ch = getopt(argc, argv, "j:d:o:i:")) != -1) {
		switch (ch) {
		case 'i':
			ctx.fd_in = strtol(optarg, NULL, 0);
			break;
		case 'o':
			ctx.out_file = strdup(optarg);
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
	if (ctx.out_file == NULL)
		if (asprintf(&ctx.out_file, "%s/in", ctx.dir) < 0) goto err;
	if (asprintf(&ctx.id, "messaged-%d", getpid()) < 0) goto err;
	ctx.bxml = bxml_ctx_init(recv_message, &ctx);

	/* check roster directory */
	build_roster(&ctx);
	global_ctx = &ctx;
	signal(SIGHUP, signal_handler);

	for (;;) {
		struct contact *c = NULL;
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
		if ((sel = select(max_fd+1, &readfds, NULL, NULL, NULL)) == -1
		    && errno != EINTR)
			err(EXIT_FAILURE, "select");

		/* check for input from server */
		if (FD_ISSET(ctx.fd_in, &readfds)) {
			char buf[BUFSIZ];
			if ((n = read(ctx.fd_in, buf, BUFSIZ)) < 0) goto err;
			if (n == 0) break;	/* connection closed */
			bxml_add_buf(ctx.bxml, buf, n);
			sel--;
		}

		/* check for input form in-files */
		LIST_FOREACH(c, &ctx.roster, next)
			if (FD_ISSET(c->fd, &readfds))
				if (send_message(&ctx, c) == false) goto err;
	}
	return EXIT_SUCCESS;
 err:
	if (errno != 0)
		perror(NULL);
	return EXIT_FAILURE;
}
