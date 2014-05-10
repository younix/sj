#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void
usage(void)
{
	fprintf(stderr, "roster [-l]\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char*argv[])
{
	int ch;
	bool list_flag = false;
	char path[_XOPEN_PATH_MAX];
	char *dir = ".";
	FILE *fh = NULL;

	while ((ch = getopt(argc, argv, "l")) != -1) {
		switch (ch) {
		case 'l':
			list_flag = 1;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	snprintf(path, sizeof path, "%s/roster-%d", dir, getpid());
	if ((fh = fopen(path, "r")) == NULL) goto err;

	if (list_flag == true) {
		fprintf(fh, "");
	}

	if (fclose(fh) == -1) goto err;

	return EXIT_SUCCESS;
 err:
	perror("roster");
	return EXIT_FAILURE;
}
