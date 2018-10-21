// pathalias -- by steve bellovin, as told to peter honeyman

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "def.h"
#include "fns.h"

#define EQ(n, s)	(*(n)->name == *(s) && strcmp((n)->name, (s)) == 0)

// globals
Node **Table;			// hash table ^ priority queue
long Tabsize;			// size of Table

// privates
static void crcinit(void);
static void rehash(void);
static void lowercase(char *s);
static unsigned long fold(char *s);
static unsigned long hash(char *name, int unique);
static Node *isprivate(char *name);

static Node *Private;		// list of private nodes in current input file

//
// these numbers are chosen because:
//	-> they are prime,
//	-> they are monotonic increasing,
//	-> each is a tad smaller than a multiple of 1024,
//	-> they form a fibonacci sequence (almost).
// the first point yields good hash functions, the second is used for the
// standard re-hashing implementation of open addressing, the third
// optimizes for quirks in some mallocs i have seen, and the fourth simply
// appeals to me.
//
static long Primes[] = {
	1021, 2039, 3067, 5113, 8179, 13309, 21499, 34807, 56311, 0
};

static int Tabindex;
static long Tab128;		// Tabsize * 128

Node *
addnode(char *name)
{
	unsigned long i;
	Node *n;
	char *dot;

	if (Iflag)
		lowercase(name);

	// is it a private host?
	n = isprivate(name);
	if (n != NULL)
		return n;

	i = hash(name, 0);
	if (Table[i] != NULL)
		return Table[i];

	n = newnode();
	n->name = strsave(name);
	Table[i] = n;
	n->tloc = i;		// essentially a back link to the table

	if (InetFlag && Home != NULL &&
	    (dot = strrchr(name, '.')) != NULL && isadomain(dot + 1))
		addlink(Home, n, 100 + strlen(name), DEFNET, DEFDIR);

	return n;
}

void
alias(Node *n1, Node *n2)
{
	Link *l;

	if (ISADOMAIN(n1) && ISADOMAIN(n2)) {
		fprintf(stderr, "%s: domain alias %s = %s is illegal\n",
		    Argv[0], n1->name, n2->name);
		return;
	}
	l = addlink(n1, n2, (Cost)0, DEFNET, DEFDIR);
	l->flag |= LALIAS;
	l = addlink(n2, n1, (Cost)0, DEFNET, DEFDIR);
	l->flag |= LALIAS;
	if (Tflag)
		atrace(n1, n2);
}

//
// fold a string into an unsigned long int.  31 bit crc (from andrew appel).
// the crc table is computed at run time by crcinit() -- we could
// precompute, but it takes 1 clock tick on a 750.
//
// This fast table calculation works only if POLY is a prime polynomial
// in the field of integers modulo 2.  Since the coefficients of a
// 32-bit polynomial won't fit in a 32-bit word, the high-order bit is
// implicit.  IT MUST ALSO BE THE CASE that the coefficients of orders
// 31 down to 25 are zero.  Happily, we have candidates, from
// E. J.  Watson, "Primitive Polynomials (Mod 2)", Math. Comp. 16 (1962):
//	x^32 + x^7 + x^5 + x^3 + x^2 + x^1 + x^0
//	x^31 + x^3 + x^0
//
// We reverse the bits to get:
//	111101010000000000000000000000001 but drop the last 1
//         f   5   0   0   0   0   0   0
//	010010000000000000000000000000001 ditto, for 31-bit crc
//	   4   8   0   0   0   0   0   0
//

#define POLY32 0xf5000000UL	// 32-bit polynomial
#define POLY31 0x48000000UL	// 31-bit polynomial
#define POLY POLY31		// use 31-bit to avoid sign problems

static unsigned long CrcTable[128];

static void
crcinit(void)
{

	for (int i = 0; i < 128; i++) {
		unsigned long sum = 0;
		for (int j = 7 - 1; j >= 0; --j)
			if (i & (1 << j))
				sum ^= POLY >> j;
		CrcTable[i] = sum;
	}
}

static unsigned long
fold(char *s)
{
	unsigned long sum = 0;
	int c;

	while ((c = *s++) != '\0')
		sum = (sum >> 7) ^ CrcTable[(sum ^ c) & 0x7f];

	return sum;
}


