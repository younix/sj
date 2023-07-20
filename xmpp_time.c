#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <mxml.h>

static void
send_time(FILE *fh, const char *to, const char *id)
{
	char tzo[BUFSIZ];
	char utc[BUFSIZ];
	time_t t = time(NULL);

	/* +HHMM  */
	strftime(tzo, sizeof tzo, "%z", localtime(&t));

	/* convert "+HHMM" to "+HH:MM" */
	tzo[6] = tzo[5];
	tzo[5] = tzo[4];
	tzo[4] = tzo[3];
	tzo[3] = ':';

	/* 2006-12-19T17:58:35Z */
	strftime(utc, sizeof utc, "%FT%TZ", gmtime(&t));

	fprintf(fh,
	"<iq type='result' to='%s' id='%s'>"
	  "<time xmlns='urn:xmpp:time'>"
	    "<tzo>%s</tzo>"
	    "<utc>%s</utc>"
	  "</time>"
	"</iq>", to, id, tzo, utc);
}

static void
usage(void)
{
	fprintf(stderr, "xmpp:time [-d dir]\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	/* HACK: we need this, cause mxml can't parse pure tags */
	mxml_node_t *tree = NULL;
	mxml_node_t *node = NULL;
	const char *base = "<?xml ?>";
	const char *id = NULL;
	const char *from = NULL;
	const char *type = NULL;
	char *dir = ".";
	char out_file[PATH_MAX];
	FILE *fh;
	int ch;

	while ((ch = getopt(argc, argv, "d:h")) != -1) {
		switch (ch) {
		case 'd':
			if ((dir = strdup(optarg)) == NULL)
				err(EXIT_FAILURE, "strdup");
			break;
		case 'h':
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if ((tree = mxmlLoadString(NULL, base, MXML_NO_CALLBACK)) == NULL)
		err(EXIT_FAILURE, "unable to read xml tree");
	mxmlLoadFile(tree, stdin, MXML_NO_CALLBACK);

	/* check iq tag */
	if ((node = mxmlGetFirstChild(tree)) == NULL)
		errx(EXIT_FAILURE, "unable to parse xml data");

	if ((id = mxmlElementGetAttr(node, "id")) == NULL)
		errx(EXIT_FAILURE, "iq stanze has no \"id\" attribute");

	if ((from = mxmlElementGetAttr(node, "from")) == NULL)
		errx(EXIT_FAILURE, "iq stanze has no \"from\" attribute");

	if ((type = mxmlElementGetAttr(node, "type")) == NULL)
		errx(EXIT_FAILURE, "iq stanze has no \"type\" attribute");

	if (strcmp(type, "get") != 0)
		errx(EXIT_FAILURE, "unable to handle iq type: %s", type);

	/* open file for output */
	snprintf(out_file, sizeof out_file, "%s/in", dir);
	if ((fh = fopen(out_file, "w")) == NULL)
		err(EXIT_FAILURE, "fopen");

	send_time(fh, from, id);

	if (fclose(fh) == EOF)
		err(EXIT_FAILURE, "fclose");

	return EXIT_SUCCESS;
}
