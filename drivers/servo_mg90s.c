#include <linux/module.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/pwm.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define DRIVER_NAME "mg90s_servo"
#define PERIOD_NS 20000000  // 20ms period (50Hz)

static struct pwm_device *pwm;
static unsigned int current_angle = 0;

static int servo_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int servo_release(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t servo_write(struct file *file, const char __user *buf, 
                          size_t count, loff_t *ppos)
{
    char kbuf[32];
    unsigned int angle;
    unsigned int duty_cycle;

    if (count >= sizeof(kbuf)) return -EINVAL;
    if (copy_from_user(kbuf, buf, count)) return -EFAULT;
    kbuf[count] = '\0';

    if (sscanf(kbuf, "%u", &angle) != 1) return -EINVAL;
    if (angle > 180) angle = 180;

    // Convert angle to duty cycle (1ms-2ms pulse width)
    duty_cycle = 1000000 + (angle * 1000000) / 180;
    
    pwm_config(pwm, duty_cycle, PERIOD_NS);
    current_angle = angle;
    
    return count;
}

static struct file_operations servo_fops = {
    .owner = THIS_MODULE,
    .open = servo_open,
    .release = servo_release,
    .write = servo_write,
};

static struct miscdevice servo_miscdev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DRIVER_NAME,
    .fops = &servo_fops,
    .mode = 0666,
};

static int __init servo_init(void)
{
    int ret;
    
    // Get PWM0 (GPIO18)
    pwm = pwm_request(0, "mg90s-servo");
    if (IS_ERR(pwm)) {
        pr_err("Failed to get PWM0\n");
        return PTR_ERR(pwm);
    }
    
    pwm_config(pwm, 0, PERIOD_NS);
    pwm_enable(pwm);
    
    ret = misc_register(&servo_miscdev);
    if (ret) {
        pwm_free(pwm);
        pr_err("Failed to register misc device\n");
    }
    
    pr_info("Servo driver loaded. Control via /dev/mg90s_servo\n");
    return ret;
}

static void __exit servo_exit(void)
{
    pwm_disable(pwm);
    pwm_free(pwm);
    misc_deregister(&servo_miscdev);
    pr_info("Servo driver unloaded\n");
}

module_init(servo_init);
module_exit(servo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Misc Device Driver for MG90S Servo");