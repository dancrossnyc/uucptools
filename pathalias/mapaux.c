/*
 * pathalias -- by steve bellovin, as told to peter honeyman
 */

#include <stdio.h>
#include <string.h>

#include "def.h"
#include "fns.h"

// privates
static void dumpnode(Node *from);
static void untangle(void);
static void dfs(Node *n);
static int height(Node *n);
static Link *lcopy(Node *parent, Node *n);

static FILE *Gstream;		// for dumping graph

/*
 * slide everything from Table[low] to Table[high]
 * up toward the high end.  make room!  make room!
 */
long
pack(long low, long high)
{
	long hole, next;

	// find first "hole "
	for (hole = high; hole >= low && Table[hole] != 0; --hole);

	// repeatedly move next filled entry into last hole
	for (next = hole - 1; next >= low; --next) {
		if (Table[next] != 0) {
			Table[hole] = Table[next];
			Table[hole]->tloc = hole;
			Table[next] = 0;
			while (Table[--hole] != 0)	// find next hole
				;
		}
	}
	return hole + 1;
}

void
resetnodes(void)
{
	long i;
	Node *n;

	for (i = Hashpart; i < Tabsize; i++)
		if ((n = Table[i]) != 0) {
			n->cost = (Cost) 0;
			n->flag &=
			    ~(NALIAS | ATSIGN | MAPPED | HASLEFT | HASRIGHT
			    | NTERMINAL);
			n->copy = n;
		}

	Home->cost = (Cost) 0;
	Home->flag &=
	    ~(NALIAS | ATSIGN | MAPPED | HASLEFT | HASRIGHT | NTERMINAL);
	Home->copy = Home;
}

void
dumpgraph(void)
{
	long i;
	Node *n;

	if ((Gstream = fopen(Graphout, "w")) == NULL) {
		fprintf(stderr, "%s: ", Argv[0]);
		perror(Graphout);
		return;
	}

	untangle();		// untangle net cycles for proper output

	for (i = Hashpart; i < Tabsize; i++) {
		n = Table[i];
		if (n == 0)
			continue;	// impossible, but ...
		// a minor optimization ...
		if (n->link == 0)
			continue;
		// pathparse doesn't need these
		if (n->flag & NNET)
			continue;
		dumpnode(n);
	}
}

static void
dumpnode(Node *from)
{
	Node *to;
	Link *l;
	Link *lnet = 0, *ll, *lnext;

	for (l = from->link; l; l = l->next) {
		to = l->to;
		if (from == to)
			continue;	// oops -- it's me!

		if ((to->flag & NNET) == 0) {
			// host -> host -- print host>host
			if (l->cost == INF)
				continue;	// phoney link
			fputs(from->name, Gstream);
			putc('>', Gstream);
			fputs(to->name, Gstream);
			putc('\n', Gstream);
		} else {
			/*
			 * host -> net -- just cache it for now.
			 * first check for dups.  (quadratic, but
			 * n is small here.)
			 */
			while (to->root && to != to->root)
				to = to->root;
			for (ll = lnet; ll; ll = ll->next)
				if (strcmp(ll->to->name, to->name) ==
				    0)
					break;
			if (ll)
				continue;	// dup
			ll = newlink();
			ll->next = lnet;
			ll->to = to;
			lnet = ll;
		}
	}

	// dump nets
	if (lnet) {
		// nets -- print host@\tnet,net, ...
		fputs(from->name, Gstream);
		putc('@', Gstream);
		putc('\t', Gstream);
		for (ll = lnet; ll; ll = lnext) {
			lnext = ll->next;
			fputs(ll->to->name, Gstream);
			if (lnext)
				fputc(',', Gstream);
			freelink(ll);
		}
		putc('\n', Gstream);
	}
}

/*
 * remove cycles in net definitions.
 *
 * depth-first search
 *
 * for each net, run dfs on its neighbors (nets only).  if we return to
 * a visited node, that's a net cycle.  mark the cycle with a pointer
 * in the root field (which gets us closer to the root of this
 * portion of the dfs tree).
 */
static void
untangle(void)
{
	long i;
	Node *n;

	for (i = Hashpart; i < Tabsize; i++) {
		n = Table[i];
		if (n == 0 || (n->flag & NNET) == 0 || n->root)
			continue;
		dfs(n);
	}
}

static void
dfs(Node *n)
{
	Link *l;
	Node *next;

	n->flag |= INDFS;
	n->root = n;
	for (l = n->link; l; l = l->next) {
		next = l->to;
		if ((next->flag & NNET) == 0)
			continue;
		if ((next->flag & INDFS) == 0) {
			dfs(next);
			if (next->root != next)
				n->root = next->root;
		} else
			n->root = next->root;
	}
	n->flag &= ~INDFS;
}

