#ifndef _VIRTIO_H_
#define _VIRTIO_H_
/*
 * A minimal implementation of virtio for virtio-mmio config space
 * access.
 *
 * Copyright (C) 2014, Red Hat Inc, Andrew Jones <drjones@redhat.com>
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.
 */
#include "libcflat.h"
#include "asm/page.h"

struct virtio_device_id {
	u32 device;
	u32 vendor;
};

struct virtio_device {
	struct virtio_device_id id;
	const struct virtio_config_ops *config;
};

struct virtqueue {
	void (*callback)(struct virtqueue *vq);
	const char *name;
	struct virtio_device *vdev;
	unsigned int index;
	unsigned int num_free;
	void *priv;
};

typedef void vq_callback_t(struct virtqueue *);
struct virtio_config_ops {
	void (*get)(struct virtio_device *vdev, unsigned offset,
		    void *buf, unsigned len);
	void (*set)(struct virtio_device *vdev, unsigned offset,
		    const void *buf, unsigned len);
	int (*find_vqs)(struct virtio_device *vdev, unsigned nvqs,
			struct virtqueue *vqs[],
			vq_callback_t *callbacks[],
			const char *names[]);
};

extern struct virtio_device *virtio_bind(u32 devid);

static inline u8
virtio_config_readb(struct virtio_device *vdev, unsigned offset)
{
	u8 val;
	vdev->config->get(vdev, offset, &val, 1);
	return val;
}

static inline u16
virtio_config_readw(struct virtio_device *vdev, unsigned offset)
{
	u16 val;
	vdev->config->get(vdev, offset, &val, 2);
	return val;
}

static inline u32
virtio_config_readl(struct virtio_device *vdev, unsigned offset)
{
	u32 val;
	vdev->config->get(vdev, offset, &val, 4);
	return val;
}

static inline void
virtio_config_writeb(struct virtio_device *vdev, unsigned offset, u8 val)
{
	vdev->config->set(vdev, offset, &val, 1);
}

static inline void
virtio_config_writew(struct virtio_device *vdev, unsigned offset, u16 val)
{
	vdev->config->set(vdev, offset, &val, 2);
}

static inline void
virtio_config_writel(struct virtio_device *vdev, unsigned offset, u32 val)
{
	vdev->config->set(vdev, offset, &val, 4);
}

#define VRING_DESC_F_NEXT	1
#define VRING_DESC_F_WRITE	2

struct vring_desc {
	u64 addr;
	u32 len;
	u16 flags;
	u16 next;
};

struct vring_avail {
	u16 flags;
	u16 idx;
	u16 ring[];
};

struct vring_used_elem {
	u32 id;
	u32 len;
};

struct vring_used {
	u16 flags;
	u16 idx;
	struct vring_used_elem ring[];
};

struct vring {
	unsigned int num;
	struct vring_desc *desc;
	struct vring_avail *avail;
	struct vring_used *used;
};

struct vring_virtqueue {
	struct virtqueue vq;
	struct vring vring;
	unsigned int free_head;
	unsigned int num_added;
	u16 last_used_idx;
	bool (*notify)(struct virtqueue *vq);
	void *data[];
};

#define to_vvq(_vq) container_of(_vq, struct vring_virtqueue, vq)

extern int virtqueue_add_outbuf(struct virtqueue *vq, char *buf, size_t len);
extern bool virtqueue_kick(struct virtqueue *vq);
extern void *virtqueue_get_buf(struct virtqueue *_vq, unsigned int *len);

/******************************************************
 * virtio-mmio
 ******************************************************/

#define VIRTIO_MMIO_MAGIC_VALUE		0x000
#define VIRTIO_MMIO_VERSION		0x004
#define VIRTIO_MMIO_DEVICE_ID		0x008
#define VIRTIO_MMIO_VENDOR_ID		0x00c
#define VIRTIO_MMIO_HOST_FEATURES	0x010
#define VIRTIO_MMIO_HOST_FEATURES_SEL	0x014
#define VIRTIO_MMIO_GUEST_FEATURES	0x020
#define VIRTIO_MMIO_GUEST_FEATURES_SEL	0x024
#define VIRTIO_MMIO_GUEST_PAGE_SIZE	0x028
#define VIRTIO_MMIO_QUEUE_SEL		0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX	0x034
#define VIRTIO_MMIO_QUEUE_NUM		0x038
#define VIRTIO_MMIO_QUEUE_ALIGN		0x03c
#define VIRTIO_MMIO_QUEUE_PFN		0x040
#define VIRTIO_MMIO_QUEUE_NOTIFY	0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS	0x060
#define VIRTIO_MMIO_INTERRUPT_ACK	0x064
#define VIRTIO_MMIO_STATUS		0x070
#define VIRTIO_MMIO_CONFIG		0x100

#define VIRTIO_MMIO_INT_VRING		(1 << 0)
#define VIRTIO_MMIO_INT_CONFIG		(1 << 1)

#define VIRTIO_MMIO_VRING_ALIGN		PAGE_SIZE

/*
 * The minimum queue size is 2*VIRTIO_MMIO_VRING_ALIGN, which
 * means the largest queue num for the minimum queue size is 128, i.e.
 * 2*VIRTIO_MMIO_VRING_ALIGN = vring_size(128, VIRTIO_MMIO_VRING_ALIGN),
 * where vring_size is
 *
 * unsigned vring_size(unsigned num, unsigned long align)
 * {
 *     return ((sizeof(struct vring_desc) * num + sizeof(u16) * (3 + num)
 *              + align - 1) & ~(align - 1))
 *             + sizeof(u16) * 3 + sizeof(struct vring_used_elem) * num;
 * }
 */
#define VIRTIO_MMIO_QUEUE_SIZE_MIN	(2*VIRTIO_MMIO_VRING_ALIGN)
#define VIRTIO_MMIO_QUEUE_NUM_MIN	128

#define to_virtio_mmio_device(vdev_ptr) \
	container_of(vdev_ptr, struct virtio_mmio_device, vdev)

struct virtio_mmio_device {
	struct virtio_device vdev;
	void *base;
};

#endif /* _VIRTIO_H_ */
