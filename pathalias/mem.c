/* pathalias -- by steve bellovin, as told to peter honeyman */
#ifndef lint
static char *sccsid = "@(#)mem.c	9.6 92/08/25";
#endif

#include <string.h>

#include "def.h"

/* exports */
long Ncount;
extern void freelink(), wasted(), freetable();
extern long allocation();

/* imports */
extern char *Netchars;
extern int Vflag;
extern void die();

/* privates */
static void nomem();
static link *Lcache;
static unsigned int Memwaste;

link *
newlink(void)
{
	link *rval;

	if (Lcache) {
		rval = Lcache;
		Lcache = Lcache->l_next;
		strclear((char *)rval, sizeof(link));
	} else if ((rval = (link *) calloc(1, sizeof(link))) == 0)
		nomem();
	return rval;
}

/* caution: this destroys the contents of l_next */
void
freelink(link *l)
{
	l->l_next = Lcache;
	Lcache = l;
}

node *
newnode(void)
{
	node *rval;

	if ((rval = (node *) calloc(1, sizeof(node))) == 0)
		nomem();
	Ncount++;
	return rval;
}

dom *
newdom(void)
{
	dom *rval;

	if ((rval = (dom *) calloc(1, sizeof(dom))) == 0)
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

node **
newtable(long size)
{
	node **rval;

	if ((rval =
	    (node **) calloc(1,
	    (unsigned int)size * sizeof(node *))) == 0)
		nomem();
	return rval;
}

void
freetable(node **t, long size)
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
	vprintf(stderr, "memory allocator wasted %ld bytes\n", Memwaste);
}
