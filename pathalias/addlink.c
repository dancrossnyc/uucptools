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

#define EQ(n1, n2)	(strcmp((n1)->name, (n2)->name) == 0)
#define LTRACE		if (Tflag) ltrace

Link *
addlink(Node *from, Node *to, Cost cost, int netchar, int netdir)
{
	Link *l, *prev = 0;

	LTRACE(from, to, cost, netchar, netdir, "");
	/*
	 * maintain uniqueness for dead links (only).
	 */
	for (l = from->link; l; l = l->next) {
		if (!DEADLINK(l))
			break;
		if (to == l->to) {
			/* what the hell, use cheaper dead cost */
			if (cost < l->cost) {
				l->cost = cost;
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
		l->next = prev->next;
		prev->next = l;
	} else {
		l->next = from->link;
		from->link = l;
	}

	l->to = to;
	/* add penalty */
	if ((l->cost = cost + from->cost) < 0) {
		char buf[100];

		l->flag |= LDEAD;
		snprintf(buf, sizeof buf,
		    "link to %s ignored with negative cost",
		    to->name);
		yyerror(buf);
	}
	if (netchar == 0) {
		netchar = DEFNET;
		netdir = DEFDIR;
	}
	netbits(l, netchar, netdir);
	if (Dflag && ISADOMAIN(from))
		l->flag |= LTERMINAL;

	return l;
}

void
deadlink(Node *nleft, Node *nright)
{
	Link *l, *lhold = 0, *lprev, *lnext;

	/* DEAD host */
	if (nright == 0) {
		nleft->flag |= NDEAD;	/* DEAD host */
		return;
	}

	/* DEAD link */

	/* grab <nleft, nright> instances at head of nleft adjacency list */
	while ((l = nleft->link) != 0 && l->to == nright) {
		nleft->link = l->next;	/* disconnect */
		l->next = lhold;	/* terminate */
		lhold = l;	/* add to lhold */
	}

	/* move remaining <nleft, nright> instances */
	for (lprev = nleft->link; lprev && lprev->next;
	    lprev = lprev->next) {
		if (lprev->next->to == nright) {
			l = lprev->next;
			lprev->next = l->next;	/* disconnect */
			l->next = lhold;	/* terminate */
			lhold = l;
		}
	}

	/* check for emptiness */
	if (lhold == 0) {
		addlink(nleft, nright, INF / 2, DEFNET, DEFDIR)->flag |=
		    LDEAD;
		return;
	}

	/* reinsert deleted edges as DEAD links */
	for (l = lhold; l; l = lnext) {
		lnext = l->next;
		addlink(nleft, nright, l->cost, NETCHAR(l),
		    NETDIR(l))->flag |= LDEAD;
		freelink(l);
	}
}

static void
netbits(Link *l, int netchar, int netdir)
{
	l->flag &= ~LDIR;
	l->flag |= netdir;
	l->netop = netchar;
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
		l->to = addnode(bang + 1);
	} else
		l->to = 0;

	l->from = addnode(arg);
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
		if (l->to == 0) {
			if (EQ(from, l->from) || EQ(to, l->from))
				break;
		} else if (EQ(from, l->from) && EQ(to, l->to))
			break;
		else if (EQ(from, l->to) && EQ(to, l->from))
			break;	/* potential dead backlink */
	}
	if (i < Tracecount)
		ltrprint(from, to, cost, netchar, netdir, message);
}

/* print a trace item */
static void
ltrprint(Node *from, Node *to, Cost cost, int netchar, int netdir, char *message)
{
	char buf[256], nc[4];

	snprintf(nc, sizeof nc, "%c", netchar);
	snprintf(buf, sizeof buf,
	    "%s %s%s%s(%ld) %s",
	    from->name,
	    (netdir == LRIGHT) ? nc : "",
	    to->name,
	    (netdir == LLEFT) ? nc : "",
	    cost, message);
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
		if (l->to == 0
		    && ((Node *) l->from == n1
		    || (Node *) l->from == n2)) {
			snprintf(buf, sizeof buf, "%s = %s", n1->name, n2->name);
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
		if (l->to == 0) {
			if (EQ(from, l->from) || EQ(to, l->from))
				return 1;
		} else if (EQ(from, l->from) && EQ(to, l->to))
			return 1;
	}
	return 0;
}

void
deletelink(Node *from, Node *to)
{
	Link *l, *lnext;

	l = from->link;

	/* delete all neighbors of from */
	if (to == 0) {
		while (l) {
			LTRACE(from, l->to, l->cost, NETCHAR(l),
			    NETDIR(l), "DELETED");
			lnext = l->next;
			freelink(l);
			l = lnext;
		}
		from->link = 0;
		return;
	}

	/* delete from head of list */
	while (l && EQ(to, l->to)) {
		LTRACE(from, to, l->cost, NETCHAR(l), NETDIR(l),
		    "DELETED");
		lnext = l->next;
		freelink(l);
		l = from->link = lnext;
	}

	/* delete from interior of list */
	if (l == 0)
		return;
	for (lnext = l->next; lnext; lnext = l->next) {
		if (EQ(to, lnext->to)) {
			LTRACE(from, to, l->cost, NETCHAR(l), NETDIR(l),
			    "DELETED");
			l->next = lnext->next;
			freelink(lnext);
			/* continue processing this link */
		} else
			l = lnext;	/* next link */
	}
}
