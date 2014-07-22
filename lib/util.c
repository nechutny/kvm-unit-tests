#include "util.h"
#include "libcflat.h"

void testname_set(const char *group, const char *subtest)
{
	strcpy(testname, group);
	if (subtest) {
		strcat(testname, "::");
		strcat(testname, subtest);
	}
}

void assert_args(int num_args, int needed_args)
{
	if (num_args < needed_args) {
		printf("%s: not enough arguments\n", testname);
		abort();
	}
}
