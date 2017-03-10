/* pathalias -- by steve bellovin, as told to peter honeyman */

/* default place for dbm output of makedb (or use -o at run-time) */
#define	ALIASDB			"/usr/local/lib/palias"

/* the usual case: unix */
#define	NULL_DEVICE	"/dev/null"
#define	OK		0
#define	ERROR		1
#define	SEVERE_ERROR	(-1)

#define strclear(s, n)	((void) memset((s), 0, (n)))
