/*
 * Copyright (C) 2014, Red Hat Inc, Andrew Jones <drjones@redhat.com>
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.
 */
#include "libcflat.h"
#include "alloc.h"
#include "devicetree.h"
#include "asm/page.h"
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
	struct virtio_device *(*device_bind)(u32 devid);
};

static struct virtio_device *vm_dt_device_bind(u32 devid);

static struct virtio_bind_bus
virtio_bind_busses[NR_VIRTIO_HWDESC_TYPES][NR_VIRTIO_BUS_TYPES] = {

[VIRTIO_HWDESC_TYPE_DT] = {

	[VIRTIO_BUS_TYPE_MMIO] = {
		.hwdesc_probe = dt_available,
		.device_bind = vm_dt_device_bind,
	},
},
};

struct virtio_device *virtio_bind(u32 devid)
{
	struct virtio_bind_bus *bus;
	struct virtio_device *dev;
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

static void vring_init(struct vring *vr, unsigned int num, void *p,
		       unsigned long align)
{
	vr->num = num;
	vr->desc = p;
	vr->avail = p + num*sizeof(struct vring_desc);
	vr->used = (void *)(((unsigned long)&vr->avail->ring[num] + sizeof(u16)
		+ align-1) & ~(align - 1));
}

static void
vring_init_virtqueue(struct vring_virtqueue *vq,
		     unsigned index, unsigned num, unsigned vring_align,
		     struct virtio_device *vdev, void *pages,
		     bool (*notify)(struct virtqueue *),
		     void (*callback)(struct virtqueue *),
		     const char *name)
{
	unsigned i;

	vring_init(&vq->vring, num, pages, vring_align);
	vq->vq.callback = callback;
	vq->vq.vdev = vdev;
	vq->vq.name = name;
	vq->vq.num_free = num;
	vq->vq.index = index;
	vq->notify = notify;
	vq->last_used_idx = 0;
	vq->num_added = 0;
	vq->free_head = 0;

	for (i = 0; i < num-1; i++) {
		vq->vring.desc[i].next = i+1;
		vq->data[i] = NULL;
	}
	vq->data[i] = NULL;
}

int virtqueue_add_outbuf(struct virtqueue *_vq, char *buf, size_t len)
{
	struct vring_virtqueue *vq = to_vvq(_vq);
	unsigned avail;
	int head;

	assert(buf != NULL);
	assert(len != 0);

	if (!vq->vq.num_free)
		return -1;

	--vq->vq.num_free;

	head = vq->free_head;

	vq->vring.desc[head].flags = 0;
	vq->vring.desc[head].addr = virt_to_phys(buf);
	vq->vring.desc[head].len = len;

	vq->free_head = vq->vring.desc[head].next;

	vq->data[head] = buf;

	avail = (vq->vring.avail->idx & (vq->vring.num-1));
	vq->vring.avail->ring[avail] = head;
	wmb();
	vq->vring.avail->idx++;
	vq->num_added++;

	return 0;
}

bool virtqueue_kick(struct virtqueue *_vq)
{
	struct vring_virtqueue *vq = to_vvq(_vq);
	mb();
	return vq->notify(_vq);
}

static void detach_buf(struct vring_virtqueue *vq, unsigned head)
{
	unsigned i = head;

	vq->data[head] = NULL;

	while (vq->vring.desc[i].flags & VRING_DESC_F_NEXT) {
		i = vq->vring.desc[i].next;
		vq->vq.num_free++;
	}

	vq->vring.desc[i].next = vq->free_head;
	vq->free_head = head;
	vq->vq.num_free++;
}

void *virtqueue_get_buf(struct virtqueue *_vq, unsigned int *len)
{
	struct vring_virtqueue *vq = to_vvq(_vq);
	u16 last_used;
	unsigned i;
	void *ret;

	rmb();

	last_used = (vq->last_used_idx & (vq->vring.num-1));
	i = vq->vring.used->ring[last_used].id;
	*len = vq->vring.used->ring[last_used].len;

	ret = vq->data[i];
	detach_buf(vq, i);

	vq->last_used_idx++;

	return ret;
}

/******************************************************
 * virtio-mmio support (config space only)
 ******************************************************/

static void vm_get(struct virtio_device *vdev, unsigned offset,
		   void *buf, unsigned len)
{
	struct virtio_mmio_device *vm_dev = to_virtio_mmio_device(vdev);
	u8 *p = buf;
	unsigned i;

	for (i = 0; i < len; ++i)
		p[i] = readb(vm_dev->base + VIRTIO_MMIO_CONFIG + offset + i);
}

static void vm_set(struct virtio_device *vdev, unsigned offset,
		   const void *buf, unsigned len)
{
	struct virtio_mmio_device *vm_dev = to_virtio_mmio_device(vdev);
	const u8 *p = buf;
	unsigned i;

	for (i = 0; i < len; ++i)
		writeb(p[i], vm_dev->base + VIRTIO_MMIO_CONFIG + offset + i);
}

static bool vm_notify(struct virtqueue *vq)
{
	struct virtio_mmio_device *vm_dev = to_virtio_mmio_device(vq->vdev);
	writel(vq->index, vm_dev->base + VIRTIO_MMIO_QUEUE_NOTIFY);
	return true;
}

static struct virtqueue *vm_setup_vq(struct virtio_device *vdev,
				     unsigned index,
				     void (*callback)(struct virtqueue *vq),
				     const char *name)
{
	struct virtio_mmio_device *vm_dev = to_virtio_mmio_device(vdev);
	struct vring_virtqueue *vq;
	void *queue;
	unsigned num = VIRTIO_MMIO_QUEUE_NUM_MIN;

	vq = alloc(sizeof(*vq));
	queue = alloc_aligned(VIRTIO_MMIO_QUEUE_SIZE_MIN, PAGE_SIZE);
	if (!vq || !queue)
		return NULL;

	writel(index, vm_dev->base + VIRTIO_MMIO_QUEUE_SEL);

	assert(readl(vm_dev->base + VIRTIO_MMIO_QUEUE_NUM_MAX) >= num);

	if (readl(vm_dev->base + VIRTIO_MMIO_QUEUE_PFN) != 0) {
		printf("%s: virtqueue %d already setup! base=%p\n",
				__func__, index, vm_dev->base);
		return NULL;
	}

	writel(num, vm_dev->base + VIRTIO_MMIO_QUEUE_NUM);
	writel(VIRTIO_MMIO_VRING_ALIGN,
			vm_dev->base + VIRTIO_MMIO_QUEUE_ALIGN);
	writel(virt_to_pfn(queue), vm_dev->base + VIRTIO_MMIO_QUEUE_PFN);

	vring_init_virtqueue(vq, index, num, VIRTIO_MMIO_VRING_ALIGN,
			     vdev, queue, vm_notify, callback, name);

	return &vq->vq;
}

static int vm_find_vqs(struct virtio_device *vdev, unsigned nvqs,
		       struct virtqueue *vqs[], vq_callback_t *callbacks[],
		       const char *names[])
{
	unsigned i;

	for (i = 0; i < nvqs; ++i) {
		vqs[i] = vm_setup_vq(vdev, i, callbacks[i], names[i]);
		if (vqs[i] == NULL)
			return -1;
	}

	return 0;
}

static const struct virtio_config_ops vm_config_ops = {
	.get = vm_get,
	.set = vm_set,
	.find_vqs = vm_find_vqs,
};

static void vm_device_init(struct virtio_mmio_device *vm_dev)
{
	vm_dev->vdev.id.device = readl(vm_dev->base + VIRTIO_MMIO_DEVICE_ID);
	vm_dev->vdev.id.vendor = readl(vm_dev->base + VIRTIO_MMIO_VENDOR_ID);
	vm_dev->vdev.config = &vm_config_ops;

	writel(PAGE_SIZE, vm_dev->base + VIRTIO_MMIO_GUEST_PAGE_SIZE);
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
	struct dt_pbus_reg base;
	u32 magic;

	dt_device_bind_node((struct dt_device *)dev, fdtnode);

	assert(dt_pbus_get_base(dev, &base) == 0);
	info->base = ioremap(base.addr, base.size);

	magic = readl(info->base + VIRTIO_MMIO_MAGIC_VALUE);
	if (magic != ('v' | 'i' << 8 | 'r' << 16 | 't' << 24))
		return false;

	return readl(info->base + VIRTIO_MMIO_DEVICE_ID) == info->devid;
}

static struct virtio_device *vm_dt_device_bind(u32 devid)
{
	struct virtio_mmio_device *vm_dev;
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

	vm_dev = alloc(sizeof(*vm_dev));
	if (!vm_dev)
		return NULL;

	vm_dev->base = info.base;
	vm_device_init(vm_dev);

	return &vm_dev->vdev;
}
