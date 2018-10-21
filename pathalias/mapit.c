// pathalias -- by steve bellovin, as told to peter honeyman

#include <stdio.h>

#include "def.h"
#include "fns.h"

#define trprint(stream, n) \
	fprintf((stream), (n)->flag & NTERMINAL ? "<%s>" : "%s", (n)->name)

// globals
// invariant while mapping: Nheap < Hashpart
long Hashpart;			// start of unreached nodes
long Nheap;			// end of heap
long NumNcopy, Nlink, NumLcopy;

// privates
static long Heaphighwater;
static Link **Heap;

static void insert(Link *l);
static void heapup(Link *l);
static void heapdown(Link *l);
static void heapswap(long i, long j);
static void heapchildren(Node *n);
static void backlinks(void);
static void setheapbits(Link *l);
static void mtracereport(Node *from, Link *l, char *excuse);
static void otracereport(Node *n);
static Link *min_node(void);
static int dehash(Node *n);
static int skiplink(Link *l, Node *parent, Cost cost, int trace);
static int skipterminalalias(Node *n, Node *next);
static Cost costof(Node *prev, Link *l);
static Node *mappedcopy(Node *n);

// transform the graph to a shortest-path tree by marking tree edges
void
mapit(void)
{
	Node *n;
	Link *l;

	vprint(stderr, "*** mapping\ttcount = %ld\n", Tcount);
	Tflag = Tflag && Vflag;	// tracing here only if verbose
	// re-use the hash table space for the heap
	Heap = (Link **) Table;
	Hashpart = pack(0L, Tabsize - 1);

	// expunge penalties from -a option and make circular copy lists
	resetnodes();

	if (Linkout && *Linkout)	// dump cheapest links
		showlinks();
	if (Graphout && *Graphout)	// dump the edge list
		dumpgraph();

	// insert Home to get things started
	l = newlink();		// link to get things started
	l->to = Home;
	(void)dehash(Home);
	insert(l);

	// main mapping loop
	do {
		Heaphighwater = Nheap;
		while ((l = min_node()) != 0) {
			l->flag |= LTREE;
			n = l->to;
			if (n->flag & MAPPED)	// sanity check
				die("mapped node in heap");
			if (Tflag && maptrace(n, n))
				otracereport(n);	// tracing
			n->flag |= MAPPED;
			heapchildren(n);	// add children to heap
		}
		vprint(stderr,
		    "heap hiwat %ld\nncopy = %ld, nlink = %ld, lcopy = %ld\n",
		    Heaphighwater, NumNcopy, Nlink, NumLcopy);

		if (Nheap != 0)	// sanity check
			die("null entry in heap");

		//
		// add back links from unreachable hosts to reachable
		// neighbors, then remap.  asymptotically, this is
		// quadratic; in practice, this is done once or twice,
		// when n is small.
		//
		backlinks();
	} while (Nheap > 0);

	if (Hashpart < Tabsize) {
		int foundone = 0;

		for (; Hashpart < Tabsize; Hashpart++) {
			if (Table[Hashpart]->flag & ISPRIVATE)
				continue;
			if (foundone++ == 0)
				fputs("You can't get there from here:\n",
				    stderr);
			putc('\t', stderr);
			trprint(stderr, Table[Hashpart]);
			putc('\n', stderr);
		}
	}
}

