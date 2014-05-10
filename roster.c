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

#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/stat.h>

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

	while ((ch = getopt(argc, argv, "bf:")) != -1) {
		switch (ch) {
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
	// TODO: mxml
	if (fclose(fh) == EOF) goto err;
	if (unlink(path_in) == -1) goto err;

	return EXIT_SUCCESS;
 err:
	perror("roster");
	return EXIT_FAILURE;
}
