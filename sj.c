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

#include <expat.h>

#include "sasl/sasl.h"

/* we parsing the xml-stream at this tag-level-depth */
#define TAG_LEVEL 2

struct context {
	/* socket to xmpp server */
	int sock;

	/* xml parser */
	XML_Parser parser;
	int depth;
	int start_tag;
	int decl_done;

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
	int state;
};

#define NULL_CONTEXT {				\
	0,	/* int sock; */			\
	NULL,	/* XML_Parser parser; */	\
	0,	/* int depth; */		\
	0,	/* int start_tag; */		\
	0,	/* int decl_done; */		\
	NULL,	/* char *user; */		\
	NULL,	/* char *pass; */		\
	NULL,	/* char *server; */		\
	NULL,	/* char *host; */		\
	NULL,	/* char *port; */		\
	NULL,	/* char *resource; */		\
	NULL,	/* char *dir; */		\
	0,	/* int fd_out; */		\
	0,	/* int fd_in; */		\
	0	/* int state; */		\
}

/* XMPP session states */
enum state {OPEN = 0, AUTH, BIND_OUT, BIND, SESSION};

void init_parser(struct context *ctx);

void
xmpp_ping(struct context *ctx)
{
	char *msg = NULL;
	ssize_t size = asprintf(&msg,
	    "<iq from='%s@%s' to='%s' id='ping' type='get'>"
		"<ping xmlns='urn:xmpp:ping'/>"
	    "</iq>", ctx->user, ctx->server, ctx->server);

	if ((size = send(ctx->sock, msg, strlen(msg), 0)) < 0)
		perror(__func__);

	free(msg);
}

void
xmpp_session(struct context *ctx)
{
	char *msg = NULL;
	ssize_t size = asprintf(&msg,
	    "<iq to='%s' "
		"type='set' "
		"id='sess_1'>"
		"<session xmlns='urn:ietf:params:xml:ns:xmpp-session'/>"
	    "</iq>" , ctx->server);

	if ((size = send(ctx->sock, msg, strlen(msg), 0)) < 0)
		perror(__func__);

	free(msg);
}

void
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

void
xmpp_auth_type(struct context *ctx, char *type)
{
	char *msg = NULL;
	ssize_t size = asprintf(&msg,
		"<auth xmlns='urn:ietf:params:xml:ns:xmpp-sasl'"
		" mechanism='%s'/>", type);

	if ((size = send(ctx->sock, msg, strlen(msg), 0)) < 0)
		perror(__func__);

	free(msg);
}

void
xmpp_auth(struct context *ctx)
{
	char *authstr = sasl_plain(ctx->user, ctx->pass);

	char *msg = NULL;
	ssize_t size = asprintf(&msg,
		"<auth xmlns='urn:ietf:params:xml:ns:xmpp-sasl'"
		" mechanism='PLAIN'>%s</auth>", authstr);

	size = send(ctx->sock, msg, strlen(msg), 0);

	free(authstr);
	free(msg);
}

void
xmpp_init(struct context *ctx)
{
	char *msg = NULL;
	ssize_t size = asprintf(&msg,
	    "<?xml version='1.0'?>"
	    "<stream:stream "
		"from='%s@%s' "
		"to='%s' "
		"version='1.0' "
		"xml:lang='en' "
		"xmlns='jabber:client' "
		"xmlns:stream='http://etherx.jabber.org/streams'>\n",
	    ctx->user, ctx->server, ctx->server);

	printf("size: %zd\nmsg:%s", size, msg);

	if (send(ctx->sock, msg, strlen(msg), 0) < 0)
		perror(__func__);

	free(msg);
}

void
xmpp_close(int sock)
{
	char *msg = "</stream:stream>";
	send(sock, msg, strlen(msg), 0);
}

void
xmpp_message(struct context *ctx, char *to, char *text) {
	char *msg = NULL;
	ssize_t size = asprintf(&msg,
	    "<message "
	    "    id='ktx72v49'"
	    "    to='%s'"
	    "    type='chat'"
	    "    xml:lang='en'>"
	    "<body>%s</body>"
	    "</message>",
	    to, text);

	size = send(ctx->sock, msg, strlen(msg), 0);

	free(msg);
}

void
start_tag(void *data, const char *el, const char **attr)
{
	struct context *ctx = data;
	int offset = 0, size = 0;
	char *buf = NULL;
	ctx->depth++;

	buf = XML_GetInputContext(ctx->parser, &offset, &size);
	printf("START: ->%.*s<-\n\n\n", size, (buf + offset));

	if (ctx->depth == TAG_LEVEL) {
		ctx->start_tag = offset;
	}
}

