/*
 * Copyright (c) 2014 Jan Klemkow <j.klemkow@wemelug.de>
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
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

bool
isshow(const char *show)
{
	if (strcmp(show, "away") == 0 ||
	    strcmp(show, "chat") == 0 ||
	    strcmp(show, "dnd") == 0 ||
	    strcmp(show, "xa") == 0)
		return true;

	return false;
}

bool
istype(const char *type)
{
	if (strcmp(type, "subscribe") == 0 ||
	    strcmp(type, "subscribed") == 0 ||
	    strcmp(type, "unavailable") == 0 ||
	    strcmp(type, "unsubscribe") == 0 ||
	    strcmp(type, "unsubscribed") == 0)
		return true;

	return false;
}

void
usage(void)
{
	fprintf(stderr,
	    "presence [-d <dir>] [-t <to>] [-s <show>] [-p <prio>] [type]\n"
	    "  prio: -128..127\n"
	    "  show: away|chat|dnd|xa\n"
	    "  type: subscribe|subscribed|unavailable|unsubscribe|"
	    "unsubscribed\n");

	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	FILE *fh = NULL;
	int ch;
	char path_out[PATH_MAX];
	char *dir = getenv("SJ_DIR");
	char *to = NULL;
	char *show = NULL;
	char *status = NULL;
	char *type = NULL;
	signed int priority = 0;
	char type_str[BUFSIZ];
	char to_str[BUFSIZ];
	char show_str[BUFSIZ];
	char status_str[BUFSIZ];

	while ((ch = getopt(argc, argv, "d:t:s:p:h")) != -1) {
		switch (ch) {
		case 'd':
			dir = strdup(optarg);
			break;
		case 't':
			if ((to = strdup(optarg)) == NULL) goto err;
			break;
		case 's':
			if ((show = strdup(optarg)) == NULL) goto err;
			if (isshow(show) == false)
				usage();
			break;
		case 'S':
			if ((status = strdup(optarg)) == NULL) goto err;
			break;
		case 'p':
			priority = strtol(optarg, NULL, 0);
			if (errno != 0) goto err;
			if (priority < -128 || priority > 127)
				usage();
			break;
		case 'h':
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 1) {
		type = argv[0];
		if (istype(type) == false)
			usage();
	}

	if (dir == NULL)
		usage();

	snprintf(to_str, sizeof to_str, "to='%s'", to);
	snprintf(type_str, sizeof type_str, "type='%s'", type);
	snprintf(show_str, sizeof show_str, "<show>%s</show>", show);

	/* send query to server */
	snprintf(path_out, sizeof path_out, "%s/%s", dir, "in");
	if ((fh = fopen(path_out, "w")) == NULL) goto err;

	if (fprintf(fh,
	    "<presence id='presence-%d' %s %s>"
		"<priority>%d</priority>"
		"%s %s"
	    "</presence>",
	    getpid(),
	    to       == NULL ? "" : to_str,
	    type     == NULL ? "" : type_str,
	    priority,
	    show     == NULL ? "" : show_str,
	    status   == NULL ? "" : status_str) == -1) goto err;

	if (fclose(fh) == EOF) goto err;

	return EXIT_SUCCESS;
 err:
	perror("roster");
	return EXIT_FAILURE;
}
