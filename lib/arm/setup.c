/*
 * Initialize machine setup information and I/O.
 *
 * After running setup() unit tests may query how many cpus they have
 * (nr_cpus), how much free memory they have, and at what physical
 * address that free memory starts (memregions[1].{addr,size}),
 * printf() and exit() will both work, and (argc, argv) are ready
 * to be passed to main().
 *
 * Copyright (C) 2014, Red Hat Inc, Andrew Jones <drjones@redhat.com>
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.
 */
#include "libcflat.h"
#include "alloc.h"
#include "libfdt/libfdt.h"
#include "devicetree.h"
#include "asm/spinlock.h"
#include "asm/setup.h"
#include "asm/page.h"

#define ALIGN_UP_MASK(x, mask)	(((x) + (mask)) & ~(mask))
#define ALIGN_UP(x, a)		ALIGN_UP_MASK(x, (typeof(x))(a) - 1)

extern unsigned long stacktop;
extern void io_init(void);
extern void setup_args(const char *args);

u32 cpus[NR_CPUS] = { [0 ... NR_CPUS-1] = (~0UL) };
int nr_cpus;

static struct spinlock memregion_lock;
struct memregion memregions[NR_MEMREGIONS];
int nr_memregions;

static void cpu_set(int fdtnode __unused, u32 regval, void *info __unused)
{
	assert(nr_cpus < NR_CPUS);
	cpus[nr_cpus++] = regval;
}

static void cpu_init(void)
{
	nr_cpus = 0;
	assert(dt_for_each_cpu_node(cpu_set, NULL) == 0);
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

	memregions[0].addr = PAGE_ALIGN(addr); /* PHYS_OFFSET */

	assert(freemem_start >= PHYS_OFFSET && freemem_start < mem_end);

	memregions[0].size = freemem_start - PHYS_OFFSET;
	memregions[1].addr = freemem_start;
	memregions[1].size = mem_end - freemem_start;
	memregions[1].free = true;
	nr_memregions = 2;

#ifdef __arm__
	/*
	 * Make sure 32-bit unit tests don't have any surprises when
	 * running without virtual memory, by ensuring the initial
	 * memory region uses 32-bit addresses. Other memory regions
	 * may have > 32-bit addresses though, and the unit tests are
	 * free to do as they wish with that.
	 */
	assert(!(memregions[0].addr >> 32));
	assert(!((memregions[0].addr + memregions[0].size - 1) >> 32));
#endif
}

struct memregion *memregion_new(phys_addr_t size, phys_addr_t align)
{
	phys_addr_t freemem_start, mem_end, addr, size_orig = size;
	struct memregion *mr;

	spin_lock(&memregion_lock);

	mr = &memregions[nr_memregions-1];

	addr = ALIGN_UP(mr->addr, align);
	size += addr - mr->addr;

	if (!mr->free || mr->size < size) {
		printf("%s: requested=0x%llx (align=0x%llx), "
		       "need=0x%llx, but free=0x%llx.\n", __func__,
		       size_orig, align, size, mr->free ? mr->size : 0ULL);
		return NULL;
	}

	mem_end = mr->addr + mr->size;
	freemem_start = mr->addr + size;

	mr->addr = addr;
	mr->size = size_orig;
	mr->free = false;

	if (freemem_start < mem_end && nr_memregions < NR_MEMREGIONS) {
		memregions[nr_memregions].addr = freemem_start;
		memregions[nr_memregions].size = mem_end - freemem_start;
		memregions[nr_memregions].free = true;
		++nr_memregions;
	}

	spin_unlock(&memregion_lock);

	return mr;
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

static void *early_alloc_aligned(size_t size, size_t align)
{
	struct memregion *mr;
	void *addr;

	mr = memregion_new(size, align);
	if (!mr)
		return NULL;

	addr = __va(mr->addr);
	memset(addr, 0, size);

	return addr;
}

static void *early_alloc(size_t size)
{
	phys_addr_t align = size < SMP_CACHE_BYTES ? SMP_CACHE_BYTES : size;
	return early_alloc_aligned(size, align);
}

static void early_free(const void *addr __unused)
{
}

static const struct alloc_ops early_alloc_ops = {
	.alloc = early_alloc,
	.alloc_aligned = early_alloc_aligned,
	.free = early_free,
};

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
	alloc_ops = early_alloc_ops;

	io_init();
	cpu_init();

	assert(dt_get_bootargs(&bootargs) == 0);
	setup_args(bootargs);
}
