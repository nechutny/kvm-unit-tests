#ifndef _ALLOC_H_
#define _ALLOC_H_
#include "libcflat.h"

struct alloc_ops {
	void *(*alloc)(size_t size);
	void *(*alloc_aligned)(size_t size, size_t align);
	void (*free)(const void *addr);
};

extern struct alloc_ops alloc_ops;

static inline void *alloc(size_t size)
{
	assert(alloc_ops.alloc);
	return alloc_ops.alloc(size);
}

static inline void *alloc_aligned(size_t size, size_t align)
{
	assert(alloc_ops.alloc_aligned);
	return alloc_ops.alloc_aligned(size, align);
}

static inline void free(const void *addr)
{
	assert(alloc_ops.free);
	alloc_ops.free(addr);
}

#endif
