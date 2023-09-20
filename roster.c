/*
 * Copyright (c) 2014-2015 Jan Klemkow <j.klemkow@wemelug.de>
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

#include <sys/stat.h>

#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <mxml.h>

static bool
result(mxml_node_t *iq)
{
	if (iq == NULL) return false;
	if (mxmlGetFirstChild(iq) != NULL) return false;

	return true;
}

static bool
add(FILE *fh, const char *jid, const char *name, const char *group)
{
	char group_str[BUFSIZ];
	char name_str[BUFSIZ];
	snprintf(group_str, sizeof group_str, "<group>%s</group>", group);
	snprintf(name_str, sizeof group_str, "name='%s'", name);

	if (fprintf(fh,
	    "<iq type='set' id='roster-%d'>"
		"<query xmlns='jabber:iq:roster'>"
		   "<item jid='%s' %s>%s</item>"
		"</query>"
	    "</iq>", getpid(), jid,
	    name  == NULL ? "" : name_str,
	    group == NULL ? "" : group_str) == -1) goto err;
	return true;
 err:
	perror(__func__);
	return false;
}

static bool
list(mxml_node_t *iq)
{
	mxml_node_t *item = NULL;

	if (iq == NULL) return false;
	if ((item = mxmlGetFirstChild(iq)) == NULL) return false;

	for (item = mxmlGetFirstChild(item); item != NULL;
	    item = mxmlGetNextSibling(item)) {
		const char *name = mxmlElementGetAttr(item, "name");
		const char *jid = mxmlElementGetAttr(item, "jid");
		const char *sub = mxmlElementGetAttr(item, "subscription");

		printf("%-30s\t%s\t%s\n", jid, sub, name == NULL ? "" : name);
	}
	return true;
}

static void
usage(void)
{
	fprintf(stderr, "roster [-d <dir>]\n");
	fprintf(stderr, "roster [-d <dir>] [-n <name>] [-g group] -a <jid>\n");
	fprintf(stderr, "roster [-d <dir>] -r <jid>\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	FILE *fh = NULL;
	int fd;
	int ch;
	bool add_flag = false;
	bool remove_flag = false;
	bool list_flag = false;
	char path_out[PATH_MAX];
	char path_in[PATH_MAX];
	char *dir = getenv("SJ_DIR");
	char *jid = NULL;
	char *name = NULL;
	char *group = NULL;

	while ((ch = getopt(argc, argv, "d:g:n:la:r:h")) != -1) {
		switch (ch) {
		case 'd':
			dir = strdup(optarg);
			break;
		case 'g':
			if ((group = strdup(optarg)) == NULL) goto err;
			break;
		case 'n':
			if ((name = strdup(optarg)) == NULL) goto err;
			break;
		case 'a':
			add_flag = true;
			if ((jid = strdup(optarg)) == NULL) goto err;
			break;
		case 'r':
			remove_flag = true;
			if ((jid = strdup(optarg)) == NULL) goto err;
			break;
		case 'h':
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (dir == NULL)
		usage();

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

	if (add_flag && jid != NULL) {
		add(fh, jid, name, group);
	} else if (remove_flag && jid != NULL) {
		if (fprintf(fh,
		    "<iq type='set' id='roster-%d'>"
			"<query xmlns='jabber:iq:roster'>"
			    "<item jid='%s' subscription='remove'/>"
			"</query>"
		    "</iq>", getpid(), jid) == -1) goto err;
	} else {
		list_flag = true;
		if (fprintf(fh,
		    "<iq type='get' id='roster-%d'>"
			"<query xmlns='jabber:iq:roster'/>"
		    "</iq>", getpid()) == -1) goto err;
	}

	if (fclose(fh) == EOF) goto err;

	/* read answer from server */
	if ((fd = open(path_in, O_RDONLY )) == -1) goto err;
	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) goto err;
	mxmlLoadFd(tree, fd, MXML_NO_CALLBACK);
	if (close(fd) == -1) goto err;
	if (unlink(path_in) == -1) goto err;

	if (list_flag &&
	    list(mxmlGetNextSibling(mxmlGetFirstChild(tree))) == false)
		return EXIT_FAILURE;

	if (add_flag &&
	    result(mxmlGetNextSibling(mxmlGetFirstChild(tree))) == false)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
 err:
	perror("roster");
	return EXIT_FAILURE;
}
