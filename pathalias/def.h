/* pathalias -- by steve bellovin, as told to peter honeyman */

#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>

typedef long Cost;
typedef struct Node Node;
typedef struct Link Link;
typedef struct Dom Dom;

extern int Vflag;

static inline int
vprint(FILE *fp, const char *fmt, ...)
{
	int r = 0;

	if (Vflag) {
		va_list ap;
		va_start(ap, fmt);
		r = vfprintf(fp, fmt, ap);
		va_end(ap);
	}

	return r;
}

#define NTRACE	16		/* can trace up to NTRACE hosts/links */

/* flags for n_flag */
#define ISPRIVATE  0x0001	/* invisible outside its definition file */
#define NALIAS	   0x0002	/* heaped as an alias */
#define ATSIGN	   0x0004	/* seen an at sign?  used for magic @/% rules */
#define MAPPED	   0x0008	/* extracted from heap */
#define	NDEAD	   0x0010	/* out links are dead */
#define HASLEFT	   0x0020	/* has a left side net character */
#define HASRIGHT   0x0040	/* route has a right side net character */
#define	NNET	   0x0080	/* network pseudo-host */
#define INDFS	   0x0100	/* used when removing net cycles (for -g) */
#define DUMP	   0x0200	/* we have dumped this net's edges (for -g) */
#define PRINTED	   0x0400	/* this host has been printed */
#define NTERMINAL  0x0800	/* heaped as terminal edge (or alias thereto) */
#define NREF	   0x1000	/* node has an "interesting" reference */

#define ISADOMAIN(n)	 ((n)->name[0] == '.')
#define ISANET(n)	 (((n)->flag & NNET) || ISADOMAIN(n))
#define ALTEREGO(n1, n2) ((n1)->name == (n2)->name)
#define DEADHOST(n)	 (((n)->flag & (NDEAD | NTERMINAL)) && !ISANET(n))
#define DEADLINK(l)	 ((l)->flag & LDEAD)
#define DEADNET(n)	 (((n)->flag & (NNET | NDEAD)) == (NNET | NDEAD))
#define GATEWAYED(n)	 (DEADNET(n) || ISADOMAIN(n))

struct Node {
	char *name;		/* host name */
	Link *link;		/* adjacency list */
	Cost cost;		/* cost to this host */
	Node *net;		/* others in this network (parsing) */
	Node *root;		/* root of net cycle (graph dumping) */
	Node *copy;		/* circular copy list (mapping) */
	Node *private;		/* other privates in this file (parsing) */
	Node *parent;		/* parent in shortest path tree (mapping) */
	unsigned int tloc;	/* back ptr to heap/hash table */
	unsigned int flag;	/* see manifests above */
};

#define MILLION (1000L * 1000L)
#define	DEFNET	'!'		/* default network operator */
#define	DEFDIR	LLEFT		/* host on left is default */
#define	DEFCOST	((Cost)4000)	/* default cost of a link */
#define	INF	((Cost)100 * MILLION)	/* infinitely expensive link */
#define DEFPENALTY ((Cost) 200)	/* default avoidance cost */

/*
 * data structure for adjacency list representation
 */

/* flags for l_dir */
#define NETDIR(l)	((l)->flag & LDIR)
#define NETCHAR(l)	((l)->netop)
#define LDIR	  0x0008	/* 0 for left, 1 for right */
#define LRIGHT	  0x0000	/* user@host style */
#define LLEFT	  0x0008	/* host!user style */
#define LDEAD	  0x0010	/* this link is dead */
#define LALIAS	  0x0020	/* this link is an alias */
#define LTREE	  0x0040	/* member of shortest path tree */
#define LGATEWAY  0x0080	/* this link is a gateway */
#define LTERMINAL 0x0100	/* this link is terminal */

/*
 * Link structure.
 */
struct Link {
	Node *to;		/* adjacent node */
	Cost cost;		/* edge cost */
	Link *next;		/* rest of adjacency list (not tracing) */
	Node *from;		/* source node (tracing) */
	short flag;		/* right/left syntax, flags */
	char netop;		/* network operator */
};

/*
 * Doubly linked list for known and unknown domains.
 */
struct Dom {
	Dom *next;
	Dom *prev;
	char *name;
};

extern int Argc;
extern int Cflag;
extern int Dflag;
extern int Fflag;
extern int Iflag;
extern int InetFlag;
extern int Lineno;
extern int Tflag;
extern int Vflag;
extern long Hashpart;
extern long Lcount;
extern long Ncount;
extern long Nheap;
extern long Nlink;
extern long NumLcopy;
extern long NumNcopy;
extern long Tabsize;
extern long Tcount;
extern char **Argv;
extern char *Cfile;
extern char *Graphout;
extern char *Linkout;
extern char *Netchars;
extern char *optarg;
extern Node **Table;
extern Node *Home;
