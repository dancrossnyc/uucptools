/* addlink.c */
Link *addlink(Node *from, Node *to, Cost cost, int netchar, int netdir);
void deadlink(Node *nleft, Node *nright);
int tracelink(char *arg);
void atrace(Node *n1, Node *n2);
int maptrace(Node *from, Node *to);
void deletelink(Node *from, Node *to);
/* addnode.c */
Node *addnode(char *name);
void alias(Node *n1, Node *n2);
Node *addhidden(char *name);
void fixprivate(void);
Node *addprivate(char *name);
/* domain.c */
int isadomain(char *domain);
int ondomlist(Dom **headp, char *domain);
int adddom(Dom **headp, char *domain);
int movetofront(Dom **headp, Dom * d);
int nslookup(char *domain);
/* local.c */
char *local(void);
/* makedb.c */
int main(int argc, char *argv[]);
int dbfile(char *dbf);
int dbcreat(char *dbf, char *suffix);
int makedb(char *ifile);
int perror_(char *str);
/* mapaux.c */
long pack(long low, long high);
void resetnodes(void);
void dumpgraph(void);
void showlinks(void);
int tiebreaker(Node *n, Node *newp);
Node *ncopy(Node *parent, Link *l);
/* mapit.c */
void mapit(void);
/* mem.c */
Link *newlink(void);
void freelink(Link *l);
Node *newnode(void);
Dom *newdom(void);
char *strsave(char *s);
Node **newtable(long size);
void freetable(Node **t, long size);
long allocation(void);
void wasted(void);
/* printit.c */
void printit(void);
