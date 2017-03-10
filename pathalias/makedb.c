/*
 * pathalias -- by steve bellovin, as told to peter honeyman
 */

#include <stdio.h>
#include <unistd.h>

#include "config.h"

typedef struct {
	char *dptr;
	int dsize;
} Datum;

char *Ofile = ALIASDB, *ProgName;

#define USAGE "%s [-o dbmname] [-a] [file ...]\n"

int
main(int argc, char *argv[])
{
	char *ofptr;
	int c, append = 0;
	extern int optind;
	extern char *optarg;

	ProgName = argv[0];
	while ((c = getopt(argc, argv, "o:a")) != -1)
		switch (c) {
		case 'o':	/* dbm output file */
			Ofile = optarg;
			break;

		case 'a':	/* append mode */
			append++;
			break;

		default:
			fprintf(stderr, USAGE, ProgName);
			exit(1);
			break;
		}


	if ((ofptr = strrchr(Ofile, '/')) != 0)
		ofptr++;
	else
		ofptr = Ofile;
	if (strlen(ofptr) > 10) {
		ofptr[10] = 0;
		fprintf(stderr, "%s: using %s for dbm output\n", ProgName,
		    Ofile);
	}

	if (append == 0 && dbfile(Ofile) != 0) {
		perror(Ofile);
		exit(1);
	}

	if (dbminit(Ofile) < 0) {
		perror(Ofile);
		exit(1);
	}

	if (optind == argc)
		makedb((char *)0);
	else
		for (; optind < argc; optind++)
			makedb(argv[optind]);
	exit(0);
}

int
dbfile(char *dbf)
{
	return (dbcreat(dbf, "dir") != 0 || dbcreat(dbf, "pag") != 0);
}

int
dbcreat(char *dbf, char *suffix)
{
	char buf[BUFSIZ];
	int fd;

	(void)sprintf(buf, "%s.%s", dbf, suffix);
	if ((fd = creat(buf, 0666)) < 0)
		return (-1);
	(void)close(fd);
	return (0);
}

int
makedb(char *ifile)
{
	char line[BUFSIZ];
	Datum key, val;

	if (ifile && (freopen(ifile, "r", stdin) == NULL)) {
		perror(ifile);
		return;
	}

	/*
	 * keys and values are 0 terminated.  this wastes time and (disk) space,
	 * but does lend simplicity and backwards compatibility.
	 */
	key.dptr = line;
	while (fgets(line, sizeof(line), stdin) != NULL) {
		char *op, *end;

		end = line + strlen(line);
		end[-1] = 0;	/* kill newline, stuff null terminator */
		op = strchr(line, '\t');
		if (op != NULL) {
			*op++ = 0;
			key.dsize = op - line;	/* 0 terminated */
			val.dptr = op;
			val.dsize = end - op;	/* 0 terminated */
		} else {
			key.dsize = end - line;	/* 0 terminated */
			val.dptr = "\0";	/* why must i do this? */
			val.dsize = 1;
		}
		if (store(key, val) < 0)
			perror(Ofile);
	}
}
