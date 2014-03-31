/*
 * Copyright (c) 2013-2014 Jan Klemkow <j.klemkow@wemelug.de>
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

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <mxml.h>

#include "sasl/sasl.h"
#include "bxml/bxml.h"

/* XMPP session states */
enum xmpp_state {OPEN, AUTH, BIND_OUT, BIND, SESSION};

struct context {
	/* socket to xmpp server */
	int sock;

	/* xml parser */
	struct bxml_ctx *bxml;

	/* connection information */
	char *user;
	char *pass;
	char *server;
	char *host;
	char *port;
	char *resource;

	/* file system frontend */
	char *dir;
	int fd_out;
	int fd_in;

	/* state of the xmpp session */
	enum xmpp_state state;
};

#define NULL_CONTEXT {				\
	0,	/* int sock; */			\
	NULL,	/* struct bxml_ctx; */		\
	NULL,	/* char *user; */		\
	NULL,	/* char *pass; */		\
	NULL,	/* char *server; */		\
	NULL,	/* char *host; */		\
	NULL,	/* char *port; */		\
	NULL,	/* char *resource; */		\
	NULL,	/* char *dir; */		\
	0,	/* int fd_out; */		\
	0,	/* int fd_in; */		\
	OPEN	/* int state; */		\
}

static void
xmpp_ping(struct context *ctx)
{
	char msg[BUFSIZ];
	int size = snprintf(msg, sizeof msg,
	    "<iq from='%s@%s/%s' to='%s' id='ping-sj' type='get'>"
		"<ping xmlns='urn:xmpp:ping'/>"
	    "</iq>", ctx->user, ctx->server, ctx->resource, ctx->server);

	if ((size = send(ctx->sock, msg, size, 0)) < 0)
		perror(__func__);
}

static void
xmpp_session(struct context *ctx)
{
	char msg[BUFSIZ];
	int size = snprintf(msg, sizeof msg,
	    "<iq to='%s' type='set' id='sess_1'>"
		"<session xmlns='urn:ietf:params:xml:ns:xmpp-session'/>"
	    "</iq>" , ctx->server);

	if ((size = send(ctx->sock, msg, size, 0)) < 0)
		perror(__func__);
}

static void
xmpp_bind(struct context *ctx)
{
	char *msg = NULL;
	ssize_t size = asprintf(&msg,
	    "<iq type='set' id='bind_2'>"
		"<bind xmlns='urn:ietf:params:xml:ns:xmpp-bind'>"
		    "<resource>%s</resource>"
		"</bind>"
	    "</iq>", ctx->resource);

	if ((size = send(ctx->sock, msg, strlen(msg), 0)) < 0)
		perror(__func__);

	ctx->state = BIND_OUT;

	free(msg);
}

static void
xmpp_auth_type(struct context *ctx, char *type)
{
	char msg[BUFSIZ];
	int size = snprintf(msg, sizeof msg,
		"<auth xmlns='urn:ietf:params:xml:ns:xmpp-sasl'"
		" mechanism='%s'/>", type);

	if ((size = send(ctx->sock, msg, strlen(msg), 0)) < 0)
		perror(__func__);
}

static void
xmpp_auth(struct context *ctx)
{
	char msg[BUFSIZ];
	char *authstr = sasl_plain(ctx->user, ctx->pass);
	int size = snprintf(msg, sizeof msg,
		"<auth xmlns='urn:ietf:params:xml:ns:xmpp-sasl'"
		" mechanism='PLAIN'>%s</auth>", authstr);

	if (send(ctx->sock, msg, size, 0) < 0)
		perror(__func__);;
	free(authstr);
}

static void
xmpp_init(struct context *ctx)
{
	char msg[BUFSIZ];
	int size = snprintf(msg, sizeof msg,
	    "<?xml version='1.0'?>"
	    "<stream:stream "
		"from='%s@%s' "
		"to='%s' "
		"version='1.0' "
		"xml:lang='en' "
		"xmlns='jabber:client' "
		"xmlns:stream='http://etherx.jabber.org/streams'>\n",
	    ctx->user, ctx->server, ctx->server);

	if (send(ctx->sock, msg, size, 0) < 0)
		perror(__func__);
}

