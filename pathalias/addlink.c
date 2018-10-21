/*
 * pathalias -- by steve bellovin, as told to peter honeyman
 */

#include <stdio.h>
#include <string.h>

#include "def.h"
#include "fns.h"

/* exports */
char *Netchars = "!:@%";	/* sparse, but sufficient */
long Lcount;			/* how many edges? */

/* imports */
extern int Tflag, Dflag;

/* privates */
static void netbits(Link *l, int netchar, int netdir);
static void ltrace(Node *from, Node *to, Cost cost, int netchar, int netdir, char *message);
static void ltrprint(Node *from, Node *to, Cost cost, int netchar, int netdir, char *message);
static Link *Trace[NTRACE];
static int Tracecount;

#define EQ(n1, n2)	(strcmp((n1)->n_name, (n2)->n_name) == 0)
#define LTRACE		if (Tflag) ltrace

Link *
addlink(Node *from, Node *to, Cost cost, int netchar, int netdir)
{
	Link *l, *prev = 0;

	LTRACE(from, to, cost, netchar, netdir, "");
	/*
	 * maintain uniqueness for dead links (only).
	 */
	for (l = from->n_link; l; l = l->l_next) {
		if (!DEADLINK(l))
			break;
		if (to == l->l_to) {
			/* what the hell, use cheaper dead cost */
			if (cost < l->l_cost) {
				l->l_cost = cost;
				netbits(l, netchar, netdir);
			}
			return l;
		}
		prev = l;
	}


	/* allocate and link in the new link struct */
	l = newlink();
	if (cost != INF)	/* ignore back links */
		Lcount++;
	if (prev) {
		l->l_next = prev->l_next;
		prev->l_next = l;
	} else {
		l->l_next = from->n_link;
		from->n_link = l;
	}

	l->l_to = to;
	/* add penalty */
	if ((l->l_cost = cost + from->n_cost) < 0) {
		char buf[100];

		l->l_flag |= LDEAD;
		sprintf(buf, "link to %s ignored with negative cost",
		    to->n_name);
		yyerror(buf);
	}
	if (netchar == 0) {
		netchar = DEFNET;
		netdir = DEFDIR;
	}
	netbits(l, netchar, netdir);
	if (Dflag && ISADOMAIN(from))
		l->l_flag |= LTERMINAL;

	return l;
}

void
deadlink(Node *nleft, Node *nright)
{
	Link *l, *lhold = 0, *lprev, *lnext;

	/* DEAD host */
	if (nright == 0) {
		nleft->n_flag |= NDEAD;	/* DEAD host */
		return;
	}

	/* DEAD link */

	/* grab <nleft, nright> instances at head of nleft adjacency list */
	while ((l = nleft->n_link) != 0 && l->l_to == nright) {
		nleft->n_link = l->l_next;	/* disconnect */
		l->l_next = lhold;	/* terminate */
		lhold = l;	/* add to lhold */
	}

	/* move remaining <nleft, nright> instances */
	for (lprev = nleft->n_link; lprev && lprev->l_next;
	    lprev = lprev->l_next) {
		if (lprev->l_next->l_to == nright) {
			l = lprev->l_next;
			lprev->l_next = l->l_next;	/* disconnect */
			l->l_next = lhold;	/* terminate */
			lhold = l;
		}
	}

	/* check for emptiness */
	if (lhold == 0) {
		addlink(nleft, nright, INF / 2, DEFNET, DEFDIR)->l_flag |=
		    LDEAD;
		return;
	}

	/* reinsert deleted edges as DEAD links */
	for (l = lhold; l; l = lnext) {
		lnext = l->l_next;
		addlink(nleft, nright, l->l_cost, NETCHAR(l),
		    NETDIR(l))->l_flag |= LDEAD;
		freelink(l);
	}
}

static void
netbits(Link *l, int netchar, int netdir)
{
	l->l_flag &= ~LDIR;
	l->l_flag |= netdir;
	l->l_netop = netchar;
}