void
end_tag(void *data, const char *name)
{
	struct context *ctx = data;
	int offset = 0, size = 0;
	char *buf;

	char *tag_name = strrchr(name, '/');
	if (tag_name != NULL)
		tag_name += 1;
	else
		tag_name = (char *)name;

//	printf("end!\n");

	ctx->depth--;
	if (ctx->depth == TAG_LEVEL - 1) {
		buf = XML_GetInputContext(ctx->parser, &offset, &size);
		//XML_GetCurrentByteIndex(ctx->parser);

		write(ctx->fd_out, buf + ctx->start_tag,
		    offset - ctx->start_tag);

//		if (XML_GetCurrentByteCount(ctx->parser) != 0)
//			printf("</%s>\n", tag_name);

		if (strcmp("streams:features", tag_name) == 0) {
//			printf("\n\nEND OF FEATURE\n\n");
			if (ctx->state == OPEN)
				xmpp_auth(ctx);
			else if (ctx->state == AUTH)
				xmpp_bind(ctx);
			else
				printf("state: %d\n", ctx->state);
		}

	}

	if (strcmp("urn:ietf:params:xml:ns:xmpp-bind:bind", tag_name) == 0 &&
	    ctx->state == BIND_OUT) {
		ctx->state = BIND;
		xmpp_session(ctx);
	}

//	printf("tag: %s\n", tag_name);

	/* SASL authentification successful */
	if (strcmp("urn:ietf:params:xml:ns:xmpp-sasl:success", tag_name) == 0) {
//		printf("AUTH!!!\n");
		ctx->state = AUTH;
		init_parser(ctx);
		xmpp_init(ctx);
		ctx->decl_done++;
	}
}

void
decl_handler(void *data, const char *version, const char *encoding,
    int standalone)
{
	struct context *ctx = data;

	if (ctx->decl_done == 0) {
		xmpp_init(ctx);
		ctx->decl_done = 1;
	} else {
		ctx->decl_done = 1;
	}
}

void
init_dir(struct context *ctx)
{
	if (mkdir(ctx->dir, S_IRWXU) < 0)
		if (errno != EEXIST)
			perror(__func__);

	char *file = NULL;
	asprintf(&file, "%s/out", ctx->dir);
	if ((ctx->fd_out = open(file, O_WRONLY|O_CREAT|O_TRUNC,
	    S_IRUSR|S_IWUSR)) < 0)
		perror(__func__);
	free(file);

	asprintf(&file, "%s/in", ctx->dir);
	if (mkfifo(file, S_IRUSR|S_IWUSR) > -1)
		if (errno != EEXIST)
			perror(__func__);

	if ((ctx->fd_in = open(file, O_RDONLY|O_NONBLOCK, 0)) < 0)
		perror(__func__);
	free(file);
}

/* (re)initialize eXpat parser */
void
init_parser(struct context *ctx)
{
	if (ctx->parser == NULL) {
		ctx->parser = XML_ParserCreateNS(NULL, ':');
	} else {
		XML_ParserFree(ctx->parser);
		ctx->parser = XML_ParserCreateNS(NULL, ':');
	}

	XML_SetXmlDeclHandler(ctx->parser, decl_handler);
	XML_SetElementHandler(ctx->parser, start_tag, end_tag);
	XML_SetUserData(ctx->parser, ctx);
	ctx->depth = 0;
}

void
usage(void)
{
	fprintf(stderr, "sj OPTIONS\n"
		"OPTIONS:\n"
		"\t-U <user>"
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
	ctx.parser = NULL;
	ctx.depth = 0;		/* depth of the current parsing tag level */
	ctx.decl_done = 0;	/* already send xml decl to server */
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

	init_dir(&ctx);
	init_parser(&ctx);
	xmpp_init(&ctx);
	ctx.decl_done++;

	int max_fd = ctx.sock;
	int n;
	fd_set readfds;

	/* timeinterval for keep alive pings */
	struct timeval tv = {30, 0};

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
			char *buff = XML_GetBuffer(ctx.parser, BUFSIZ);
			if (buff == NULL) goto err;

			if ((n = recv(ctx.sock, buff, BUFSIZ, 0)) < 0) goto err;

//			printf("NET: %.*s\n", n, buff);
//			printf("size: %d\n", n);

			if (XML_ParseBuffer(ctx.parser, n, n == 0)
			    == XML_STATUS_ERROR)
				goto err;
		} else if (FD_ISSET(ctx.fd_in, &readfds)) {
			char buf[BUFSIZ];
			ssize_t n = 0;
			while ((n = read(ctx.fd_in, buf, BUFSIZ)) > 0) {
				send(ctx.sock, buf, n, 0);
			}
		} else if (sel == 0) {
			fprintf(stdout, "sending ping to server\n");
			xmpp_ping(&ctx);
		} else { /* data from FIFO */
			printf("other event!\n");
		}
	}

	fprintf(stdout, "</stream:stream>");
	close(ctx.sock);

	return EXIT_SUCCESS;

 err:
	fprintf(stderr, "XML_ERROR: %s\n",
	    XML_ErrorString(XML_GetErrorCode(ctx.parser)));
	perror(__func__);
	return EXIT_FAILURE;
}
