#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct context {
	char *jid;
	char *id;
};

#define NULL_CONTEXT {	\
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

void
usage(void)
{
	fprintf(stderr, "messaged [-j JID]\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	struct context ctx = NULL_CONTEXT;
	int ch;

	while ((ch = getopt(argc, argv, "j:")) != -1) {
		switch (ch) {
		case 'j':
			ctx.jid = strdup(optarg);
			break;
		case 'f':
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	asprintf(&ctx.id, "messaged-%d", getpid());
	msg_send(&ctx, "test", "younix@jabber.ccc.de");

	return EXIT_SUCCESS;
}
