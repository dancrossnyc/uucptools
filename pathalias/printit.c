/*
 * pathalias -- by steve bellovin, as told to peter honeyman
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "def.h"
#include "fns.h"

/*
 * print the routes by traversing the shortest path tree in preorder.
 * use lots of char bufs -- profiling indicates this costs about 5 kbytes
 */

/* privates */
static void preorder(Link *l, char *ppath);
static void setpath(Link *l, char *ppath, char *npath, size_t nlen);
static void printhost(Node *n, char *path, Cost cost);
static void printdomain(Node *n, char *path, Cost cost);
static size_t hostpath(char *path, size_t len, Link *l, int netchar);
static int printable(Node *n);

static Link *Ancestor;		/* for -f option */

/* in practice, even the longest paths are < 100 bytes */
#define UUPATHSIZE 512

void
printit(void)
{
	Link *l;
	char pbuf[UUPATHSIZE];

	/* print home */
	if (Cflag)
		printf("%ld\t", (long)Home->cost);
	printf("%s\t%%s\n", Home->name);

	memmove(pbuf, "%s", sizeof "%s");
	for (l = Home->link; l; l = l->next) {
		if (l->flag & LTREE) {
			l->flag &= ~LTREE;
			Ancestor = l;
			preorder(l, pbuf);
			memmove(pbuf, "%s", sizeof "%s");
		}
	}
	fflush(stdout);
	fflush(stderr);
}

/*
 * preorder traversal of shortest path tree.
 */
static void
preorder(Link *l, char *ppath)
{
	Node *n;
	Node *ncp;		/* circular copy list */
	Cost cost;
	char npath[UUPATHSIZE];
	short p_dir;		/* DIR bits of parent (for nets) */
	char p_op;		/* net op of parent (for nets) */

	setpath(l, ppath, npath, sizeof npath);
	n = l->to;
	if (printable(n)) {
		if (Fflag)
			cost = Ancestor->to->cost;
		else
			cost = n->cost;
		if (ISADOMAIN(n))
			printdomain(n, npath, cost);
		else if (!(n->flag & NNET)) {
			printhost(n, npath, cost);
		}
		n->flag |= PRINTED;
		for (ncp = n->copy; ncp != n; ncp = ncp->copy)
			ncp->flag |= PRINTED;
	}

	/* prepare routing bits for domain members */
	p_dir = l->flag & LDIR;
	p_op = l->netop;

	/* recursion */
	for (l = n->link; l; l = l->next) {
		if (!(l->flag & LTREE))
			continue;
		/* network member inherits the routing syntax of its gateway */
		if (ISANET(n)) {
			l->flag = (l->flag & ~LDIR) | p_dir;
			l->netop = p_op;
		}
		l->flag &= ~LTREE;
		preorder(l, npath);
	}
}

static int
printable(Node *n)
{
	Node *ncp;
	Link *l;

	if (n->flag & PRINTED)
		return 0;

	/* is there a cheaper copy? */
	for (ncp = n->copy; n != ncp; ncp = ncp->copy) {
		if (!(ncp->flag & MAPPED))
			continue;	/* unmapped copy */

		if (n->cost > ncp->cost)
			return 0;	/* cheaper copy */

		if (n->cost == ncp->cost && !(ncp->flag & NTERMINAL))
			return 0;	/* synthetic copy */
	}

	/* will a domain route suffice? */
	if (Dflag && !ISANET(n) && ISADOMAIN(n->parent)) {
		/*
		 * are there any interesting links?  a link
		 * is interesting if it doesn't point back
		 * to the parent, and is not an alias.
		 */

		/* check n */
		for (l = n->link; l; l = l->next) {
			if (l->to == n->parent)
				continue;
			if (!(l->flag & LALIAS))
				return 1;
		}

		/* check copies of n */
		for (ncp = n->copy; ncp != n; ncp = ncp->copy) {
			for (l = ncp->link; l; l = l->next) {
				if (l->to == n->parent)
					continue;
				if (!(l->flag & LALIAS))
					return 1;
			}
		}

		/* domain route suffices */
		return 0;
	}
	return 1;
}

