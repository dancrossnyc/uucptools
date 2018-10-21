/*
 * pathalias -- by steve bellovin, as told to peter honeyman
 */
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>

#include <errno.h>
#include <resolv.h>
#include <string.h>

#include "def.h"
#include "fns.h"

/* privates */
static Dom *good, *bad;

/*
 * good and bad are passed by reference for move-to-front
 */
int
isadomain(char *domain)
{

	if (ondomlist(&good, domain)) {
		vprint(stderr, "%s on\n", domain);
		return 1;
	}

	if (ondomlist(&bad, domain)) {
		vprint(stderr, "%s off\n", domain);
		return 0;
	}

	if (nslookup(domain)) {
		adddom(&good, domain);
		vprint(stderr, "%s add\n", domain);
		return 1;
	} else {
		adddom(&bad, domain);
		vprint(stderr, "%s del\n", domain);
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



void
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

void
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

int
nslookup(char *domain)
{
	HEADER *hp;
	int n;
	unsigned char q[PACKETSZ], a[PACKETSZ];	// query, answer
	char buf[PACKETSZ + 1];

	if ((n = strlen(domain)) >= PACKETSZ)
		return 0;
	memmove(buf, domain, n);
	if (buf[n - 1] != '.')
		buf[n++] = '.';
	buf[n] = '\0';
	n = res_mkquery(QUERY, buf, C_IN, T_ANY, NULL, 0, NULL, q, sizeof q);
	if (n < 0)
		die("impossible res_mkquery error");
	errno = 0;
	n = res_send(q, n, a, sizeof a);
	if (n < 0)
		die("res_send");
	hp = (HEADER *) a;
	if (hp->rcode == NOERROR)
		return 1;

	return 0;
}