static void
xmpp_close(int sock)
{
	if (send(sock, "</stream:stream>", 16, 0) < 0)
		perror(__func__);
}

static void
xmpp_message(struct context *ctx, char *to, char *text) {
	char msg[BUFSIZ];
	int size = snprintf(msg, sizeof msg,
	    "<message "
	    "    id='ktx72v49'"
	    "    to='%s'"
	    "    type='chat'"
	    "    xml:lang='en'>"
	    "<body>", to);

	if (send(ctx->sock, msg, size, 0) < 0) perror(__func__); 
	if (send(ctx->sock, text, strlen(text), 0) < 0) perror(__func__);
	if (send(ctx->sock, "</body></message>", 17, 0) < 0) perror(__func__);
}

/*
 * This callback function is called from bxml-lib if a whole xml-tag from
 * the xmpp-server is recieved.
 */
static void
server_tag(char *tag, void *data)
{
	struct context *ctx = data;
	/* HACK: we need this, cause mxml can't parse tags by itself */
	static mxml_node_t *tree = NULL;
	const char *base = "<?xml ?><stream:stream></stream:stream>";

	fprintf(stderr, "SERVER: %s\n\n", tag);

	if (tree == NULL) tree = mxmlLoadString(NULL, base, MXML_NO_CALLBACK);
	if (tree == NULL) err(EXIT_FAILURE, "no tree found");

	mxmlLoadString(tree, tag, MXML_NO_CALLBACK);

	if (tree->child->next == NULL)
		err(EXIT_FAILURE, "no tree found");

 	const char *tag_name = mxmlGetElement(tree->child->next);

	/* authentication and binding */
	if (strcmp("stream:features", tag_name) == 0) {
		if (ctx->state == OPEN)
			xmpp_auth(ctx);
		else if (ctx->state == AUTH)
			xmpp_bind(ctx);
		else
			printf("state: %d\n", ctx->state);
	}

	/* binding completed */
	if (strcmp("iq", tag_name) == 0 &&
	    strcmp("bind_2", mxmlElementGetAttr(tree->child->next, "id")) == 0&&
	    ctx->state == BIND_OUT) {
		ctx->state = BIND;
		xmpp_session(ctx);
	}

	/* session completed */
	if (strcmp("iq", tag_name) == 0 &&
	    strcmp("sess_1", mxmlElementGetAttr(tree->child->next, "id")) == 0&&
	    ctx->state == BIND &&
	    tree->child->next->child != NULL &&
	    strcmp("urn:ietf:params:xml:ns:xmpp-session",
	    mxmlElementGetAttr(tree->child->next->child, "xmlns")) == 0) {
		ctx->state = SESSION;
	}

	/* SASL authentification successful */
	if (strcmp("success", tag_name) == 0 &&
	    strcmp("urn:ietf:params:xml:ns:xmpp-sasl",
		   mxmlElementGetAttr(tree->child->next, "xmlns")) == 0) {
		ctx->state = AUTH;
		ctx->bxml->depth = 0; /* The stream will reset after success */
		xmpp_init(ctx);
	}

	mxmlDelete(tree->child->next);
}

/*
 * This function creates and opens the files used as front end.
 */
static void
init_dir(struct context *ctx)
{
	if (mkdir(ctx->dir, S_IRWXU) < 0)
		if (errno != EEXIST)
			perror(__func__);

	char *file = NULL;
	asprintf(&file, "%s/out", ctx->dir);
	if ((ctx->fd_out = open(file, O_WRONLY|O_CREAT|O_TRUNC,
	    S_IRUSR|S_IWUSR)) < 0) {
		fprintf(stderr, "open_error:");
		perror(__func__);
	}
	free(file);

	asprintf(&file, "%s/in", ctx->dir);
	if (mkfifo(file, S_IRUSR|S_IWUSR) < 0)
		if (errno != EEXIST)
			perror(__func__);

	if ((ctx->fd_in = open(file, O_RDONLY|O_NONBLOCK, 0)) < 0)
		perror(__func__);
	free(file);
}

