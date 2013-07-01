/*
 * Copyright (c) 2013 Jan Klemkow <j.klemkow@wemelug.de>
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <err.h>
#include <netdb.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <libxml/tree.h>

void
xmpp_init(int sock, char *user, char *server)
{
	char *msg = NULL;
	ssize_t size = asprintf(&msg,
	    "<?xml version='1.0'?>"
	    "<stream:stream "
//		"from='%s@%s' "
		"to='%s' "
		"version='1.0' "
		"xml:lang='en' "
		"xmlns='jabber:client' "
		"xmlns:stream='http://etherx.jabber.org/streams'>\n\n",
	    server);
//	    user, server, server);

	printf("size: %d\nmsg:%s\n", size, msg);

	size = send(sock, msg, strlen(msg), 0);
//	size = write(sock, msg, strlen(msg));
	printf("send size: %d\n", size);
}

void
xmpp_close(int sock)
{
	char *msg = "</stream:stream>";
	send(sock, msg, strlen(msg), 0);
}

void
usage(void)
{
	fprintf(stderr, "sj -u <user> -s <server>\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char**argv)
{
	char *user = "user";
	char *server = "jabber.ccc.de";
	char *port = "5222";
	char buff[BUFSIZ];
	buff[0] = '\0';

	struct addrinfo hints, *addrinfo = NULL, *addrinfo0 = NULL;
	int sock = 0, ch;

	while ((ch = getopt(argc, argv, "s:p:")) != -1) {
		switch (ch) {
		case 's':
			server = strdup(optarg);
			break;
		case 'p':
			port = strdup(optarg);
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	getaddrinfo(server, port, &hints, &addrinfo0);

	for (addrinfo = addrinfo0; addrinfo; addrinfo = addrinfo->ai_next) {

		if ((sock = socket(addrinfo->ai_family, addrinfo->ai_socktype, 0)) < 0)
			continue;

		if (connect(sock, addrinfo->ai_addr, addrinfo->ai_addrlen) < 0) {
			close(sock);
			continue;
		}

		break;
	}

	freeaddrinfo(addrinfo0);

	if (sock < 0)
		goto err;

//	xmpp_init(sock, user, server);
	xmpp_init(sock, "user", "jabber.ccc.de");

//	xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
//	xmlNodePtr node;
//	xmlBufferPtr buf = xmlBufferCreate();
//	xmlBufNodeDump(buf, doc, node, 0, 0);
//	xmlBufferDump(stdout, buf);

	fd_set readfds;
	int max_fd = sock;
	int n;
	FD_ZERO(&readfds);
	FD_SET(sock, &readfds);

	printf("event loop\n");

	for (;;) {
		select(max_fd+1, &readfds, NULL, NULL, NULL);
//		printf("trigger!\n");
		if (FD_ISSET(sock, &readfds)) {
			n = recv(sock, buff, BUFSIZ, 0);
			buff[n] = '\0';
//			node = xmlStringGetNodeList(doc, BAD_CAST buff);
//			printf("node: %p\n", node);
//			xmlBufNodeDump(buf, doc, node, 0, 0);
//			xmlBufferDump(stdout, buf);
			printf("n: %d", n);
			printf("r: %s\n", buff);
		} else {
			printf("other event!\n");
		}
		if (n < 1)
			break;
	}
/*
	xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
	xmlNodePtr node = xmlNewNode(NULL, (xmlChar*)"iq");
	xmlBufferPtr buf = xmlBufferCreate();
	xmlBufNodeDump(buf, doc, node, 0, 0);
	xmlBufferDump(stdout, buf);

	fprintf(stdout, "</stream:stream>");
	close(sock);
*/
	return EXIT_SUCCESS;

 err:
	perror(NULL);
	return EXIT_FAILURE;
}