static void
heapchildren(Node *n)
{
	Node *next;
	int mtrace;
	Cost cost;

	for (Link *l = n->link; l != NULL; l = l->next) {
		next = l->to;	// neighboring node
		mtrace = Tflag && maptrace(n, next);
		if (l->flag & LTREE)
			continue;
		if (l->flag & LTERMINAL)
			l->to = next = ncopy(n, l);
		if ((n->flag & NTERMINAL) && (l->flag & LALIAS)) {
			if (skipterminalalias(n, next))
				continue;
			else
				l->to = next = ncopy(n, l);
		}
		if (next->flag & MAPPED) {
			if (mtrace)
				mtracereport(n, l, "-\talready mapped");
			continue;
		}
		cost = costof(n, l);
		if (skiplink(l, n, cost, mtrace))
			continue;
		//
		// put this link in the heap and restore the
		// heap property.
		//
		if (mtrace) {
			if (next->parent != NULL)
				mtracereport(next->parent, l, "*\tdrop");
			mtracereport(n, l, "+\tadd");
		}
		next->parent = n;
		if (dehash(next) == 0) {	// first time
			next->cost = cost;
			insert(l);		// insert at end
			heapup(l);
		} else {
			// replace inferior path
			Heap[next->tloc] = l;
			if (cost > next->cost) {
				// increase cost (gateway)
				next->cost = cost;
				heapdown(l);
			} else if (cost < next->cost) {
				// cheaper route
				next->cost = cost;
				heapup(l);
			}
		}
		setheapbits(l);
	}
}

//
// n is a terminal node just sucked out of the heap, next is an alias
// for n.  if n was heaped because of a copy (ALTEREGO) of next, don't
// heap next -- it will happen over and over and over and ...
//
static int
skipterminalalias(Node *n, Node *next)
{

	while (n->flag & NALIAS) {
		n = n->parent;
		if (ALTEREGO(n, next))
			return 1;
	}

	return 0;
}

//
// return 1 if we definitely don't want want this link in the
// shortest path tree, 0 if we might want it, i.e., best so far.
//
// if tracing is turned on, report only if this node is being skipped.
//
static int
skiplink(
    Link *l,			// new link to this node
    Node *parent,		// (potential) new parent of this node
    Cost cost,			// new cost to this node
    int trace			// trace this link?
)
{
	Node *n;	// this node
	Link *lheap;	// old link to this node

	n = l->to;

	// first time we've reached this node?
	if (n->tloc >= Hashpart)
		return 0;

	lheap = Heap[n->tloc];

	// examine links to nets that require gateways
	if (GATEWAYED(n)) {
		// if exactly one is a gateway, use it
		if ((lheap->flag & LGATEWAY) && !(l->flag & LGATEWAY)) {
			if (trace)
				mtracereport(parent, l, "-\told gateway");
			return 1;	// old is gateway
		}
		if (!(lheap->flag & LGATEWAY) && (l->flag & LGATEWAY))
			return 0;	// new is gateway

		// no gateway or both gateways;  resolve in standard way ...
	}

	// examine dup link (sanity check)
	if (n->parent == parent && (DEADLINK(lheap) || DEADLINK(l)))
		die("dup dead link");

	// examine cost
	if (cost < n->cost)
		return 0;
	if (cost > n->cost) {
		if (trace)
			mtracereport(parent, l, "-\tcheaper");
		return 1;
	}

	// all other things being equal, ask the oracle
	if (tiebreaker(n, parent)) {
		if (trace)
			mtracereport(parent, l, "-\ttiebreaker");
		return 1;
	}

	return 0;
}

// compute cost to next (l->to) via prev
static Cost
costof(Node *prev, Link *l)
{
	Node *next;
	Cost cost;

	if (l->flag & LALIAS)
		return prev->cost;	// by definition
	next = l->to;
	cost = prev->cost + l->cost;	// basic cost
	if (cost >= INF)
		return cost + 1;
	//
	// heuristics:
	//    charge for a dead link.
	//    charge for getting past a terminal host
	//      or getting out of a dead host.
	//    charge for getting into a gatewayed net (except at a gateway).
	//    discourage mixing of syntax (when prev is a host).
	//
	// life was simpler when pathalias truly computed shortest paths.
	//
	if (DEADLINK(l))
		cost += INF;		// dead link
	else if (DEADHOST(prev))
		cost += INF;		// dead parent
	else if (GATEWAYED(next) && !(l->flag & LGATEWAY))
		cost += INF;		// not gateway
	else if (!ISANET(prev)) {
		if ((NETDIR(l) == LLEFT && (prev->flag & HASRIGHT)) ||
		    (NETDIR(l) == LRIGHT && (prev->flag & HASLEFT)))
			cost += INF;	// mixed syntax
	}

	return cost;
}

