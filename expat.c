#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <expat.h>

#define STREAM ""

struct context {
	int depth;
	int found_tag;
	int start_tag;
	char *buf;
	char *buf_ptr;
	char *parse_ptr;
	ssize_t buf_size;
	XML_Parser parser;
};

void
start_tag(void *data, const char *el, const char **attr)
{
	struct context *ctx = data;

////	if (strcmp(el, "stream") == 0 && ctx->depth == 1)

	printf("start: %d, %s\n", ctx->depth, el);
	if (ctx->depth == 2)
		ctx->start_tag = XML_GetCurrentByteIndex(ctx->parser);
	ctx->depth++;
}

void
end_tag(void *data, const char *name)
{
	struct context *ctx = data;
	ctx->depth--;

	if (ctx->depth <= 3) {
		printf("end: %d, %s\n",
		    XML_GetCurrentByteIndex(ctx->parser), name);
//		XML_GetCurrentByteIndex(ctx->parser);
		printf("\n\n%.*s\n", XML_GetCurrentByteIndex(ctx->parser) - ctx->start_tag,
		    ctx->buf + ctx->start_tag);
	}
}

int
main(int argc, char**argv)
{
	struct context ctx = {0};
	ssize_t size = 0;
	ssize_t offset = 0;
	int parse_size;

	XML_Parser parser = XML_ParserCreateNS(NULL, ':');
	ctx.parser = parser;
	ctx.depth = 0;
	ctx.found_tag = 0;
	ctx.buf_size = BUFSIZ;
	ctx.buf = calloc(1, ctx.buf_size);
	ctx.buf_ptr = ctx.buf;
	ctx.parse_ptr = ctx.buf;

	XML_SetElementHandler(parser, start_tag, end_tag);
	XML_SetUserData(parser, &ctx);
	while ((size = read(STDIN_FILENO, ctx.buf_ptr + offset,
	    ctx.buf_size - offset))) {
		parse_size = XML_Parse(parser, ctx.parse_ptr, size, 1);
		printf("parse: %d\n", size);

		if (ctx.found_tag == 0) {
			ctx.buf_size *= 2;
			ctx.buf = ctx.buf = realloc(ctx.buf, ctx.buf_size);
			if (ctx.buf == NULL)
				exit(EXIT_FAILURE);
			ctx.buf_ptr = ctx.buf + size;
			ctx.parse_ptr = ctx.buf + size;
			offset += size;
		} else {
			offset = 0;
			ctx.buf_ptr = ctx.buf;
		}
	}

	printf("ende!!!\n");

	return 0;
}
