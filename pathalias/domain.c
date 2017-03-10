/* pathalias -- by steve bellovin, as told to peter honeyman */
#ifndef lint
static char *sccsid = "@(#)domain.c	9.5 92/08/25";
#endif				/* lint */

#include "def.h"

/* imports */
extern dom *newdom();
extern char *strsave();
extern int errno, Vflag;

/* exports */

/* privates */
static Dom *good, *bad;

/*
 * good and bad are passed by reference for move-to-front
 */
int
isadomain(char *domain)
{

	if (ondomlist(&good, domain)) {
		vprintf(stderr, "%s on\n", domain);
		return 1;
	}

	if (ondomlist(&bad, domain)) {
		vprintf(stderr, "%s off\n", domain);
		return 0;
	}

	if (nslookup(domain)) {
		adddom(&good, domain);
		vprintf(stderr, "%s add\n", domain);
		return 1;
	} else {
		adddom(&bad, domain);
		vprintf(stderr, "%s del\n", domain);
		return 0;
	}
}

int
ondomlist(Dom **headp, char *domain)
{
	Dom *d, *head = *headp;

	for (d = head; d != 0; d = d->next) {
		if (strcmp(d->name, domain) == 0) {
			if (d != head)
				movetofront(headp, d);
			return 1;
		}
	}
	return 0;
}



int
adddom(Dom **headp, char *domain)
{
	Dom *d, *head = *headp;

	d = newdom();
	d->next = head;
	d->name = strsave(domain);
	if (d->next)
		d->next->prev = d;
	*headp = d;
}

int
movetofront(Dom **headp, Dom *d)
{
	Dom *head = *headp;

	if (d->prev)
		d->prev->next = d->next;
	if (d->next)
		d->next->prev = d->prev;
	if (head)
		head->prev = d;
	d->next = head;
	*headp = d;
}

#ifdef RESOLVER
#include <sys/types.h>
#include <arpa/nameser.h>

int
nslookup(char *domain)
{
	HEADER *hp;
	int n;
	char q[PACKETSZ], a[PACKETSZ];	/* query, answer */
	char buf[PACKETSZ + 1];

	if ((n = strlen(domain)) >= PACKETSZ)
		return 0;
	strcpy(buf, domain);
	if (buf[n - 1] != '.') {
		buf[n++] = '.';
		buf[n] = 0;
	}
	if ((n =
	    res_mkquery(QUERY, buf, C_IN, T_ANY, (char *)0, 0,
	    (struct rrec *)0, q, sizeof(q))) < 0)
		die("impossible res_mkquery error");
	errno = 0;
	if ((n = res_send(q, n, a, sizeof(a))) < 0)
		die("res_send");
	hp = (HEADER *) a;
	if (hp->rcode == NOERROR)
		return 1;
	return 0;
}
#else				/*!RESOLVER */
/*ARGSUSED*/
int
nslookup(char *domain)
{
	return 0;		/* i guess !?! */
}
#endif /*RESOLVER*/