// binary heap implementation of priority queue
static void
insert(Link *l)
{
	Node *n;

	n = l->to;
	if (n->flag & MAPPED)
		die("insert mapped node");

	Heap[n->tloc] = 0;
	if (Heap[Nheap + 1] != NULL)
		die("heap error in insert");
	if (Nheap++ == 0) {
		Heap[1] = l;
		n->tloc = 1;
		return;
	}
	if (Vflag && Nheap > Heaphighwater)
		Heaphighwater = Nheap;	// diagnostics

	// insert at the end.  caller must heapup(l).
	Heap[Nheap] = l;
	n->tloc = Nheap;
}

//
// "percolate" up the heap by exchanging with the parent.  as in
// min_node(), give tiebreaker() a chance to produce better, stable
// routes by moving nets and domains close to the root, nets closer
// than domains.
//
// i know this seems obscure, but it's harmless and cheap.  trust me.
//
static void
heapup(Link *l)
{
	long cindx, pindx;	// child, parent indices
	Cost cost;
	Node *child, *parent;

	child = l->to;
	cost = child->cost;
	for (cindx = child->tloc; cindx > 1; cindx = pindx) {
		pindx = cindx / 2;
		if (Heap[pindx] == NULL)	// sanity check
			die("impossible error in heapup");
		parent = Heap[pindx]->to;
		if (cost > parent->cost)
			return;

		// net/domain heuristic
		if (cost == parent->cost) {
			if (!ISANET(child))
				return;
			if (!ISADOMAIN(parent))
				return;
			if (ISADOMAIN(child))
				return;
		}
		heapswap(cindx, pindx);
	}
}

// extract min (== Heap[1]) from heap
static Link *
min_node(void)
{
	Link *rval, *lastlink;

	if (Nheap == 0)
		return 0;

	rval = Heap[1];	// return this one

	// move last entry into root and reheap
	lastlink = Heap[Nheap];
	Heap[Nheap] = 0;

	if (--Nheap) {
		Heap[1] = lastlink;
		lastlink->to->tloc = 1;
		heapdown(lastlink);	// restore heap property
	}

	return rval;
}

//
// swap Heap[i] with smaller child, iteratively down the tree.
//
// given the opportunity, attempt to place nets and domains
// near the root.  this helps tiebreaker() shun domain routes.
//
static void
heapdown(Link *l)
{
	long pindx, cindx;
	Node *child, *rchild, *parent;

	pindx = l->to->tloc;
	parent = Heap[pindx]->to;	// invariant
	for (; (cindx = pindx * 2) <= Nheap; pindx = cindx) {
		// pick lhs or rhs child
		child = Heap[cindx]->to;
		if (cindx < Nheap) {
			// compare with rhs child
			rchild = Heap[cindx + 1]->to;
			//
			// use rhs child if smaller than lhs child.
			// if equal, use rhs if net or domain.
			//
			if (child->cost > rchild->cost) {
				child = rchild;
				cindx++;
			} else if (child->cost == rchild->cost)
				if (ISANET(rchild)) {
					child = rchild;
					cindx++;
				}
		}

		// child is the candidate for swapping
		if (parent->cost < child->cost)
			break;

		//
		// heuristics:
		//      move nets/domains up
		//      move nets above domains
		//
		if (parent->cost == child->cost) {
			if (!ISANET(child))
				break;
			if (ISANET(parent) && ISADOMAIN(child))
				break;
		}

		heapswap(pindx, cindx);
	}
}

// exchange Heap[i] and Heap[j] pointers
static void
heapswap(long i, long j)
{
	Link *temp;

	temp = Heap[i];
	Heap[i] = Heap[j];
	Heap[j] = temp;
	Heap[j]->to->tloc = j;
	Heap[i]->to->tloc = i;
}

// return 1 if n is already de-hashed (tloc < Hashpart), 0 o.w.
static int
dehash(Node *n)
{
	if (n->tloc < Hashpart)
		return 1;

	// swap with entry in Table[Hashpart]
	Table[Hashpart]->tloc = n->tloc;
	Table[n->tloc] = Table[Hashpart];
	Table[Hashpart] = n;
	n->tloc = Hashpart++;

	return 0;
}

