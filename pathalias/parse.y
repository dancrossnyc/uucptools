%{
/*
 * pathalias -- by steve bellovin, as told to peter honeyman
 */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "def.h"
#include "fns.h"

/* scanner states (yylex, parse) */
#define OTHER		0
#define COSTING		1
#define NEWLINE		2
#define FILENAME	3

/* exports */
long Tcount;

/* privates */
static void fixnet(Node *network, Node *nlist, Cost cost, char netchar, char netdir);
static void adjust(Node *n, Cost cost);
static int yylex(void);
static int yywrap(void);
static int getword(char *str, int c);
static int Scanstate = NEWLINE;	/* scanner (yylex) state */

/* flags for ys_flags */
#define TERMINAL 1
%}

%union {
	Node	*y_node;
	Cost	y_cost;
	char	y_net;
	char	*y_name;
	struct {
		Node *ys_node;
		Cost ys_cost;
		short ys_flag;
		char ys_net;
		char ys_dir;
	} y_s;
}

%type <y_s>	site asite
%type <y_node>	links aliases plist network nlist host nhost
%type <y_node>	usite delem dlist
%type <y_cost>	cost cexpr

%token <y_name>	SITE HOST STRING
%token <y_cost>	COST
%token <y_net>	NET
%token EOL PRIVATE DEAD DELETE FILETOK ADJUST

%left	'+' '-'
%left	'*' '/'

%%
map	:	/* empty */
	|	map		EOL
	|	map links	EOL
	|	map aliases	EOL
	|	map network	EOL
	|	map private	EOL
	|	map dead	EOL
	|	map delete	EOL
	|	map file	EOL
	|	map adjust	EOL
	|	error		EOL
	;

links	: host site cost {
		Link *l;

		l = addlink($1, $2.ys_node, $3, $2.ys_net, $2.ys_dir);
		if (GATEWAYED($2.ys_node))
			l->flag |= LGATEWAY;
		if ($2.ys_flag & TERMINAL)
			l->flag |= LTERMINAL;
	  }			
	| links ',' site cost {
		Link *l;

		l = addlink($1, $3.ys_node, $4, $3.ys_net, $3.ys_dir);
		if (GATEWAYED($3.ys_node))
			l->flag |= LGATEWAY;
		if ($3.ys_flag & TERMINAL)
			l->flag |= LTERMINAL;
	  }
	| links ','	/* benign error */
	;

host	: HOST		{$$ = addnode($1);}
	| PRIVATE	{$$ = addnode("private");}
	| DEAD		{$$ = addnode("dead");}
	| DELETE	{$$ = addnode("delete");}
	| FILETOK	{$$ = addnode("file");}
	| ADJUST	{$$ = addnode("adjust");}
	;

site	: asite {
		$$ = $1;
		$$.ys_net = DEFNET;
		$$.ys_dir = DEFDIR;
	  }
	| NET asite {
		$$ = $2;
		$$.ys_net = $1;
		$$.ys_dir = LRIGHT;
	  }
	| asite NET {
		$$ = $1;
		$$.ys_net = $2;
		$$.ys_dir = LLEFT;
	  }
	;

asite	: SITE {
		$$.ys_node = addnode($1);
		$$.ys_flag = 0;
	  }
	| '<' SITE '>' {
		Tcount++;
		$$.ys_node = addnode($2);
		$$.ys_flag = TERMINAL;
	  }
	;

aliases	: host '=' SITE	{alias($1, addnode($3));}
	| aliases ',' SITE	{alias($1, addnode($3));}
	| aliases ','	/* benign error */
	;

network	: nhost '{' nlist '}' cost	{fixnet($1, $3, $5, DEFNET, DEFDIR);}
	| nhost NET '{' nlist '}' cost	{fixnet($1, $4, $6, $2, LRIGHT);}
	| nhost '{' nlist '}' NET cost	{fixnet($1, $3, $6, $5, LLEFT);}
	;

nhost	: '='		{$$ = 0;	/* anonymous net */}
	| host '='	{$$ = $1;	/* named net */}
	;

nlist	: SITE		{$$ = addnode($1);}
	| nlist ',' SITE {
		Node *n;

		n = addnode($3);
		if (n->net == 0) {
			n->net = $1;
			$$ = n;
		}
	  }
	| nlist ','	/* benign error */
	;
		
private	: PRIVATE '{' plist '}'			/* list of privates */
	| PRIVATE '{' '}'	{fixprivate();}	/* end scope of privates */
	;

plist	: SITE			{addprivate($1)->flag |= ISPRIVATE;}
	| plist ',' SITE	{addprivate($3)->flag |= ISPRIVATE;}
	| plist ','		/* benign error */
	;

dead	: DEAD '{' dlist '}';

