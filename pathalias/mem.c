/* pathalias -- by steve bellovin, as told to peter honeyman */
#ifndef lint
static char *sccsid = "@(#)mem.c	9.6 92/08/25";
#endif

#include <string.h>

#include "def.h"

/* exports */
long Ncount;

/* imports */
extern char *Netchars;
extern int Vflag;

/* privates */
static void nomem();
static Link *Lcache;
static unsigned int Memwaste;

Link *
newlink(void)
{
	Link *rval;

	if (Lcache) {
		rval = Lcache;
		Lcache = Lcache->l_next;
		strclear((char *)rval, sizeof(Link));
	} else if ((rval = (Link *) calloc(1, sizeof(Link))) == 0)
		nomem();
	return rval;
}

/* caution: this destroys the contents of l_next */
void
freelink(Link *l)
{
	l->l_next = Lcache;
	Lcache = l;
}

Node *
newnode(void)
{
	Node *rval;

	if ((rval = (Node *) calloc(1, sizeof(Node))) == 0)
		nomem();
	Ncount++;
	return rval;
}

Dom *
newdom(void)
{
	Dom *rval;

	if ((rval = (Dom *) calloc(1, sizeof(Dom))) == 0)
		nomem();

	return rval;
}


char *
strsave(char *s)
{
	char *r;

	if ((r = malloc((unsigned)strlen(s) + 1)) == 0)
		nomem();
	(void)strcpy(r, s);
	return r;
}

Node **
newtable(long size)
{
	Node **rval;

	if ((rval =
	    (Node **) calloc(1,
	    (unsigned int)size * sizeof(Node *))) == 0)
		nomem();
	return rval;
}

void
freetable(Node **t, long size)
{
	free((char *)t);
}

static void
nomem(void)
{
#ifdef DEBUG
	static char epitaph[128];

	sprintf(epitaph, "out of memory (%ldk allocated)", allocation());
	die(epitaph);
#else
	die("out of memory");
#endif
}

/* data space allocation -- main sets `dataspace' very early */
long
allocation(void)
{
	return 0;
}

/* how much memory has been wasted? */
void
wasted(void)
{
	if (Memwaste == 0)
		return;
	vprint(stderr, "memory allocator wasted %ld bytes\n", Memwaste);
}