static void
usage(void)
{
	fprintf(stderr, "sj OPTIONS\n"
		"OPTIONS:\n"
		"\t-U <user>"
		"\t-H <host>"
		"\t-s <server>"
		"\t-p <port>"
		"\t-r <resource>"
		"\t-d <directory>\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char**argv)
{
	struct addrinfo hints, *addrinfo = NULL, *addrinfo0 = NULL;
	int ch;

	/* struct with all context informations */
	struct context ctx = NULL_CONTEXT;
	ctx.sock = 0;
	ctx.user = NULL;
	ctx.pass = NULL;
	ctx.server = NULL;
	ctx.port = "5222";
	ctx.dir = "xmpp";
	ctx.resource = "sj";
	ctx.state = OPEN;	/* set inital state of the connection */

	while ((ch = getopt(argc, argv, "d:s:H:p:U:P:r:")) != -1) {
		switch (ch) {
		case 'd':
			ctx.dir = strdup(optarg);
			break;
		case 's':
			ctx.server = strdup(optarg);
			break;
		case 'H':
			ctx.host = strdup(optarg);
			break;
		case 'p':
			ctx.port = strdup(optarg);
			break;
		case 'U':
			ctx.user = strdup(optarg);
			break;
		case 'P':
			ctx.pass = strdup(optarg);
			break;
		case 'r':
			ctx.resource = strdup(optarg);
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (ctx.host == NULL)
		ctx.host = ctx.server;

	/* prepare network connection to xmpp server */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	getaddrinfo(ctx.host, ctx.port, &hints, &addrinfo0);

	for (addrinfo = addrinfo0; addrinfo; addrinfo = addrinfo->ai_next) {
		if ((ctx.sock =
		    socket(addrinfo->ai_family, addrinfo->ai_socktype, 0)) < 0)
			continue;

		if (connect(ctx.sock, addrinfo->ai_addr,
		    addrinfo->ai_addrlen) < 0) {
			close(ctx.sock);
			continue;
		}

		break;
	}
	freeaddrinfo(addrinfo0);
	if (ctx.sock < 0) goto err;	/* no network connection */

	/* init block xml parser */
	ctx.bxml = bxml_ctx_init(server_tag, &ctx);
	ctx.bxml->block_depth = 1;

	init_dir(&ctx);
	xmpp_init(&ctx);

	int max_fd = ctx.sock;
	char buf[BUFSIZ];
	ssize_t n = 0;
	fd_set readfds;

	/* timeinterval for keep alive pings */
	struct timeval tv = {10, 0};

	for (;;) {
		FD_ZERO(&readfds);
		FD_SET(ctx.sock, &readfds);
		FD_SET(ctx.fd_in, &readfds);

		max_fd = ctx.sock;
		if (max_fd < ctx.fd_in)
			max_fd = ctx.fd_in;

		int sel = select(max_fd+1, &readfds, NULL, NULL, &tv);

		/* data from xmpp server */
		if (FD_ISSET(ctx.sock, &readfds)) {
			if ((n = recv(ctx.sock, buf, BUFSIZ, 0)) < 0) goto err;
			bxml_add_buf(ctx.bxml, buf, n);
		} else if (FD_ISSET(ctx.fd_in, &readfds) &&
			   ctx.state == SESSION) {
			while ((n = read(ctx.fd_in, buf, BUFSIZ)) > 0) {
				if (send(ctx.sock, buf, n, 0) < 0) goto err;
			}
		} else if (sel == 0 && ctx.state == SESSION) {
			xmpp_ping(&ctx);
		} else { /* data from FIFO */
			printf("other event!\n");
		}
	}

 err:
	perror(__func__);
	return EXIT_FAILURE;
}