dlist	: delem
	| dlist ',' delem
	| dlist ','		/* benign error */
	;

delem	: SITE			{deadlink(addnode($1), (Node *) 0);}
	| usite NET usite	{deadlink($1, $3);}
	;

usite	: SITE	{$$ = addnode($1);} ;	/* necessary unit production */

delete	: DELETE '{' dellist '}';

dellist	: delelem
	| dellist ',' delelem
	| dellist ','		/* benign error */
	;

delelem	: SITE {
		Node *n;

		n = addnode($1);
		deletelink(n, (Node *) 0);
		n->flag |= ISPRIVATE;
		/* reset Home if it's deleted */
		if (n == Home)
			Home = addnode(Home->name);
	  }
	| usite NET usite	{deletelink($1, $3);}
	;

file	: FILETOK '{' {Scanstate = FILENAME;} STRING {Scanstate = OTHER;} '}' {
		Lineno = 0;
		Cfile = strsave($4);
	}

adjust	: ADJUST '{' adjlist '}' ;

adjlist	: adjelem
	| adjlist ',' adjelem
	| adjlist ','		/* benign error */
	;

adjelem	: usite cost	{adjust($1, $2);} ;

cost	: {$$ = DEFCOST;	/* empty -- cost is always optional */}
	| '(' {Scanstate = COSTING;} cexpr {Scanstate = OTHER;} ')'
		{$$ = $3;}
	;

cexpr	: COST
	| '-' cexpr	  {$$ = -$2;}
	| '(' cexpr ')'   {$$ = $2;}
	| cexpr '+' cexpr {$$ = $1 + $3;}
	| cexpr '-' cexpr {$$ = $1 - $3;}
	| cexpr '*' cexpr {$$ = $1 * $3;}
	| cexpr '/' cexpr {
		if ($3 == 0)
			yyerror("zero divisor\n");
		else
			$$ = $1 / $3;
	  }
	;
%%

void
yyerror(char *s)
{
	/* a concession to bsd error(1) */
	fprintf(stderr, "\"%s\", ", Cfile);
	fprintf(stderr, "line %d: %s\n", Lineno, s);
}

/*
 * patch in the costs of getting on/off the network.
 *
 * for each network member on netlist, add links:
 *	network -> member	cost = 0;
 *	member -> network	cost = parameter.
 *
 * if network and member both require gateways, assume network
 * is a gateway to member (but not v.v., to avoid such travesties
 * as topaz!seismo.css.gov.edu.rutgers).
 *
 * note that members can have varying costs to a network, by suitable
 * multiple declarations.  this is a feechur, albeit a useless one.
 */
static void
fixnet(Node *network, Node *nlist, Cost cost, char netchar, char netdir)
{
	Node *member, *nextnet;
	Link *l;
	static int netanon = 0;
	char anon[25];

	if (network == 0) {
		snprintf(anon, sizeof anon, "[unnamed net %d]", netanon++);
		network = addnode(anon);
	}
	network->flag |= NNET;

	/* insert the links */
	for (member = nlist ; member; member = nextnet) {

		/* network -> member, cost is 0 */
		l = addlink(network, member, (Cost) 0, netchar, netdir);
		if (GATEWAYED(network) && GATEWAYED(member))
			l->flag |= LGATEWAY;

		/* member -> network, cost is parameter */
		/* never ever ever crawl up from a domain*/
		if (!ISADOMAIN(network))
			(void) addlink(member, network, cost, netchar, netdir);

		nextnet = member->net;
		member->net = 0;	/* clear for later use */
	}
}

/* scanner */

#define QUOTE '"'
#define STR_EQ(s1, s2) (s1[2] == s2[2] && strcmp(s1, s2) == 0)
#define NLRETURN() {Scanstate = NEWLINE; return EOL;}

static struct ctable {
	char *cname;
	Cost cval;
} ctable[] = {
	/* ordered by frequency of appearance in a "typical" dataset */
	{"DIRECT", 200},
	{"DEMAND", 300},
	{"DAILY", 5000},
	{"HOURLY", 500},
	{"DEDICATED", 100},
	{"EVENING", 2000},
	{"LOCAL", 25},
	{"LOW", 5},	/* baud rate, quality penalty */
	{"DEAD", MILLION},
	{"POLLED", 5000},
	{"WEEKLY", 30000},
	{"HIGH", -5},	/* baud rate, quality bonus */
	{"FAST", -80},	/* high speed (>= 9.6 kbps) modem */
	/* deprecated */
	{"ARPA", 100},
	{"DIALED", 300},
	{0, 0}
};

