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

struct virtio_device_id {
	u32 device;
	u32 vendor;
};

struct virtio_device {
	struct virtio_device_id id;
	const struct virtio_config_ops *config;
};

struct virtio_config_ops {
	void (*get)(struct virtio_device *vdev, unsigned offset,
		    void *buf, unsigned len);
	void (*set)(struct virtio_device *vdev, unsigned offset,
		    const void *buf, unsigned len);
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

/******************************************************
 * virtio-mmio
 ******************************************************/

#define VIRTIO_MMIO_DEVICE_ID	0x008
#define VIRTIO_MMIO_CONFIG	0x100

#define to_virtio_mmio_device(vdev_ptr) \
	container_of(vdev_ptr, struct virtio_mmio_device, vdev)

struct virtio_mmio_device {
	struct virtio_device vdev;
	void *base;
};

#endif /* _VIRTIO_H_ */
