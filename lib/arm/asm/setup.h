#ifndef _ASMARM_SETUP_H_
#define _ASMARM_SETUP_H_
/*
 * Copyright (C) 2014, Red Hat Inc, Andrew Jones <drjones@redhat.com>
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.
 */
#include "libcflat.h"

#define NR_CPUS			8
extern u32 cpus[NR_CPUS];
extern int nr_cpus;

typedef u64 phys_addr_t;

#define NR_MEMREGIONS		16
struct memregion {
	phys_addr_t addr;
	phys_addr_t size;
	bool free;
};

extern struct memregion memregions[NR_MEMREGIONS];
extern int nr_memregions;

extern struct memregion *memregion_new(phys_addr_t size);
extern void memregions_show(void);

#define PHYS_OFFSET		({ memregions[0].addr; })
#define PHYS_SHIFT		40
#define PHYS_SIZE		(1ULL << PHYS_SHIFT)
#define PHYS_MASK		(PHYS_SIZE - 1ULL)

#define PAGE_SHIFT		12
#define PAGE_SIZE		(1UL << PAGE_SHIFT)
#define PAGE_MASK		(~(PAGE_SIZE - 1UL))
#define PAGE_ALIGN(addr)	(((addr) + (PAGE_SIZE-1UL)) & PAGE_MASK)

#endif /* _ASMARM_SETUP_H_ */
