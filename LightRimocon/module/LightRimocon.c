#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/gpio.h>

#define DRIVER_NAME "Light Rimocon"
#define EXEC_FILE "/home/pi/BackApp/LightRimocon/program/light.py"

#define GPIO_CHIP "/dev/gpiochip0"
#define ON_PIN 27
#define GPIOF_PULL_UP (1<<1) //ヘッダーファイルになかったので自作

static dev_t lrimo_device;
static struct cdev lrimo_cdevice;
static struct class *lrimo_class = NULL;
static unsigned int major;

static int lrimo_open(struct inode *i, struct file *f){
	printk("%s opened\n", DRIVER_NAME);
	return 0;
}

static int lrimo_release(struct inode *i, struct file *f){
	printk("%s closed\n", DRIVER_NAME);
	return 0;
}

static ssize_t lrimo_read(struct file *file, char __user *buf, size_t length, loff_t *offset){
	unsigned char read_val = gpio_get_value(ON_PIN);
	int read_length = (length < sizeof(read_val))?length:sizeof(read_val);
	if(copy_to_user(buf, &read_val, read_length)) return -EFAULT;
	return read_length;
}

static void lrimo_exec(int mode){
	char *argv[] = {"/usr/bin/python3", EXEC_FILE, "", NULL};
	char *envp[] = {"HOME=/home/pi", "PATH=/usr/bin:/usr/local/bin", NULL};
	int result = -1;
	switch(mode){
		case 0:
			argv[2] = "off";
			result = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
			printk("Light off : executed\n");
			break;
		case 1:
			argv[2] = "on";
			result = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
			printk("Light on : executed\n");
			break;
		default:
			printk("wrong value is written\n");
			break;
	}
	if(result){
		printk("exec result : %d\n", result);
	}
	return;
}

// 0:off 1:on
static ssize_t lrimo_write(struct file *file, const char __user *buf, size_t length, loff_t *offset){
	unsigned char write_val;
	int write_length = (length < sizeof(write_val))?length:sizeof(write_val);
	if(copy_from_user(&write_val, buf, write_length)) return -EFAULT;
	lrimo_exec((int)write_val);
	return write_length;
}

static struct file_operations lrimo_fops = {
	.owner = THIS_MODULE,
	.open = lrimo_open,
	.read = lrimo_read,
	.write = lrimo_write,
	.release = lrimo_release,
};

static int lrimo_init(void){
	int ret_value;

	// struct gpio_desc *on_gpio = gpio_to_desc(ON_PIN);
	if(gpio_direction_input(ON_PIN)){
		printk("gpio direction set failed\n");
		gpio_free(ON_PIN);
		return -1;
	}

	// メジャー番号確保
	if((ret_value = alloc_chrdev_region(&lrimo_device, 0,1,DRIVER_NAME))<0){
		printk("%s: cdev_add failed (%d)\n", __FUNCTION__, ret_value);
		return ret_value;
	}
	major = MAJOR(lrimo_device);
	
	// cデバイスをカーネルに登録
	cdev_init(&lrimo_cdevice, &lrimo_fops);
	lrimo_cdevice.owner = THIS_MODULE;
	if((ret_value = cdev_add(&lrimo_cdevice, lrimo_device,1))<0){
		printk("%s: cdev_add failed (%d)\n", __FUNCTION__, ret_value);
		unregister_chrdev_region(lrimo_device,1);
		return ret_value;
	}

	// デバイスのクラス登録(/sys/clas/lrimo/を作成)
	lrimo_class = class_create(THIS_MODULE, "lrimo");
	if(IS_ERR(lrimo_class)){
		printk("%s: class_create failed\n", __FUNCTION__);
		class_destroy(lrimo_class);
		cdev_del(&lrimo_cdevice);
		unregister_chrdev_region(lrimo_device, 0);
		return -1;
	}

	// /sys/class/lrimo/lrimo*の作成
	device_create(lrimo_class, NULL, MKDEV(major, 0), NULL, "lrimo");

	// Success
	printk("%s loaded: Major=%d, Minor=%d\n", DRIVER_NAME, MAJOR(lrimo_device), MINOR(lrimo_device));
	return 0;
}

static void lrimo_cleanup(void){
	gpio_free(ON_PIN);

	device_destroy(lrimo_class, MKDEV(major, 0));
	class_destroy(lrimo_class);
	cdev_del(&lrimo_cdevice);
	unregister_chrdev_region(lrimo_device,1);
	printk("%s unloaded\n", DRIVER_NAME);
}

module_init(lrimo_init);
module_exit(lrimo_cleanup);

MODULE_LICENSE("GPL");