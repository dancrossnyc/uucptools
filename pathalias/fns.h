/* addlink.c */
link *addlink(node * from, node * to, Cost cost, char netchar,
char netdir);
void deadlink(node * nleft, node * nright);
int tracelink(char *arg);
void atrace(node * n1, node * n2);
int maptrace(node * from, node * to);
void deletelink(node * from, node * to);
/* addnode.c */
node *addnode(char *name);
void alias(node * n1, node * n2);
void hashanalyze(void);
node *addhidden(char *name);
void fixprivate(void);
node *addprivate(char *name);
/* domain.c */
int isadomain(char *domain);
int ondomlist(dom ** headp, char *domain);
int adddom(dom ** headp, char *domain);
int movetofront(dom ** headp, dom * d);
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
int tiebreaker(node * n, node * newp);
node *ncopy(node * parent, link * l);
/* mapit.c */
void mapit(void);
/* mem.c */
link *newlink(void);
void freelink(link * l);
node *newnode(void);
dom *newdom(void);
char *strsave(char *s);
node **newtable(long size);
void freetable(node ** t, long size);
long allocation(void);
void wasted(void);
/* printit.c */
void printit(void);
