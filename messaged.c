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

#include <mxml.h>

#include "bxml/bxml.h"

struct contact {
	char *name;
	int fd;
	struct contact *next;
};

struct context {
	int fd_in;
	int fd_out;
	struct bxml_ctx *bxml;
	char *jid;
	char *id;
	char *dir;
	struct contact *roster;
};

#define NULL_CONTEXT {	\
	STDIN_FILENO,	\
	STDOUT_FILENO,	\
	NULL,		\
	NULL,		\
	NULL,		\
	NULL,		\
	NULL		\
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
	/* HACK: we need this, cause mxml can't parse tags by itself */
	static mxml_node_t *tree = NULL;
	const char *base = "<?xml ?><stream:stream></stream:stream>";

	if (tree == NULL) tree = mxmlLoadString(NULL, base, MXML_NO_CALLBACK);
	if (tree == NULL) err(EXIT_FAILURE, "no tree found");

	const char *tag_name = mxmlGetElement(tree->child->next);
	/* authentication and binding */
	if (strcmp("message", tag_name) != 0) {
		fprintf(stderr, "recv unknown tag\n");
		goto err;
	}

	char *from = mxmlElementGetAttr(tree->child->next, "from");
	fprintf(stderr, "got message from: %s\n", from);


 err:
	mxmlDelete(tree->child->next);
}

static bool
build_roster(struct context *ctx)
{
	struct contact *c = NULL;
	struct contact *c_old = NULL;
	char path[_XOPEN_PATH_MAX];
	int fd;
	DIR *dirp;
	struct dirent *dp;

	if (ctx->dir == NULL)
		return false;

	if ((dirp = opendir(ctx->dir)) == NULL)
		goto err;

	while ((dp = readdir(dirp)) != NULL) {
		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0 ||
		    dp->d_type != DT_DIR) continue;

		/* create and open in-files */
		snprintf(path, sizeof path, "%s/%s/in", ctx->dir, dp->d_name);
		if (mkfifo(path, S_IRWXU) < 0 && errno != EEXIST) goto err;
		if ((fd = open(path, O_RDONLY|O_NONBLOCK, 0)) < 0) goto err;

		/* add fh and file name to current contact structure */
		if ((c = calloc(1, sizeof *c)) == NULL) goto err;
		c->name = strdup(dp->d_name);
		c->fd = fd;
		fprintf(stderr, "add: %s\n", c->name);
		c->next = c_old;
		c_old = c;
	}

	closedir(dirp);
	ctx->roster = c; /* save the last one */
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
		for (struct contact *c = ctx.roster; c->next != NULL;
		    c = c->next) {
			FD_SET(c->fd, &readfds);
			if (max_fd < c->fd)
				max_fd = c->fd;
		}

		/* wait for input */
		if ((sel = select(max_fd+1, &readfds, NULL, NULL, NULL)) < 0)
			goto err;

		/* check for input from server */
		if (FD_ISSET(ctx.fd_in, &readfds)) {
			fprintf(stderr, "got data from server!\n");
			if ((n = read(ctx.fd_in, buf, BUFSIZ)) < 0) goto err;
			bxml_add_buf(ctx.bxml, buf, n);
			sel--;
		}

		/* check for input date form in-files */
		for (struct contact *c = ctx.roster;
		    c->next != NULL && sel > 0; c = c->next, sel--)
			if (FD_ISSET(c->fd, &readfds))
				send_message(&ctx, c);
	}

 err:
	return EXIT_FAILURE;
}
