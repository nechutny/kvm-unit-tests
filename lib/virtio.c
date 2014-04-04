/*
 * Copyright (C) 2014, Red Hat Inc, Andrew Jones <drjones@redhat.com>
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.
 */
#include "libcflat.h"
#include "devicetree.h"
#include "asm/spinlock.h"
#include "asm/io.h"
#include "virtio.h"

enum virtio_hwdesc_type {
	VIRTIO_HWDESC_TYPE_DT = 0,	/* device tree */
	NR_VIRTIO_HWDESC_TYPES,
};

enum virtio_bus_type {
	VIRTIO_BUS_TYPE_MMIO = 0,	/* virtio-mmio */
	NR_VIRTIO_BUS_TYPES,
};

struct virtio_bind_bus {
	bool (*hwdesc_probe)(void);
	struct virtio_dev *(*device_bind)(u32 devid);
};

static struct virtio_dev *vm_dt_device_bind(u32 devid);

static struct virtio_bind_bus
virtio_bind_busses[NR_VIRTIO_HWDESC_TYPES][NR_VIRTIO_BUS_TYPES] = {

[VIRTIO_HWDESC_TYPE_DT] = {

	[VIRTIO_BUS_TYPE_MMIO] = {
		.hwdesc_probe = dt_available,
		.device_bind = vm_dt_device_bind,
	},
},
};

struct virtio_dev *virtio_bind(u32 devid)
{
	struct virtio_bind_bus *bus;
	struct virtio_dev *dev;
	int i, j;

	for (i = 0; i < NR_VIRTIO_HWDESC_TYPES; ++i) {
		for (j = 0; j < NR_VIRTIO_BUS_TYPES; ++j) {

			bus = &virtio_bind_busses[i][j];

			if (!bus->hwdesc_probe())
				continue;

			dev = bus->device_bind(devid);
			if (dev)
				return dev;
		}
	}

	return NULL;
}

/******************************************************
 * virtio-mmio support (config space only)
 ******************************************************/

static void vm_get(struct virtio_dev *vdev, unsigned offset,
		   void *buf, unsigned len)
{
	struct virtio_mmio_dev *vmdev = to_virtio_mmio_dev(vdev);
	u8 *p = buf;
	unsigned i;

	for (i = 0; i < len; ++i)
		p[i] = readb(vmdev->base + VIRTIO_MMIO_CONFIG + offset + i);
}

static void vm_set(struct virtio_dev *vdev, unsigned offset,
		   const void *buf, unsigned len)
{
	struct virtio_mmio_dev *vmdev = to_virtio_mmio_dev(vdev);
	const u8 *p = buf;
	unsigned i;

	for (i = 0; i < len; ++i)
		writeb(p[i], vmdev->base + VIRTIO_MMIO_CONFIG + offset + i);
}

#define NR_VM_DEVICES 32
static struct spinlock vm_lock;
static struct virtio_mmio_dev vm_devs[NR_VM_DEVICES];
static struct virtio_conf_ops vm_confs[NR_VM_DEVICES];
static int nr_vm_devs;

static struct virtio_mmio_dev *vm_new_device(u32 devid)
{
	struct virtio_mmio_dev *vmdev;

	if (nr_vm_devs >= NR_VM_DEVICES)
		return NULL;

	spin_lock(&vm_lock);
	vmdev = &vm_devs[nr_vm_devs];
	vmdev->vdev.config = &vm_confs[nr_vm_devs];
	++nr_vm_devs;
	spin_unlock(&vm_lock);

	vmdev->vdev.id.device = devid;
	vmdev->vdev.id.vendor = -1;
	vmdev->vdev.config->get = vm_get;
	vmdev->vdev.config->set = vm_set;

	return vmdev;
}

/******************************************************
 * virtio-mmio device tree support
 ******************************************************/

struct vm_dt_info {
	u32 devid;
	void *base;
};

static int vm_dt_match(const struct dt_device *dev, int fdtnode)
{
	struct vm_dt_info *info = (struct vm_dt_info *)dev->info;
	dt_pbus_addr_t base;

	dt_device_bind_node((struct dt_device *)dev, fdtnode);

	assert(dt_pbus_get_baseaddr(dev, &base) == 0);
	assert(sizeof(long) == 8 || !(base >> 32));

	info->base = (void *)(unsigned long)base;

	return readl(info->base + VIRTIO_MMIO_DEVICE_ID) == info->devid;
}

static struct virtio_dev *vm_dt_device_bind(u32 devid)
{
	struct virtio_mmio_dev *vmdev;
	struct dt_device dt_dev;
	struct dt_bus dt_bus;
	struct vm_dt_info info;
	int node;

	dt_bus_init_defaults(&dt_bus);
	dt_bus.match = vm_dt_match;

	info.devid = devid;

	dt_device_init(&dt_dev, &dt_bus, &info);

	node = dt_device_find_compatible(&dt_dev, "virtio,mmio");
	assert(node >= 0 || node == -FDT_ERR_NOTFOUND);

	if (node == -FDT_ERR_NOTFOUND)
		return NULL;

	vmdev = vm_new_device(devid);
	vmdev->base = info.base;

	return &vmdev->vdev;
}
