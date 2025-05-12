#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>

//lets make GPio
#define GPIO_PIN 529 //512 offset + gpio17
#define DEVICE_NAME "ir_sensor" //makes in dev a node

static ssize_t ir_read(struct file *filep, char __user *buffer, size_t len, loff_t *offset){
	
	int val;
	if (*offset > 0) return 0;
	val = gpio_get_value_cansleep(GPIO_PIN) ? '1' : '0';
		
	if (copy_to_user(buffer, &val, 1)) return -EFAULT;
	*offset = 1;
	return 1;	
	
}

static struct file_operations ir_fops = {
	.owner = THIS_MODULE,
	.read = ir_read,
};

static struct miscdevice ir_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEVICE_NAME, //dev/ir_sensor
	.fops = &ir_fops,
	//.mode = 0666,
};

static int __init ir_init(void){
		
		if (!gpio_is_valid(GPIO_PIN) || gpio_request(GPIO_PIN, "in") || gpio_direction_input(GPIO_PIN)) 
			return -ENODEV;
		
		return misc_register(&ir_dev);

}

static void __exit ir_exit(void){
		misc_deregister(&ir_dev);
		gpio_free(GPIO_PIN); //FREES THE GPIO
	
}

module_init(ir_init);
module_exit(ir_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ZAINAB");
MODULE_DESCRIPTION("ir sensor gpio driver module ");