//
// everything reachable has been mapped.  what to do about any
// unreachable hosts?  the sensible thing to do is to dump them on
// stderr and be done with it.  unfortunately, there are hundreds of
// such hosts in the usenet maps.  so we take the low road: for each
// unreachable host, we add a back link from its cheapest mapped child,
// in the faint that a reverse path works.
//
// beats me why people want their error output in their map databases.
//
static void
backlinks(void)
{
	Link *l;
	Node *n, *child;

	// hosts from Hashpart to Tabsize are unreachable
	for (long i = Hashpart; i < Tabsize; i++) {
		Node *nomap = Table[i];
		// if a copy has been mapped, we're ok
		if (nomap->copy != nomap) {
			dehash(nomap);
			Table[nomap->tloc] = NULL;
			nomap->tloc = 0;
			continue;
		}
		// TODO: simplify this
		// add back link from minimal cost child
		child = NULL;
		for (l = nomap->link; l != NULL; l = l->next) {
			n = l->to;
			// never ever ever crawl out of a domain
			if (ISADOMAIN(n))
				continue;
			if ((n = mappedcopy(n)) == NULL)
				continue;
			if (child == NULL) {
				child = n;
				continue;
			}
			if (n->cost > child->cost)
				continue;
			if (n->cost == child->cost) {
				nomap->parent = child;	// for tiebreaker
				if (tiebreaker(nomap, n))
					continue;
			}
			child = n;
		}
		if (child == NULL)
			continue;
		dehash(nomap);
		l = addlink(child, nomap, INF, DEFNET, DEFDIR);	// INF cost
		nomap->parent = child;
		nomap->cost = costof(child, l);
		insert(l);
		heapup(l);
		if (Vflag > 1)
			fprintf(stderr, "backlink: %s <- %s\n",
			    nomap->name, child->name);
	}
	vprint(stderr, "%d backlinks\n", Nheap);
}

// find a mapped copy of n if it exists
static Node *
mappedcopy(Node *n)
{
	Node *ncp;

	if (n->flag & MAPPED)
		return n;
	for (ncp = n->copy; ncp != n; ncp = ncp->copy)
		if (ncp->flag & MAPPED)
			return ncp;

	return 0;
}

//
// l has just been added or changed in the heap,
// so reset the state bits for l->to.
//
static void
setheapbits(Link *l)
{
	Node *n;
	Node *parent;

	n = l->to;
	parent = n->parent;
	n->flag &= ~(NALIAS | HASLEFT | HASRIGHT);	// reset

	// record whether link is an alias
	if (l->flag & LALIAS) {
		n->flag |= NALIAS;
		// TERMINALity is inherited by the alias
		if (parent->flag & NTERMINAL)
			n->flag |= NTERMINAL;
	}

	// set left/right bits
	if (NETDIR(l) == LLEFT || (parent->flag & HASLEFT))
		n->flag |= HASLEFT;
	if (NETDIR(l) == LRIGHT || (parent->flag & HASRIGHT))
		n->flag |= HASRIGHT;
}

static void
mtracereport(Node *from, Link *l, char *excuse)
{
	Node *to = l->to;

	fprintf(stderr, "%-16s ", excuse);
	trprint(stderr, from);
	fputs(" -> ", stderr);
	trprint(stderr, to);
	fprintf(stderr, " (%ld, %ld, %ld) ",
	    from->cost, l->cost, to->cost);
	if (to->parent) {
		trprint(stderr, to->parent);
		fprintf(stderr, " (%ld)", to->parent->cost);
	}
	putc('\n', stderr);
}

static void
otracereport(Node *n)
{
	if (n->parent != NULL)
		trprint(stderr, n->parent);
	else
		fputs("[root]", stderr);
	fputs(" -> ", stderr);
	trprint(stderr, n);
	fputs(" mapped\n", stderr);
}
