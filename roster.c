/*
 * Copyright (c) Jan Klemkow <j.klemkow@wemelug.de>
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

#define _XOPEN_SOURCE 700

#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/stat.h>

#include <mxml.h>

bool
list(mxml_node_t *iq)
{
	mxml_node_t *item = NULL;

	if (iq == NULL) return false;
	if (iq->child == NULL) return false;

	for (item = iq->child->child; item != NULL; item = item->next) {
		const char *name = mxmlElementGetAttr(item, "name");
		const char *jid = mxmlElementGetAttr(item, "jid");
		const char *sub = mxmlElementGetAttr(item, "subscription");

		printf("%-30s\t%s\t%s\n", jid, sub, name == NULL ? "" : name);
	}
	return true;
}

void
usage(void)
{
	fprintf(stderr, "roster [-l]\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	FILE *fh = NULL;
	int ch;
	bool list_flag = false;
	char path_out[_XOPEN_PATH_MAX];
	char path_in[_XOPEN_PATH_MAX];
	char *dir = ".";
	char *jid = NULL;
	char *name = NULL;
	char *group = NULL;

	while ((ch = getopt(argc, argv, "d:l")) != -1) {
		switch (ch) {
		case 'j':
			if ((jid = strdup(optarg)) == NULL) goto err;
			break;
		case 'd':
			dir = strdup(optarg);
			break;
		case 'l':
			list_flag = true;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	/* HACK: we need this, cause mxml can't parse tags by itself */
        mxml_node_t *tree = NULL;
        const char *base = "<?xml ?><stream:stream></stream:stream>";
	tree = mxmlLoadString(NULL, base, MXML_NO_CALLBACK);
	assert(tree != NULL);

	/* prepare fifo for answer from server */
	snprintf(path_in, sizeof path_in, "%s/roster-%d", dir, getpid());
	if (mkfifo(path_in, S_IRUSR|S_IWUSR) == -1) goto err;

	/* send query to server */
	snprintf(path_out, sizeof path_out, "%s/%s", dir, "in");
	if ((fh = fopen(path_out, "w")) == NULL) goto err;
	if (fprintf(fh,
	    "<iq type='get' id='roster-%d'>"
		"<query xmlns='jabber:iq:roster'/>"
	    "</iq>", getpid()) == -1) goto err;
	if (fclose(fh) == EOF) goto err;

	/* read answer from server */
	if ((fh = fopen(path_in, "r")) == NULL) goto err;
	mxmlLoadFile(tree, fh, MXML_NO_CALLBACK);
	if (fclose(fh) == EOF) goto err;
	if (unlink(path_in) == -1) goto err;

	if (list(tree->child->next) == false) return EXIT_FAILURE;

	return EXIT_SUCCESS;
 err:
	perror("roster");
	return EXIT_FAILURE;
}