#define HASH1(n) ((n) % Tabsize);
#define HASH2(n) (Tabsize - 2 - ((n) % (Tabsize-2)))	// sedgewick

//
// when alpha is 0.79, there should be 2 probes per access (gonnet).
// use long constant to force promotion.  Tab128 biases HIGHWATER by
// 128/100 for reduction in strength in isfull().
//
#define HIGHWATER	79UL
#define isfull(n)	((n) * 128 >= Tab128)

static unsigned long
hash(char *name, int unique)
{
	unsigned long probe;
	unsigned long hash2;
	Node *n;

	if (isfull(Ncount)) {
		if (Tabsize == 0) {	// first time
			crcinit();
			Tabindex = 0;
			Tabsize = Primes[0];
			Table = newtable(Tabsize);
			Tab128 = (HIGHWATER * Tabsize * 128UL) / 100UL;
		} else
			rehash();	// more, more!
	}

	probe = fold(name);
	hash2 = HASH2(probe);
	probe = HASH1(probe);

	//
	// probe the hash table.
	// if unique is set, we require a fresh slot.
	// otherwise, use double hashing to find either
	//  (1) an empty slot, or
	//  (2) a non-private copy of this host name
	//
	// this is an "inner loop."
	//
	while ((n = Table[probe]) != NULL) {
		if (EQ(n, name) && !(n->flag & ISPRIVATE) && !unique)
			return probe;	// this is it!

		while (probe < hash2)
			probe += Tabsize;
		probe -= hash2;	// double hashing
	}

	return probe;		// brand new
}

static void
rehash(void)
{
	Node **otable, **optr;
	unsigned long probe;
	unsigned long osize;

	optr = Table + Tabsize - 1;	// ptr to last
	otable = Table;
	osize = Tabsize;
	Tabsize = Primes[++Tabindex];
	if (Tabsize == 0)
		die("too many hosts");	// need more prime numbers
	vprint(stderr, "rehash into %d\n", Tabsize);
	Table = newtable(Tabsize);
	Tab128 = (HIGHWATER * Tabsize * 128UL) / 100UL;

	do {
		if (*optr == NULL)
			continue;	// empty slot in old table
		probe = hash((*optr)->name,
		    ((*optr)->flag & ISPRIVATE) != 0);
		if (Table[probe] != NULL)
			die("rehash error");
		Table[probe] = *optr;
		(*optr)->tloc = probe;
	} while (optr-- > otable);
	freetable(otable, osize);
}

// convert to lower case in place
static void
lowercase(char *s)
{
	do {
		if (isupper(*s))
			*s -= 'A' - 'a';	// ASCII
	} while (*s++);
}

/*
 * this might need change if privates catch on
 */
static Node *
isprivate(char *name)
{

	for (Node *n = Private; n != NULL; n = n->private)
		if (EQ(n, name))
			return n;

	return NULL;
}

// Add a private node so private that nobody can find it.
Node *
addhidden(char *name)
{
	Node *n;
	int i;

	if (Iflag)
		lowercase(name);

	n = newnode();
	n->name = strsave(name);
	n->flag = ISPRIVATE;
	i = hash(n->name, 1);
	if (Table[i] != NULL)
		die("impossible hidden node error");
	Table[i] = n;
	n->tloc = i;
	n->private = NULL;

	return n;
}

void
fixprivate(void)
{
	Node *n, *next;
	long i;

	for (n = Private; n != NULL; n = next) {
		n->flag |= ISPRIVATE;	// overkill, but safe
		i = hash(n->name, 1);
		if (Table[i] != NULL)
			die("impossible private node error");

		Table[i] = n;
		n->tloc = i;		// essentially a back link to the table
		next = n->private;
		n->private = NULL;	// clear for later use
	}
	Private = NULL;
}

Node *
addprivate(char *name)
{
	Node *n;

	if (Iflag)
		lowercase(name);
	if ((n = isprivate(name)) != NULL)
		return n;
	n = newnode();
	n->name = strsave(name);
	n->private = Private;
	Private = n;

	return n;
}
