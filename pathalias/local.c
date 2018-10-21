#include <unistd.h>

#include "def.h"
#include "fns.h"

char *
local(void)
{
	static char lname[64];

	gethostname(lname, sizeof(lname));
	lname[sizeof(lname) - 1] = 0;
	return lname;
}
