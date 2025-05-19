#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>

#define DEVICE_NAME "ir_mc"
#define MAX_CHANNELS 8

struct ir_channel {
    int gpio_pin;
    struct miscdevice mdev;
};

static struct ir_channel *channels[MAX_CHANNELS];
static int num_channels;
static int gpio_pins[MAX_CHANNELS] = {[0 ... MAX_CHANNELS-1] = -1};

module_param_array(gpio_pins, int, &num_channels, 0);
MODULE_PARM_DESC(gpio_pins, "GPIO pins for IR sensor channels");

static int ir_open(struct inode *inode, struct file *file)
{
    int minor = iminor(inode);
    int i;

    for (i = 0; i < num_channels; i++) {
        if (channels[i] && channels[i]->mdev.minor == minor) {
            file->private_data = channels[i];
            return 0;
        }
    }
    return -ENODEV;
}

static ssize_t ir_read(struct file *filep, char __user *buffer, 
                      size_t len, loff_t *offset)
{
    struct ir_channel *ch = filep->private_data;
    int val;

    if (*offset > 0)
        return 0;

    val = gpio_get_value_cansleep(ch->gpio_pin) ? '1' : '0';

    if (copy_to_user(buffer, &val, 1))
        return -EFAULT;

    *offset = 1;
    return 1;
}

static struct file_operations ir_fops = {
    .owner = THIS_MODULE,
    .open = ir_open,
    .read = ir_read,
};

static int __init ir_init(void)
{
    int i, ret;

    if (num_channels <= 0 || num_channels > MAX_CHANNELS) {
        pr_err("Invalid number of channels\n");
        return -EINVAL;
    }

    for (i = 0; i < num_channels; i++) {
        channels[i] = kzalloc(sizeof(struct ir_channel), GFP_KERNEL);
        if (!channels[i]) {
            ret = -ENOMEM;
            goto cleanup;
        }

        if (!gpio_is_valid(gpio_pins[i])) {
            pr_err("Invalid GPIO %d for channel %d\n", gpio_pins[i], i);
            ret = -ENODEV;
            kfree(channels[i]);
            channels[i] = NULL;
            goto cleanup;
        }

        ret = gpio_request(gpio_pins[i], "ir_input");
        if (ret) {
            pr_err("Failed to request GPIO %d\n", gpio_pins[i]);
            kfree(channels[i]);
            channels[i] = NULL;
            goto cleanup;
        }

        ret = gpio_direction_input(gpio_pins[i]);
        if (ret) {
            pr_err("Failed to set GPIO %d as input\n", gpio_pins[i]);
            gpio_free(gpio_pins[i]);
            kfree(channels[i]);
            channels[i] = NULL;
            goto cleanup;
        }

        channels[i]->gpio_pin = gpio_pins[i];
        channels[i]->mdev.minor = MISC_DYNAMIC_MINOR;
        channels[i]->mdev.name = kasprintf(GFP_KERNEL, "%s%d", DEVICE_NAME, i);
        channels[i]->mdev.fops = &ir_fops;

        if (!channels[i]->mdev.name) {
            pr_err("Failed to allocate device name\n");
            gpio_free(gpio_pins[i]);
            kfree(channels[i]);
            channels[i] = NULL;
            ret = -ENOMEM;
            goto cleanup;
        }

        ret = misc_register(&channels[i]->mdev);
        if (ret) {
            pr_err("Failed to register misc device\n");
            kfree(channels[i]->mdev.name);
            gpio_free(gpio_pins[i]);
            kfree(channels[i]);
            channels[i] = NULL;
            goto cleanup;
        }
    }
    return 0;

cleanup:
    while (--i >= 0) {
        if (channels[i]) {
            misc_deregister(&channels[i]->mdev);
            kfree(channels[i]->mdev.name);
            gpio_free(gpio_pins[i]);
            kfree(channels[i]);
        }
    }
    return ret;
}

static void __exit ir_exit(void)
{
    int i;
    for (i = 0; i < num_channels; i++) {
        if (channels[i]) {
            misc_deregister(&channels[i]->mdev);
            kfree(channels[i]->mdev.name);
            gpio_free(channels[i]->gpio_pin);
            kfree(channels[i]);
        }
    }
}

module_init(ir_init);
module_exit(ir_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ahmed Wali, Zainab Ali");
MODULE_DESCRIPTION("Multi-channel IR Sensor Driver");