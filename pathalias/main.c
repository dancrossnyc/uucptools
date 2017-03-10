/*
 * pathalias -- by steve bellovin, as told to peter honeyman
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "def.h"
#include "fns.h"

/* globals */
char *Cfile;			/* current input file */
char *Graphout;			/* file for dumping edges (-g option) */
char *Linkout;			/* file for dumping shortest path tree */
char **Argv;			/* external copy of argv (for input files) */
Node *Home;			/* node for local host */
int Cflag;			/* print costs (-c option) */
int Dflag;			/* penalize routes beyond domains (-D option) */
int Iflag;			/* ignore case (-i option) */
int Tflag;			/* trace links (-t option) */
int Vflag;			/* verbose (-v option) */
int Fflag;			/* print cost of first hop */
int InetFlag;			/* local host is w/in scope of DNS (-I flag) */
int Lineno = 1;			/* line number within current input file */
int Argc;			/* external copy of argc (for input files) */

#define USAGE "usage: %s [-vciDfI] [-l localname] [-d deadlink] [-t tracelink] [-g edgeout] [-s treeout] [-a avoid] [files ...]\n"

int
main(int argc, char *argv[])
{
	char *locname = 0, *bang;
	int c;
	int errflg = 0;

	setbuf(stderr, (char *)0);
	Cfile = "[deadlinks]";	/* for tracing dead links */
	Argv = argv;
	Argc = argc;

	while ((c = getopt(argc, argv, "cd:Dfg:iIl:s:t:v")) != -1)
		switch (c) {
		case 'c':	/* print cost info */
			Cflag++;
			break;
		case 'd':	/* dead host or link */
			if ((bang = strchr(optarg, '!')) != 0) {
				*bang++ = 0;
				deadlink(addnode(optarg), addnode(bang));
			} else
				deadlink(addnode(optarg), (Node *) 0);
			break;
		case 'D':	/* penalize routes beyond domains */
			Dflag++;
			break;
		case 'f':	/* print cost of first hop */
			Cflag++;
			Fflag++;
			break;
		case 'g':	/* graph output file */
			Graphout = optarg;
			break;
		case 'i':	/* ignore case */
			Iflag++;
			break;
		case 'I':	/* Internet connected */
			InetFlag++;
			break;
		case 'l':	/* local name */
			locname = optarg;
			break;
		case 's':	/* show shortest path tree */
			Linkout = optarg;
			break;
		case 't':	/* trace this link */
			if (tracelink(optarg) < 0) {
				fprintf(stderr,
				    "%s: can trace only %d links\n",
				    Argv[0], NTRACE);
				exit(ERROR);
			}
			Tflag = 1;
			break;
		case 'v':	/* verbose stderr, mixed blessing */
			Vflag++;
			break;
		default:
			errflg++;
		}

	if (errflg) {
		fprintf(stderr, USAGE, Argv[0]);
		exit(ERROR);
	}
	argv += optind;		/* kludge for yywrap() */

	if (*argv)
		freopen(NULL_DEVICE, "r", stdin);
	else
		Cfile = "[stdin]";

	if (!locname)
		locname = local();
	if (*locname == 0) {
		locname = "lostinspace";
		fprintf(stderr, "%s: using \"%s\" for local name\n",
		    Argv[0], locname);
	}

	Home = addnode(locname);	/* add home node */
	Home->cost = 0;	/* doesn't cost to get here */

	(void)yyparse();	/* read in link info */

	vprint(stderr, "%ld nodes, %ld links\n", Ncount, Lcount);

	Cfile = "[backlinks]";	/* for tracing back links */
	Lineno = 0;

	/* compute shortest path tree */
	mapit();

	/* traverse tree and print paths */
	printit();

	return 0;
}

void
die(char *s)
{
	fprintf(stderr, "%s: %s; notify the authorities\n", Argv[0], s);
	exit(SEVERE_ERROR);
}
