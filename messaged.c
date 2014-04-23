#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#define NULL_CONTEXT {	\
	STDIN_FILENO,	\
	STDOUT_FILENO,	\
	NULL,		\
	NULL,		\
	NULL,		\
	NULL,		\
	LIST_HEAD_INITIALIZER(listhead) \
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
	char path[_XOPEN_PATH_MAX];
	char *slash = NULL;
	struct contact *c = NULL;

	if ((c = calloc(1, sizeof *c)) == NULL) goto err;
	c->fd = -1;	/* to detect a none vaild file descriptor */
	c->out = -1;	/* to detect a none vaild file descriptor */
	if ((c->name = strdup(jid)) == NULL) goto err;

	/* just handle the bare jabber id without resources */
	if ((slash = strchr(c->name, '/')) != NULL)
		*slash = '\0';

	/* prepare the folder */
	if (snprintf(path, sizeof path, "%s/%s", ctx->dir, c->name) == 0)
		goto err;
	if (mkdir(path, S_IRWXU) == -1 && errno != EEXIST) goto err;

	/* prepare an open the "in" file */
	if (snprintf(path, sizeof path, "%s/%s/in", ctx->dir, c->name) == 0)
		goto err;
	if (mkfifo(path, S_IRUSR|S_IWUSR) == -1 && errno != EEXIST) goto err;
	if ((c->fd = open(path, O_RDONLY|O_NONBLOCK, 0)) == -1) goto err;
	if ((c->out = open(path, O_WRONLY|O_APPEND|O_CREAT, 0)) == -1) goto err;

	fprintf(stderr, "add: %s\n", c->name);
	LIST_INSERT_HEAD(&ctx->roster, c, next);

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
	    "</message>", ctx->jid, to, ctx->id, msg);
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

	if (tree == NULL) tree = mxmlLoadString(NULL, base, MXML_NO_CALLBACK);
	if (tree == NULL) err(EXIT_FAILURE, "no tree found");

	mxmlLoadString(tree, tag, MXML_NO_CALLBACK);

	if (tree->child->next == NULL)
		err(EXIT_FAILURE, "no tag found");

	const char *tag_name = mxmlGetElement(tree->child->next);
	if (strcmp("message", tag_name) != 0) {
		fprintf(stderr, "recv unknown tag\n");
		goto err;
	}

	char *from = mxmlElementGetAttr(tree->child->next, "from");

	fprintf(stderr, "got message from: %s\n", from);

	if (tree->child->next->child == NULL) goto err;

	/* find contact for this message in roster */
	LIST_FOREACH(c, &ctx->roster, next) {
		if (strncmp(c->name, from, strlen(c->name)) == 0) {
			fprintf(stderr, "got message from known sender\n");
			break;
		}
	}

	if (c == LIST_END(&ctx->roster)) {
		fprintf(stderr, "got message from known sender\n");
		c = add_contact(ctx, from);
	}

	for (mxml_node_t *node = tree->child->next->child; node->next != NULL;
	    node = node->next) {
		fprintf(stderr, "node: %p\n", (void*)node);
		if (strcmp(mxmlGetElement(node), "body") != 0) continue;
		fprintf(stderr, "find a body!!\n");
		if (node->child == NULL) continue;
		fprintf(stderr, "message: %s\n", mxmlGetText(node->child, 0));
		break;
	}
 err:
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
	perror(__func__);
	return false;
}

static void
usage(void)
{
	fprintf(stderr, "usage: messaged -j JID -r ROSTERDIR\n");
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

	if (asprintf(&ctx.id, "messaged-%d", getpid()) < 0)
		goto err;

	ctx.bxml = bxml_ctx_init(recv_message, &ctx);

	/* check roster directory */
	struct stat dstat;
	stat(ctx.dir, &dstat);
	build_roster(&ctx);
	//msg_send(&ctx, "test", "younix@jabber.ccc.de");
	char buf[BUFSIZ];
	ssize_t n;
	int max_fd = 0;
	fd_set readfds;
	int sel;

	for (;;) {
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
	return EXIT_FAILURE;
}
