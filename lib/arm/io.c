/*
 * Each architecture must implement puts() and exit() with the I/O
 * devices exposed from QEMU, e.g. pl011 and virtio-testdev. That's
 * what's done here, along with initialization functions for those
 * devices.
 *
 * Copyright (C) 2014, Red Hat Inc, Andrew Jones <drjones@redhat.com>
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.
 */
#include "libcflat.h"
#include "devicetree.h"
#include "virtio-testdev.h"
#include "asm/spinlock.h"
#include "asm/io.h"

extern void halt(int code);

/*
 * Use this guess for the pl011 base in order to make an attempt at
 * having earlier printf support. We'll overwrite it with the real
 * base address that we read from the device tree later.
 */
#define QEMU_MACH_VIRT_PL011_BASE 0x09000000UL

static struct spinlock uart_lock;
static volatile u8 *uart0_base = (u8 *)QEMU_MACH_VIRT_PL011_BASE;

static void uart0_init(void)
{
	const char *compatible = "arm,pl011";
	dt_pbus_addr_t base;
	int ret;

	ret = dt_pbus_get_baseaddr_compatible(compatible, &base);
	assert(ret == 0 || ret == -FDT_ERR_NOTFOUND);

	if (ret == 0) {
		assert(sizeof(long) == 8 || !(base >> 32));
		uart0_base = (u8 *)(unsigned long)base;
	} else {
		printf("%s: %s not found in the device tree, aborting...\n",
			__func__, compatible);
		abort();
	}
}

void io_init(void)
{
	uart0_init();
	virtio_testdev_init();
}

void puts(const char *s)
{
	spin_lock(&uart_lock);
	while (*s)
		writel(*s++, uart0_base);
	spin_unlock(&uart_lock);
}

void exit(int code)
{
	virtio_testdev_exit(code);
	halt(code);
}
