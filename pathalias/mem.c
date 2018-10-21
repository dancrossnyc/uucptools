/*
 * pathalias -- by steve bellovin, as told to peter honeyman
 */

#include <stdlib.h>
#include <string.h>

#include "def.h"
#include "fns.h"

// globals
long Ncount;

// privates
static void nomem(void);
static Link *Lcache;

Link *
newlink(void)
{
	Link *rval;

	if (Lcache) {
		rval = Lcache;
		Lcache = Lcache->next;
		memset(rval, 0, sizeof(Link));
	} else if ((rval = (Link *) calloc(1, sizeof(Link))) == 0)
		nomem();
	return rval;
}

// caution: this destroys the contents of next
void
freelink(Link *l)
{
	l->next = Lcache;
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

	r = strdup(s);
	if (r  == NULL)
		nomem();

	return r;
}

Node **
newtable(long size)
{
	Node **rval;

	if ((rval =
	    (Node **) calloc(1,
	    size * sizeof(Node *))) == 0)
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
	die("out of memory");
}
