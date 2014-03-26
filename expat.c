#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <expat.h>

#define STREAM ""
#define TAG_LEVEL 2

struct context {
	int depth;
	int start_tag;
	XML_Parser parser;
};

#define NULL_CONTEXT { 0, 0, NULL }

void
start_tag(void *data, const char *el, const char **attr)
{
	struct context *ctx = data;
	int offset = 0, size = 0;
	ctx->depth++;

	const char *buf = XML_GetInputContext(ctx->parser, &offset, &size);
	int pos = XML_GetCurrentByteIndex(ctx->parser);
	printf("START: D:%d ->%.*s<-\n", ctx->depth, size, buf+offset);
	printf("START: D:%d ->%.*s<-\n", ctx->depth, size, buf+pos);

	if (ctx->depth == TAG_LEVEL) {
		ctx->start_tag = offset;
	}
	if (ctx->depth > 1)
		exit(EXIT_FAILURE);
}

void
end_tag(void *data, const char *name)
{
	struct context *ctx = data;
	int offset = 0, size = 0;
	const char *buf;

	ctx->depth--;

	if (ctx->depth == TAG_LEVEL - 1) {
		buf = XML_GetInputContext(ctx->parser, &offset, &size);
		int end_tag = XML_GetCurrentByteIndex(ctx->parser);

		printf("%.*s",
		    offset - ctx->start_tag,
		    buf + ctx->start_tag);

		if (XML_GetCurrentByteCount(ctx->parser) != 0)
			printf("<%s>\n", strrchr(name, '/'));
	}
}

int
main(int argc, char**argv)
{
	struct context ctx = NULL_CONTEXT;
	ssize_t size = 0;
	ssize_t offset = 0;
	int parse_size;

	XML_Parser parser = XML_ParserCreateNS(NULL, ':');
	ctx.parser = parser;
	ctx.depth = 0;

	XML_SetElementHandler(parser, start_tag, end_tag);
	XML_SetUserData(parser, &ctx);

	for (;;) {
		int bytes_read;
		void *buff = XML_GetBuffer(parser, 10);
		if (buff == NULL) {
			/* handle error */
		}

		bytes_read = read(STDIN_FILENO, buff, 10);
		if (bytes_read < 0) {
			perror("read(2)");
		}

		if (!XML_ParseBuffer(parser, bytes_read, bytes_read == 0)) {
			/* handle parse error */
		}

		if (bytes_read == 0)
			break;
	}

	return 0;
}
