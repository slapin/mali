/*
 * Copyright (C) 2010, 2012-2013, 2015 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <drm/drmP.h>
#include <drm/drm_legacy.h>
#include "mali_drm.h"
#include "mali_drv.h"

#define VIDEO_TYPE 0
#define MEM_TYPE 1

#define MALI_MM_ALIGN_SHIFT 4
#define MALI_MM_ALIGN_MASK ( (1 << MALI_MM_ALIGN_SHIFT) - 1)

struct mali_memblock {
	struct drm_mm_node mm_node;
	struct list_head owner_list;
};

static int mali_fb_init(struct drm_device *dev, void *data, struct drm_file *file)
{
	drm_mali_private_t *dev_priv = dev->dev_private;
	drm_mali_fb_t *fb = data;
	printk(KERN_ERR "DRM: %s\n", __func__);

	mutex_lock(&dev->struct_mutex);
//	{
//		struct drm_sman_mm sman_mm;
//		sman_mm.private = (void *)0xFFFFFFFF;
//		sman_mm.allocate = mali_sman_mm_allocate;
//		sman_mm.free = mali_sman_mm_free;
//		sman_mm.destroy = mali_sman_mm_destroy;
//		sman_mm.offset = mali_sman_mm_offset;
//		ret = drm_sman_set_manager(&dev_priv->sman, VIDEO_TYPE, &sman_mm);
//	}
//
//	if (ret)
//	{
//		DRM_ERROR("VRAM memory manager initialisation error\n");
//		mutex_unlock(&dev->struct_mutex);
//		return ret;
//	}
	drm_mm_init(&dev_priv->vram_mm, 0, fb->size >> MALI_MM_ALIGN_SHIFT);

	dev_priv->vram_initialized = 1;
	dev_priv->vram_offset = fb->offset;

	mutex_unlock(&dev->struct_mutex);
	DRM_DEBUG("offset = %u, size = %u\n", fb->offset, fb->size);

	return 0;
}

static int mali_drm_alloc(struct drm_device *dev, struct drm_file *file, void *data, int pool)
{
	drm_mali_private_t *dev_priv = dev->dev_private;
	drm_mali_mem_t *mem = data;
	int retval = 0, user_key;
	struct mali_memblock *item;
	unsigned long offset;
	struct mali_file_private *file_priv = file->driver_priv;

	printk(KERN_ERR "DRM: %s\n", __func__);

	mutex_lock(&dev->struct_mutex);

	if (0 == dev_priv->vram_initialized)
	{
		DRM_ERROR("Attempt to allocate from uninitialized memory manager.\n");
		mutex_unlock(&dev->struct_mutex);
		return -EINVAL;
	}

//	mem->size = (mem->size + MALI_MM_ALIGN_MASK) >> MALI_MM_ALIGN_SHIFT;
//	item = drm_sman_alloc(&dev_priv->sman, pool, mem->size, 0,
//	                      (unsigned long)file_priv);
	item = kzalloc(sizeof(*item), GFP_KERNEL);
	if (!item) {
		retval = -ENOMEM;
		goto fail_alloc;
	}
	retval = drm_mm_insert_node(&dev_priv->vram_mm,
			&item->mm_node, mem->size, 0, DRM_MM_SEARCH_DEFAULT);
	if (retval)
		goto fail_alloc;
#if 0
again:
	if (idr_pre_get(&dev_priv->object_idr, GFP_KERNEL) == 0) {
		retval = -ENOMEM;
		goto fail_idr;
	}
	retval = idr_get_new_above(&dev_priv->object_idr, item, 1, &user_key);
	if (retval == -EAGAIN)
		goto again;
	if (retval)
		goto fail_idr;
#else
	retval = idr_alloc(&dev_priv->object_idr, item, 1, 0, GFP_KERNEL);
	if (retval < 0)
		goto fail_idr;
	user_key = retval;

#endif
	list_add(&item->owner_list, &file_priv->obj_list);
	mutex_unlock(&dev->struct_mutex);

	mem->size = (mem->size + MALI_MM_ALIGN_MASK) >> MALI_MM_ALIGN_SHIFT;
	offset = item->mm_node.start;
	mem->offset = dev_priv->vram_offset + (offset << MALI_MM_ALIGN_SHIFT);
	mem->free = user_key;
	mem->size = mem->size << MALI_MM_ALIGN_SHIFT;
	return 0;

fail_idr:
	drm_mm_remove_node(&item->mm_node);
fail_alloc:
	mutex_unlock(&dev->struct_mutex);
	kfree(item);
	mem->offset = 0;
	mem->size = 0;
	mem->free = 0;
	DRM_DEBUG("alloc %d, size = %d, offset = %d\n", pool, mem->size, mem->offset);
	return retval;
}

static int mali_drm_free(struct drm_device *dev, void *data, struct drm_file *file)
{
	drm_mali_private_t *dev_priv = dev->dev_private;
	drm_mali_mem_t *mem = data;
	struct mali_memblock *obj;
	printk(KERN_ERR "DRM: %s\n", __func__);

	mutex_lock(&dev->struct_mutex);
	obj = idr_find(&dev_priv->object_idr, mem->free);
	if (!obj) {
		mutex_unlock(&dev->struct_mutex);
		return -EINVAL;
	}
	idr_remove(&dev_priv->object_idr, mem->free);
	list_del(&obj->owner_list);
	drm_mm_remove_node(&obj->mm_node);
	kfree(obj);
	mutex_unlock(&dev->struct_mutex);
	DRM_DEBUG("free = 0x%lx\n", mem->free);

	return 0;
}

static int mali_fb_alloc(struct drm_device *dev, void *data, struct drm_file *file)
{
	printk(KERN_ERR "DRM: %s\n", __func__);
	return mali_drm_alloc(dev, file, data, VIDEO_TYPE);
}

static int mali_ioctl_mem_init(struct drm_device *dev, void *data, struct drm_file *file)
{
	drm_mali_private_t *dev_priv = dev->dev_private;
	drm_mali_mem_t *mem = data;
	dev_priv = dev->dev_private;
	printk(KERN_ERR "DRM: %s\n", __func__);

	mutex_lock(&dev->struct_mutex);
	drm_mm_init(&dev_priv->vram_mm, 0, mem->size >> MALI_MM_ALIGN_SHIFT);
	mutex_unlock(&dev->struct_mutex);

	return 0;
}

static int mali_ioctl_mem_alloc(struct drm_device *dev, void *data,
                                struct drm_file *file)
{
	printk(KERN_ERR "DRM: %s\n", __func__);
	return mali_drm_alloc(dev, file, data, MEM_TYPE);
}

static drm_local_map_t *mem_reg_init(struct drm_device *dev)
{
	struct drm_map_list *entry;
	drm_local_map_t *map;
	printk(KERN_ERR "DRM: %s\n", __func__);

	list_for_each_entry(entry, &dev->maplist, head)
	{
		map = entry->map;

		if (!map)
		{
			continue;
		}

		if (map->type == _DRM_REGISTERS)
		{
			return map;
		}
	}
	return NULL;
}

int mali_idle(struct drm_device *dev)
{
	drm_mali_private_t *dev_priv = dev->dev_private;
	printk(KERN_ERR "DRM: %s\n", __func__);

	if (dev_priv->idle_fault)
	{
		return 0;
	}

	return 0;
}


void mali_lastclose(struct drm_device *dev)
{
	drm_mali_private_t *dev_priv = dev->dev_private;
	printk(KERN_ERR "DRM: %s\n", __func__);

	if (!dev_priv)
	{
		return;
	}

	mutex_lock(&dev->struct_mutex);
	if (dev_priv->vram_initialized) {
		drm_mm_takedown(&dev_priv->vram_mm);
		dev_priv->vram_initialized = 0;
	}
	dev_priv->mmio = NULL;
	mutex_unlock(&dev->struct_mutex);
}

void mali_reclaim_buffers_locked(struct drm_device *dev, struct drm_file *file)
{
	struct mali_file_private *file_priv = file->driver_priv;
	struct mali_memblock *entry, *next;
	if (!(file->minor->master && file->master->lock.hw_lock))
		return;
	drm_legacy_idlelock_take(&file->master->lock);

	printk(KERN_ERR "DRM: %s\n", __func__);

	mutex_lock(&dev->struct_mutex);
	if (list_empty(&file_priv->obj_list)) {
		mutex_unlock(&dev->struct_mutex);
		drm_legacy_idlelock_release(&file->master->lock);
		return;
	}
	mali_idle(dev);
	list_for_each_entry_safe(entry, next, &file_priv->obj_list,
			owner_list) {
		list_del(&entry->owner_list);
		drm_mm_remove_node(&entry->mm_node);
		kfree(entry);
	}

	mutex_unlock(&dev->struct_mutex);
	drm_legacy_idlelock_release(&file->master->lock);
	return;
}

struct drm_ioctl_desc mali_ioctls[] =
{
	DRM_IOCTL_DEF_DRV(MALI_FB_ALLOC, mali_fb_alloc, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(MALI_FB_FREE, mali_drm_free, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(MALI_MEM_INIT, mali_ioctl_mem_init, DRM_AUTH | DRM_MASTER | DRM_ROOT_ONLY),
	DRM_IOCTL_DEF_DRV(MALI_MEM_ALLOC, mali_ioctl_mem_alloc, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(MALI_MEM_FREE, mali_drm_free, DRM_AUTH),
	DRM_IOCTL_DEF_DRV(MALI_FB_INIT, mali_fb_init, DRM_AUTH | DRM_MASTER | DRM_ROOT_ONLY),
};

int mali_max_ioctl = ARRAY_SIZE(mali_ioctls);