static int
yylex()
{
	static char retbuf[128];	/* for return to yacc part */
	int c;
	char *buf = retbuf;
	struct ctable *ct;
	Cost cost;
	char errbuf[128];

	if (feof(stdin) && yywrap())
		return EOF;

	/* count lines, skip over space and comments */
	if ((c = getchar()) == EOF)
		NLRETURN();
    
continuation:
	while (c == ' ' || c == '\t')
		if ((c = getchar()) == EOF)
			NLRETURN();

	if (c == '#')
		while ((c = getchar()) != '\n')
			if (c == EOF)
				NLRETURN();

	/* scan token */
	if (c == '\n') {
		Lineno++;
		if ((c = getchar()) != EOF) {
			if (c == ' ' || c == '\t')
				goto continuation;
			ungetc(c, stdin);
		}
		NLRETURN();
	}

	switch(Scanstate) {
	case COSTING:
		if (isdigit(c)) {
			cost = c - '0';
			for (c = getchar(); isdigit(c); c = getchar())
				cost = (cost * 10) + c - '0';
			ungetc(c, stdin);
			yylval.y_cost = cost;
			return COST;
		}

		if (getword(buf, c) == 0) {
			for (ct = ctable; ct->cname; ct++)
				if (STR_EQ(buf, ct->cname)) {
					yylval.y_cost = ct->cval;
					return COST;
				}
			snprintf(errbuf, sizeof errbuf,
			    "unknown cost (%s), using default", buf);
			yyerror(errbuf);
			yylval.y_cost = DEFCOST;
			return COST;
		}

		return c;	/* pass the buck */

	case NEWLINE:
		Scanstate = OTHER;
		if (getword(buf, c) != 0)
			return c;
		/*
		 * special purpose tokens.
		 *
		 * the "switch" serves the dual-purpose of recognizing
		 * unquoted tokens only.
		 */
		switch(c) {
		case 'p':
			if (STR_EQ(buf, "private"))
				return PRIVATE;
			break;
		case 'd':
			if (STR_EQ(buf, "dead"))
				return DEAD;
			if (STR_EQ(buf, "delete"))
				return DELETE;
			break;
		case 'f':
			if (STR_EQ(buf, "file"))
				return FILETOK;
			break;
		case 'a':
			if (STR_EQ(buf, "adjust"))
				return ADJUST;
			break;
		}

		yylval.y_name = buf;
		return HOST;

	case FILENAME:
		while (c != EOF && isprint(c)) {
			if (c == ' ' || c == '\t' || c == '\n' || c == '}')
				break;
			*buf++ = c;
			c = getchar();
		}
		if (c != EOF)
			ungetc(c, stdin);
		*buf = 0;
		yylval.y_name = retbuf;
		return STRING;
	}

	if (getword(buf, c) == 0) {
		yylval.y_name = buf;
		return SITE;
	}

	if (strchr(Netchars, c) != NULL) {
		yylval.y_net = c;
		return NET;
	}

	return c;
}

/*
 * fill str with the next word in [0-9A-Za-z][-._0-9A-Za-z]+ or a quoted
 * string that contains no newline.  return -1 on failure or EOF, 0 o.w.
 */ 
static int
getword(char *str, int c)
{
	if (c == QUOTE) {
		while ((c = getchar()) != QUOTE) {
			if (c == '\n') {
				yyerror("newline in quoted string\n");
				ungetc(c, stdin);
				return -1;
			}
			if (c == EOF) {
				yyerror("EOF in quoted string\n");
				return -1;
			}
			*str++ = c;
		}
		*str = 0;
		return 0;
	}

	/* host name must start with alphanumeric or `.' */
	if (!isalnum(c) && c != '.')
		return -1;

yymore:
	do {
		*str++ = c;
		c = getchar();
	} while (isalnum(c) || c == '.' || c == '_');

	if (c == '-' && Scanstate != COSTING)
		goto yymore;

	ungetc(c, stdin);
	*str = 0;
	return 0;
}

static int
yywrap()
{	char errbuf[100];

	fixprivate();	/* munge private host definitions */
	Lineno = 1;
	while (optind < Argc) {
		if (freopen((Cfile = Argv[optind++]), "r", stdin) != 0)
			return 0;
		snprintf(errbuf, sizeof errbuf, "%s: %s", Argv[0], Cfile);
		perror(errbuf);
	}
	freopen("/dev/null", "r", stdin);
	return -1;
}

static void
adjust(Node *n, Cost cost)
{
	Link *l;

	n->cost += cost;	/* cumulative */

	/* hit existing links */
	for (l = n->link; l; l = l->next) {
		if ((l->cost += cost) < 0) {
			char buf[100];

			l->flag |= LDEAD;
			snprintf(buf, sizeof buf,
			    "link to %s deleted with negative cost",
			    l->to->name);
			yyerror(buf);
		}
	}
}
