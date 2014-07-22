#ifndef _UTIL_H_
#define _UTIL_H_

char testname[64];
extern void testname_set(const char *group, const char *subtest);
extern void assert_args(int num_args, int needed_args);

#endif /* _UTIL_H_ */
