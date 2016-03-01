/*
 * Copyright (C) 2010, 2012-2013, 2015 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/vermagic.h>
#include <linux/version.h>
#include <drm/drmP.h>
#include <drm/drm_legacy.h>
#include "mali_drm.h"
#include "mali_drv.h"

static struct platform_device *pdev;

#if 0
static const struct drm_device_id dock_device_ids[] =
{
	{"MALIDRM", 0},
	{"", 0},
};
#endif

static int mali_driver_load(struct drm_device *dev, unsigned long chipset)
{
	int ret;
	drm_mali_private_t *dev_priv;
	printk(KERN_ERR "DRM: mali_driver_load start\n");

	dev_priv = kzalloc(sizeof(drm_mali_private_t), GFP_KERNEL);

	if (dev_priv == NULL)
	{
		return -ENOMEM;
	}

	dev->dev_private = (void *)dev_priv;

#if 0
	base = drm_get_resource_start(dev, 1);
	size = drm_get_resource_len(dev, 1);
#endif
	//if ( ret ) kfree( dev_priv );
	idr_init(&dev_priv->object_idr);

	printk(KERN_ERR "DRM: mali_driver_load done\n");

	return ret;
}

static int mali_driver_unload(struct drm_device *dev)
{
	drm_mali_private_t *dev_priv = dev->dev_private;
	printk(KERN_ERR "DRM: mali_driver_unload start\n");
	idr_destroy(&dev_priv->object_idr);

	kfree(dev_priv);
	//kfree( dev_priv );
	printk(KERN_ERR "DRM: mali_driver_unload done\n");

	return 0;
}
static int mali_driver_open(struct drm_device *dev, struct drm_file *file)
{
	struct mali_file_private *file_priv;
	file_priv = kmalloc(sizeof(*file_priv), GFP_KERNEL);
	if (!file_priv)
		return -ENOMEM;
	file->driver_priv = file_priv;
	INIT_LIST_HEAD(&file_priv->obj_list);
	return 0;
}
static void mali_driver_postclose(struct drm_device *dev, struct drm_file *file)
{
	struct mali_file_private *file_priv = file->driver_priv;

	kfree(file_priv);
}

static struct file_operations mali_driver_fops = {
	.owner = THIS_MODULE,
	.open = drm_open,
	.release = drm_release,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
	.ioctl = drm_ioctl,
#else
	.unlocked_ioctl = drm_ioctl,
#endif
	.mmap = drm_legacy_mmap,
	.poll = drm_poll,
#ifdef CONFIG_COMPAT
	.compat_ioctl = drm_compat_ioctl,
#endif
};

static struct drm_driver driver =
{
	.driver_features = 0,
	.load = mali_driver_load,
	.unload = mali_driver_unload,
	.open = mali_driver_open,
	.preclose = mali_reclaim_buffers_locked,
	.postclose = mali_driver_postclose,
	.context_dtor = NULL,
	.dma_quiescent = mali_idle,
	.lastclose = mali_lastclose,
	.ioctls = mali_ioctls,
	.fops = &mali_driver_fops,
	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
	.patchlevel = DRIVER_PATCHLEVEL,
};
static int mali_platform_drm_probe(struct platform_device *pdev)
{
	return drm_platform_init(&driver, pdev);
}

static int mali_platform_drm_remove(struct platform_device *pdev)
{
	return 0;
}

static int mali_platform_drm_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

static int mali_platform_drm_resume(struct platform_device *dev)
{
	return 0;
}


static char mali_drm_device_name[] = "mali_drm";
static struct platform_driver mali_pdriver = {
	.probe = mali_platform_drm_probe,
	.remove = mali_platform_drm_remove,
	.suspend = mali_platform_drm_suspend,
	.resume = mali_platform_drm_resume,
	.driver = {
		.name = mali_drm_device_name,
		.owner = THIS_MODULE,
	},
};


static int __init mali_init(void)
{
	int ret;
	struct platform_device *pdev;
	driver.num_ioctls = mali_max_ioctl;
	ret = platform_driver_register(&mali_pdriver);
	if (ret)
		goto out;
	pdev = platform_device_register_simple(mali_drm_device_name, 0, NULL, 0);
	if (IS_ERR(pdev))
		ret = PTR_ERR(pdev);
out:
	return ret;
}

static void __exit mali_exit(void)
{
	platform_device_unregister(pdev);
}

module_init(mali_init);
module_exit(mali_exit);

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_AUTHOR("ARM Ltd.");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL and additional rights");