void
showlinks(void)
{
	Link *l;
	Node *n;
	long i;
	FILE *estream;

	if ((estream = fopen(Linkout, "w")) == 0)
		return;

	for (i = Hashpart; i < Tabsize; i++) {
		n = Table[i];
		if (n == 0 || n->link == 0)
			continue;
		for (l = n->link; l; l = l->next) {
			fputs(n->name, estream);
			putc('\t', estream);
			if (NETDIR(l) == LRIGHT)
				putc(NETCHAR(l), estream);
			fputs(l->to->name, estream);
			if (NETDIR(l) == LLEFT)
				putc(NETCHAR(l), estream);
			fprintf(estream, "(%ld)\n", l->cost);
		}
	}
	(void)fclose(estream);
}

/*
 * n is node in heap, newp is candidate for new parent.
 * choose between newp and n->parent for parent.
 * return 0 to use newp, non-zero o.w.
 */
#define NEWP 0
#define OLDP 1
int
tiebreaker(Node *n, Node *newp)
{
	char *opname, *npname, *name;
	Node *oldp;
	int metric;

	oldp = n->parent;

	// given the choice, avoid gatewayed nets
	if (GATEWAYED(newp) && !GATEWAYED(oldp))
		return OLDP;
	if (!GATEWAYED(newp) && GATEWAYED(oldp))
		return NEWP;

	// look at real parents, not nets
	while ((oldp->flag & NNET) && oldp->parent)
		oldp = oldp->parent;
	while ((newp->flag & NNET) && newp->parent)
		newp = newp->parent;

	// use fewer hops, if possible
	metric = height(oldp) - height(newp);
	if (metric < 0)
		return OLDP;
	if (metric > 0)
		return NEWP;

	/*
	 * compare names
	 */
	opname = oldp->name;
	npname = newp->name;
	name = n->name;

	// use longest common prefix with parent
	while (*opname == *name && *npname == *name && *name) {
		opname++;
		npname++;
		name++;
	}
	if (*opname == *name)
		return OLDP;
	if (*npname == *name)
		return NEWP;

	// use shorter host name
	metric = strlen(opname) - strlen(npname);
	if (metric < 0)
		return OLDP;
	if (metric > 0)
		return NEWP;

	// use larger lexicographically
	metric = strcmp(opname, npname);
	if (metric < 0)
		return NEWP;
	return OLDP;
}

static int
height(Node *n)
{
	int i = 0;

	if (n == 0)
		return 0;
	while ((n = n->parent) != 0)
		if (ISADOMAIN(n) || !(n->flag & NNET))
			i++;
	return i;
}

/*
 * return a copy of n ( == l->to).  we rely on n and its copy
 * pointing to the same name string, which is kludgey, but works
 * because the name is non-volatile.
 */

#define REUSABLE(n, l)	(((n)->flag & NTERMINAL) == 0 \
		      && ((n)->copy->flag & NTERMINAL) \
		      && !((n)->copy->flag & NALIAS) \
		      && !((l)->flag & LALIAS))
Node *
ncopy(Node *parent, Link *l)
{
	Node *n, *ncp;

	vprint(stderr, "<%s> <- %s\n", l->to->name, parent->name);
	n = l->to;
	if (REUSABLE(n, l)) {
		Nlink++;
		return n->copy;	// re-use
	}
	NumNcopy++;
	l->to = ncp = newnode();
	ncp->name = n->name;	// nonvolatile
	ncp->tloc = --Hashpart;	// better not be > 20% of total ...
	if (Hashpart == Nheap)
		die("too many terminal links");
	Table[Hashpart] = ncp;
	ncp->copy = n->copy;	// circular list
	n->copy = ncp;
	ncp->link = lcopy(parent, n);
	ncp->flag =
	    (n->
	    flag & ~(NALIAS | ATSIGN | MAPPED | HASLEFT | HASRIGHT)) |
	    NTERMINAL;
	return ncp;
}

/*
 * copy n's links but don't copy any terminal links
 * since n is itself at the end of a terminal link.
 *
 * this is a little messier than it should be, because
 * it wants to be recursive, but the recursion might be
 * very deep (for a long link list), so it iterates.
 *
 * why copy any links other than aliases?  hmmm ...
 */
static Link *
lcopy(Node *parent, Node *n)
{
	Link *l, *lcp;
	Link *first = 0, *last = 0;

	for (l = n->link; l != 0; l = l->next) {
		// skip if dest is already mapped
		if ((l->to->flag & MAPPED) != 0)
			continue;
		// don't copy terminal links
		if ((l->flag & LTERMINAL) != 0)
			continue;
		// comment needed
		if (ALTEREGO(l->to, parent))
			continue;
		vprint(stderr, "\t-> %s\n", l->to->name);
		NumLcopy++;
		lcp = newlink();
		*lcp = *l;	// struct copy
		lcp->flag &= ~LTREE;
		if (ISANET(n))
			lcp->flag |= LTERMINAL;

		if (first == 0) {
			first = last = lcp;
		} else {
			last->next = lcp;
			last = lcp;
		}
	}
	if (last)
		last->next = 0;
	return first;
}
