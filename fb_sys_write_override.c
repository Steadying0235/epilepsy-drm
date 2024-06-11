#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fb.h>
#include <linux/uaccess.h>

extern ssize_t fb_sys_write(struct fb_info *info, const char __user *buf,
                            size_t count, loff_t *ppos);

ssize_t my_fb_sys_write(struct fb_info *info, const char __user *buf,
                        size_t count, loff_t *ppos)
{
    ssize_t res;

    printk(KERN_DEBUG "my_fb_sys_write: info=%p, buf=%p, count=%zu, ppos=%p\n", info, buf, count, ppos);

    // Call the original function
    res = fb_sys_write(info, buf, count, ppos);

    printk(KERN_DEBUG "my_fb_sys_write: result=%zd\n", res);

    return res;
}

static struct fb_ops *fbops;

static int __init fb_sys_write_override_init(void)
{
    printk("Initializing override module...");
    struct fb_info *info = registered_fb[0]; // grab the first framebuffer

    if (!info) {
        printk(KERN_ERR "No framebuffer device found\n");
        return -ENODEV;
    }
    else {
        printk(KERN_INFO "Existing framebuffer 0 grabbed");
        printk(KERN_INFO "Screen size: %lu", info->screen_size);

    }

    fbops = info->fbops;
    printk(KERN_INFO "successfully dereferenced fbops");
    // Replace the original fb_sys_write with our function
    fbops->fb_write = &my_fb_sys_write;

    printk(KERN_INFO "fb_sys_write overridden\n");

    return 0;
}

static void __exit fb_sys_write_override_exit(void)
{
    struct fb_info *info = registered_fb[0]; // Assume first framebuffer device

    if (info) {
        // Restore the original function
        fbops->fb_write = fb_sys_write;
    }

    printk(KERN_INFO "fb_sys_write restored\n");
}

module_init(fb_sys_write_override_init);
module_exit(fb_sys_write_override_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Evin Jaff");
MODULE_DESCRIPTION("Override fb_sys_write to add debugging statements");

