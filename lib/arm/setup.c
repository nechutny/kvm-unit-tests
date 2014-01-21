/*
 * Initialize machine setup information and I/O.
 *
 * After running setup() a flat file knows how many cpus it has
 * (nr_cpus), how much free memory it has, and at what physical
 * address that free memory starts (memregions[1].{addr,size}),
 * printf() and exit() will both work, and (argc, argv) are ready
 * to be passed to main().
 *
 * Copyright (C) 2014, Red Hat Inc, Andrew Jones <drjones@redhat.com>
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.
 */
#include "libcflat.h"
#include "libfdt/libfdt.h"
#include "devicetree.h"
#include "asm/spinlock.h"
#include "asm/setup.h"

extern unsigned long stacktop;
extern void io_init(void);
extern void setup_args(const char *args);

u32 cpus[NR_CPUS] = { [0 ... NR_CPUS-1] = (~0UL) };
int nr_cpus;

static struct spinlock memregion_lock;
struct memregion memregions[NR_MEMREGIONS];
int nr_memregions;

static void cpu_set(int fdtnode __unused, u32 regval, void *info)
{
	unsigned *i = (unsigned *)info;
	cpus[*i] = regval;
	*i += 1;
}

static void cpu_init(void)
{
	unsigned i = 0;
	assert(dt_for_each_cpu_node(cpu_set, &i) == 0);
	nr_cpus = i;
}

static void memregions_init(phys_addr_t freemem_start)
{
	/* we only expect one membank to be defined in the DT */
	struct dt_pbus_reg regs[1];
	phys_addr_t addr, size, mem_end;

	nr_memregions = dt_get_memory_params(regs, 1);

	assert(nr_memregions > 0);

	addr = regs[0].addr;
	size = regs[0].size;
	mem_end = addr + size;

	assert(!(addr & ~PHYS_MASK) && !((mem_end-1) & ~PHYS_MASK));

#ifdef __arm__
	/* TODO: support highmem? */
	assert(!((u64)addr >> 32) && !((u64)(mem_end-1) >> 32));
#endif

	freemem_start = PAGE_ALIGN(freemem_start);

	memregions[0].addr = PAGE_ALIGN(addr);
	memregions[0].size = freemem_start - PHYS_OFFSET;

	memregions[1].addr = freemem_start;
	memregions[1].size = mem_end - freemem_start;
	memregions[1].free = true;
	nr_memregions = 2;
}

struct memregion *memregion_new(phys_addr_t size)
{
	phys_addr_t freemem_start, freemem_size, mem_end;
	struct memregion *m;

	spin_lock(&memregion_lock);

	assert(memregions[nr_memregions-1].free);
	assert(memregions[nr_memregions-1].size >= size);

	mem_end = memregions[nr_memregions-1].addr
		+ memregions[nr_memregions-1].size;

	freemem_start = PAGE_ALIGN(memregions[nr_memregions-1].addr + size);
	freemem_size = mem_end - freemem_start;

	memregions[nr_memregions-1].size = freemem_start
					 - memregions[nr_memregions-1].addr;
	memregions[nr_memregions-1].free = false;

	m = &memregions[nr_memregions-1];

	if (nr_memregions < NR_MEMREGIONS) {
		memregions[nr_memregions].addr = freemem_start;
		memregions[nr_memregions].size = freemem_size;
		memregions[nr_memregions].free = true;
		++nr_memregions;
	}

	spin_unlock(&memregion_lock);

	return m;
}

void memregions_show(void)
{
	int i;
	for (i = 0; i < nr_memregions; ++i)
		printf("%016llx-%016llx [%s]\n",
			memregions[i].addr,
			memregions[i].addr + memregions[i].size - 1,
			memregions[i].free ? "FREE" : "USED");
}

void setup(unsigned long arg __unused, unsigned long id __unused,
	   const void *fdt)
{
	const char *bootargs;
	u32 fdt_size;

	/*
	 * Move the fdt to just above the stack. The free memory
	 * then starts just after the fdt.
	 */
	fdt_size = fdt_totalsize(fdt);
	assert(fdt_move(fdt, &stacktop, fdt_size) == 0);
	assert(dt_init(&stacktop) == 0);

	memregions_init((unsigned long)&stacktop + fdt_size);

	io_init();
	cpu_init();

	assert(dt_get_bootargs(&bootargs) == 0);
	setup_args(bootargs);
}
