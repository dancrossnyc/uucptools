// pathalias -- by steve bellovin, as told to peter honeyman

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

	if (Lcache != NULL) {
		rval = Lcache;
		Lcache = Lcache->next;
		memset(rval, 0, sizeof(Link));
	} else if ((rval = calloc(1, sizeof(Link))) == NULL)
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

	if ((rval = calloc(1, sizeof(Node))) == NULL)
		nomem();
	Ncount++;

	return rval;
}

Dom *
newdom(void)
{
	Dom *rval;

	if ((rval = calloc(1, sizeof(Dom))) == NULL)
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

	if ((rval = calloc(1, size * sizeof(Node *))) == NULL)
		nomem();

	return rval;
}

void
freetable(Node **t, long size)
{
	free(t);
}

static void
nomem(void)
{
	die("out of memory");
}