// XXX: The string handling here is suspect.
static void
setpath(Link *l, char *ppath, char *npath, size_t nlen)
{
	Node *next, *parent;
	size_t hplen;
	char netchar;

	next = l->to;
	parent = next->parent;
	netchar = NETCHAR(l);

	/* for magic @->% conversion */
	if (parent->flag & ATSIGN)
		next->flag |= ATSIGN;

	/*
	 * i've noticed that distant gateways to domains frequently get
	 * ...!gateway!user@dom.ain wrong.  ...!gateway!user%dom.ain
	 * seems to work, so if next is a domain and the parent is
	 * not the local host, force a magic %->@ conversion.  in this
	 * context, "parent" is the nearest ancestor that is not a net.
	 */
	while (ISANET(parent))
		parent = parent->parent;
	if (ISADOMAIN(next) && parent != Home)
		next->flag |= ATSIGN;

	/*
	 * special handling for nets (including domains) and aliases.
	 * part of the trick is to treat aliases to domains as 0 cost
	 * links.  (the author believes they should be declared as such
	 * in the input, but the world disagrees).
	 */
	if (ISANET(next) || ((l->flag & LALIAS) && !ISADOMAIN(parent))) {
		hplen = strlen(ppath);
		if (hplen >= nlen)
			die("setpath: net name too long");
		memmove(npath, ppath, hplen + 1);
		return;
	}

	if (netchar == '@') {
		if (next->flag & ATSIGN)
			netchar = '%';	/* shazam?  shaman? */
		else
			next->flag |= ATSIGN;
	}

	/* remainder should be a sprintf -- foo on '%' as an operator */
	for (; nlen > 0 && (*npath = *ppath) != 0; ppath++) {
		if (*ppath != '%') {
			npath++;
			nlen--;
			continue;
		}
		switch (ppath[1]) {
		case 's':
			ppath++;
			hplen = hostpath(npath, nlen, l, netchar);
			npath += hplen;
			nlen -= hplen;
			break;
		case '%':		// XXX: This looks broken.
			if (nlen > 2) {
				*++npath = *++ppath;
				npath++;
				nlen -= 2;
			}
			break;
		default:
			die("unknown escape in setpath");
			break;
		}
	}
}

// XXX: The string handling here is highly suspect.
static size_t
hostpath(char *path, size_t len, Link *l, int netchar)
{
	Node *prev;
	size_t r, ll;
	char tmp[16];

	r = 0;
	prev = l->to->parent;
	if (NETDIR(l) == LLEFT) {
		/* host!%s */
		ll = strlen(l->to->name);
		if (ll >= len)
			die("hostpath: name too long");
		memmove(path, l->to->name, ll);
		len -= ll;
		path += ll;
		r = ll;
		while (ISADOMAIN(prev)) {
			ll = strlen(prev->name);
			if (ll >= len)
				die("hostname: name too long");
			memmove(path, prev->name, ll);
			len -=ll;
			path += ll;
			r += ll;
			prev = prev->parent;
		}
		snprintf(tmp, sizeof tmp, "%s%%s", netchar == '%' ? "%" : "");
		ll = strlen(tmp);
		if (len <= ll)
			die("hostpath: name too long");
		memmove(path, tmp, ll + 1);
		r += ll;
	} else {
		/* %s@host */
		snprintf(tmp, sizeof tmp, "%%s%s", (netchar == '%') ? "%" : "");
		ll = strlen(tmp);
		if (len <= ll)
			die("hostpath: name too long");
		memmove(path, tmp, ll);
		len -= ll;
		path += ll;
		r += ll;

		ll = strlen(l->to->name);
		if (len <= ll)
			die("hostpath: name too long");
		memmove(path, l->to->name, ll);
		len -= ll;
		path += ll;
		r += ll;

		while (ISADOMAIN(prev)) {
			ll = strlen(prev->name);
			if (ll >= len)
				die("hostname: name too long");
			memmove(path, prev->name, ll);
			len -=ll;
			path += ll;
			r += ll;
			prev = prev->parent;
		}
	}

	return r;
}

static void
printhost(Node *n, char *path, Cost cost)
{
	if (n->flag & PRINTED)
		die("printhost called twice");
	n->flag |= PRINTED;
	/* skip private hosts */
	if ((n->flag & ISPRIVATE) == 0) {
		if (Cflag)
			printf("%ld\t", (long)cost);
		fputs(n->name, stdout);
		putchar('\t');
		puts(path);
	}
}

static void
printdomain(Node *n, char *path, Cost cost)
{
	Node *p;

	if (n->flag & PRINTED)
		die("printdomain called twice");
	n->flag |= PRINTED;

	/*
	 * print a route for dom.ain if it is a top-level domain, unless
	 * it is private.
	 *
	 * print a route for sub.dom.ain only if all its ancestor dom.ains
	 * are private and sub.dom.ain is not private.
	 */
	if (!ISADOMAIN(n->parent)) {
		/* top-level domain */
		if (n->flag & ISPRIVATE) {
			vprint(stderr,
			    "ignoring private top-level domain %s\n",
			    n->name);
			return;
		}
	} else {
		/* subdomain */
		for (p = n->parent; ISADOMAIN(p); p = p->parent)
			if (!(p->flag & ISPRIVATE))
				return;
		if (n->flag & ISPRIVATE)
			return;
	}

	/* print it (at last!) */
	if (Cflag)
		printf("%ld\t", (long)cost);
	do {
		fputs(n->name, stdout);
		n = n->parent;
	} while (ISADOMAIN(n));
	putchar('\t');
	puts(path);
}
