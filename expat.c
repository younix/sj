#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <expat.h>

#define STREAM ""

struct context {
	int depth;
	int found_tag;
	char *buf;
	char *buf_ptr;
	ssize_t buf_size;
	XML_Parser parser;
};

void
start_tag(void *data, const char *el, const char **attr)
{
	struct context *ctx = data;

////	if (strcmp(el, "stream") == 0 && ctx->depth == 1)

	printf("start: %d, %s\n", ctx->depth, el);
	ctx->depth++;
}

void
end_tag(void *data, const char *name)
{
	struct context *context = data;
	context->depth--;

	if (context->depth <= 1) {
		printf("end: %d, %s\n",
		    XML_GetCurrentByteIndex(context->parser), name);
//		XML_GetCurrentByteIndex(context->parser);
	}
}

int
main(int argc, char**argv)
{
	struct context ctx = {0};
	ssize_t size = 0;
	ssize_t offset = 0;
	int ret;

	XML_Parser parser = XML_ParserCreateNS(NULL, ':');
	ctx.parser = parser;
	ctx.depth = 0;
	ctx.found_tag = 0;
	ctx.buf_size = 10;
	ctx.buf = calloc(1, ctx.buf_size);
	ctx.buf_ptr = ctx.buf;

	XML_SetElementHandler(parser, start_tag, end_tag);
	XML_SetUserData(parser, &ctx);
	while ((size = read(STDIN_FILENO, ctx.buf_ptr + offset,
	    ctx.buf_size - offset))) {
		ret = XML_Parse(parser, ctx.buf_ptr, size, 1);
		printf("parse: %d\n", size);

		if (ctx.found_tag == 0) {
			ctx.buf_size *= 2;
			ctx.buf = ctx.buf = realloc(ctx.buf, ctx.buf_size);
			if (ctx.buf == NULL)
				exit(EXIT_FAILURE);
			ctx.buf_ptr = ctx.buf;
			offset += size;
		} else {
			offset = 0;
		}
	}

	printf("ende!!!\n");

	return 0;
}