int
tracelink(char *arg)
{
	char *bang;
	Link *l;

	if (Tracecount >= NTRACE)
		return -1;
	l = newlink();
	bang = strchr(arg, '!');
	if (bang) {
		*bang = 0;
		l->l_to = addnode(bang + 1);
	} else
		l->l_to = 0;

	l->l_from = addnode(arg);
	Trace[Tracecount++] = l;
	return 0;
}

/*
 * the obvious choice for testing equality is to compare struct
 * addresses, but that misses private nodes, so we use strcmp().
 */
static void
ltrace(Node *from, Node *to, Cost cost, int netchar, int netdir, char *message)
{
	Link *l;
	int i;

	for (i = 0; i < Tracecount; i++) {
		l = Trace[i];
		/* overkill, but you asked for it! */
		if (l->l_to == 0) {
			if (EQ(from, l->l_from) || EQ(to, l->l_from))
				break;
		} else if (EQ(from, l->l_from) && EQ(to, l->l_to))
			break;
		else if (EQ(from, l->l_to) && EQ(to, l->l_from))
			break;	/* potential dead backlink */
	}
	if (i < Tracecount)
		ltrprint(from, to, cost, netchar, netdir, message);
}

/* print a trace item */
static void
ltrprint(Node *from, Node *to, Cost cost, int netchar, int netdir, char *message)
{
	char buf[256], *bptr = buf;

	strcpy(bptr, from->n_name);
	bptr += strlen(bptr);
	*bptr++ = ' ';
	if (netdir == LRIGHT)	/* @% */
		*bptr++ = netchar;
	strcpy(bptr, to->n_name);
	bptr += strlen(bptr);
	if (netdir == LLEFT)	/* !: */
		*bptr++ = netchar;
	sprintf(bptr, "(%ld) %s", cost, message);
	yyerror(buf);
}

void
atrace(Node *n1, Node *n2)
{
	Link *l;
	int i;
	char buf[256];

	for (i = 0; i < Tracecount; i++) {
		l = Trace[i];
		if (l->l_to == 0
		    && ((Node *) l->l_from == n1
		    || (Node *) l->l_from == n2)) {
			sprintf(buf, "%s = %s", n1->n_name, n2->n_name);
			yyerror(buf);
			return;
		}
	}
}

int
maptrace(Node *from, Node *to)
{
	Link *l;
	int i;

	for (i = 0; i < Tracecount; i++) {
		l = Trace[i];
		if (l->l_to == 0) {
			if (EQ(from, l->l_from) || EQ(to, l->l_from))
				return 1;
		} else if (EQ(from, l->l_from) && EQ(to, l->l_to))
			return 1;
	}
	return 0;
}

void
deletelink(Node *from, Node *to)
{
	Link *l, *lnext;

	l = from->n_link;

	/* delete all neighbors of from */
	if (to == 0) {
		while (l) {
			LTRACE(from, l->l_to, l->l_cost, NETCHAR(l),
			    NETDIR(l), "DELETED");
			lnext = l->l_next;
			freelink(l);
			l = lnext;
		}
		from->n_link = 0;
		return;
	}

	/* delete from head of list */
	while (l && EQ(to, l->l_to)) {
		LTRACE(from, to, l->l_cost, NETCHAR(l), NETDIR(l),
		    "DELETED");
		lnext = l->l_next;
		freelink(l);
		l = from->n_link = lnext;
	}

	/* delete from interior of list */
	if (l == 0)
		return;
	for (lnext = l->l_next; lnext; lnext = l->l_next) {
		if (EQ(to, lnext->l_to)) {
			LTRACE(from, to, l->l_cost, NETCHAR(l), NETDIR(l),
			    "DELETED");
			l->l_next = lnext->l_next;
			freelink(lnext);
			/* continue processing this link */
		} else
			l = lnext;	/* next link */
	}
}
