#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/slab.h>

#define DEVICE_NAME "pwm_mc"
#define MAX_CHANNELS 8  // Maximum supported PWM channels

struct pwm_channel {
    int gpio_pin;
    atomic_t period_ns;
    atomic_t duty_cycle_ns;
    struct task_struct *pwm_task;
    struct mutex lock;
    bool active;
};

static struct pwm_channel *channels[MAX_CHANNELS];
static int num_channels;
static int gpio_pins[MAX_CHANNELS] = {[0 ... MAX_CHANNELS-1] = -1};

module_param_array(gpio_pins, int, &num_channels, 0);
MODULE_PARM_DESC(gpio_pins, "List of GPIO pins for PWM channels");

static int pwm_thread(void *data) {
    struct pwm_channel *ch = (struct pwm_channel *)data;
    
    while (!kthread_should_stop()) {
        unsigned int period = atomic_read(&ch->period_ns);
        unsigned int duty = atomic_read(&ch->duty_cycle_ns);
        
        if (duty > period) duty = period;

        gpio_set_value(ch->gpio_pin, 1);
        udelay(duty / 1000);
        
        gpio_set_value(ch->gpio_pin, 0);
        udelay((period - duty) / 1000);
    }
    return 0;
}

static ssize_t pwm_write(struct file *filep, const char __user *buf,
                        size_t len, loff_t *offset) {
    char input[32];
    unsigned int ch_num, p, d;
    int en;
    struct pwm_channel *ch;

    if (len >= sizeof(input)) return -EINVAL;
    if (copy_from_user(input, buf, len)) return -EFAULT;
    input[len] = '\0';

    if (sscanf(input, "%u %u %u %d", &ch_num, &p, &d, &en) != 4)
        return -EINVAL;

    if (ch_num >= num_channels) return -EINVAL;
    
    ch = channels[ch_num];
    mutex_lock(&ch->lock);
    
    atomic_set(&ch->period_ns, p);
    atomic_set(&ch->duty_cycle_ns, d);

    if (en) {
        if (!ch->active) {
            ch->pwm_task = kthread_run(pwm_thread, ch, "pwm_thread_%d", ch_num);
            if (IS_ERR(ch->pwm_task)) {
                ch->active = false;
                mutex_unlock(&ch->lock);
                return PTR_ERR(ch->pwm_task);
            }
            ch->active = true;
        }
    } else {
        if (ch->active) {
            kthread_stop(ch->pwm_task);
            ch->active = false;
        }
    }
    mutex_unlock(&ch->lock);
    
    return len;
}

static struct file_operations pwm_fops = {
    .owner = THIS_MODULE,
    .write = pwm_write,
};

static struct miscdevice pwm_devs[MAX_CHANNELS];

static int __init pwm_init(void) {
    int i, ret;
    
    if (num_channels <= 0 || num_channels > MAX_CHANNELS) {
        pr_err("Invalid number of channels\n");
        return -EINVAL;
    }

    for (i = 0; i < num_channels; i++) {
        if (!gpio_is_valid(gpio_pins[i])) {
            pr_err("Invalid GPIO %d for channel %d\n", gpio_pins[i], i);
            goto cleanup;
        }

        channels[i] = kzalloc(sizeof(struct pwm_channel), GFP_KERNEL);
        if (!channels[i]) {
            ret = -ENOMEM;
            goto cleanup;
        }

        channels[i]->gpio_pin = gpio_pins[i];
        atomic_set(&channels[i]->period_ns, 20000000);
        atomic_set(&channels[i]->duty_cycle_ns, 1000000);
        mutex_init(&channels[i]->lock);
        channels[i]->active = false;

        if (gpio_request(gpio_pins[i], "pwm_out") ||
            gpio_direction_output(gpio_pins[i], 0)) {
            pr_err("Failed to initialize GPIO %d\n", gpio_pins[i]);
            kfree(channels[i]);
            goto cleanup;
        }

        pwm_devs[i] = (struct miscdevice){
            .minor = MISC_DYNAMIC_MINOR,
            .name = kasprintf(GFP_KERNEL, "%s%d", DEVICE_NAME, i),
            .fops = &pwm_fops,
        };
        
        if (!pwm_devs[i].name || misc_register(&pwm_devs[i])) {
            pr_err("Failed to register device for channel %d\n", i);
            gpio_free(gpio_pins[i]);
            kfree(channels[i]);
            kfree(pwm_devs[i].name);
            goto cleanup;
        }
    }
    return 0;

cleanup:
    while (--i >= 0) {
        misc_deregister(&pwm_devs[i]);
        gpio_free(gpio_pins[i]);
        kfree(channels[i]);
        kfree(pwm_devs[i].name);
    }
    return -ENODEV;
}

static void __exit pwm_exit(void) {
    int i;
    for (i = 0; i < num_channels; i++) {
        if (channels[i]->active)
            kthread_stop(channels[i]->pwm_task);
            
        misc_deregister(&pwm_devs[i]);
        gpio_free(gpio_pins[i]);
        kfree(channels[i]);
        kfree(pwm_devs[i].name);
    }
}

module_init(pwm_init);
module_exit(pwm_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ahmed Wali, Zainab Ali");
MODULE_DESCRIPTION("Multi-channel Software PWM Driver");
