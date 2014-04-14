#include <dirent.h>
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

static bool
build_roster(struct context *ctx)
{
	struct contact *c;
	char path[_XOPEN_PATH_MAX];
	int fd;
	DIR *dirp;
	struct dirent *dp;

	if (ctx->dir == NULL)
		return false;

	if ((dirp = opendir(ctx->dir)) == NULL)
		goto err;

	while ((dp = readdir(dirp)) != NULL) {
		fprintf(stderr, "try: %s\n", dp->d_name);
		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0 ||
		    dp->d_type != DT_DIR) continue;

		snprintf(path, sizeof path, "%s/%s/in", ctx->dir, dp->d_name);
		if (mkfifo(path, S_IRWXU) < 0 && errno != EEXIST) goto err;
		if ((fd = open(path, O_RDONLY|O_NONBLOCK, 0)) < 0) goto err;
		if ((c = calloc(1, sizeof *c)) == NULL) goto err;
		fprintf(stderr, "sizeof: %zd\n", sizeof *c);

		c->name = strdup(dp->d_name);
		c->fd = fd;
		fprintf(stderr, "add: %s\n", c->name);
		c = c->next;
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
