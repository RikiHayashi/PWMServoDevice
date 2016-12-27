#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <linux/io.h>
#include <linux/slab.h> 
#include <linux/ioport.h>
#include <asm/io.h>
#include <asm/delay.h>

MODULE_AUTHOR("Shota Hirama");
MODULE_DESCRIPTION("deriver for PWM servomotor control");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

#define RPI_BASE 0x3f000000
#define RPI_GPIO_SIZE	0xC0
#define RPI_GPIO_BASE (RPI_BASE + 0x200000)
#define RPI_PWM_SIZE	0xC0
#define RPI_PWM_BASE (RPI_BASE + 0x20C000)

#define SERVO 18

#define RPI_CLK_OFFSET	0x101000
#define RPI_CLK_SIZE	0x100
#define RPI_CLK_BASE	(RPI_BASE + RPI_CLK_OFFSET)

//clock offsets
#define CLK_PWM_INDEX		0xa0
#define CLK_PWMDIV_INDEX	0xa4

/* GPIO Functions	*/
#define	RPI_GPF_OUTPUT	0x01
#define	RPI_GPF_ALT5	0x02

/* GPIOレジスタインデックス */
#define RPI_GPFSEL0_INDEX	0

//PWM インデックス
#define RPI_PWM_CTRL	0x0
#define RPI_PWM_RNG1	0x10
#define RPI_PWM_DAT1	0x14

static dev_t dev;
static struct cdev cdv;
static struct class *cls = NULL;
static volatile void __iomem *pwm_base;
static volatile void __iomem *clk_base;
static volatile u32 *gpio_base = NULL;

static int rpi_gpio_function_set(int pin, uint32_t func){
	int index = RPI_GPFSEL0_INDEX + pin/10;
	uint32_t mask = ~(0x7 << ((pin%10)*3));
	gpio_base[index] = (gpio_base[index] & mask) | ((func&0x7) << ((pin%10)*3));
	return 1;
}

static void rpi_pwm_write32(uint32_t offset, uint32_t val){
	iowrite32(val, pwm_base + offset);
}

static int parseFreq(const char __user *buf, size_t count, int *ret)
{
	char cval;
	int error = 0, i = 0, tmp, bufcnt = 0, freq;
	size_t readcount = count;
	int sgn = 1;
	
	char *newbuf = kmalloc(sizeof(char)*count, GFP_KERNEL);

	while(readcount > 0)
	{
		if(copy_from_user(&cval, buf + i, sizeof(char)))
		{
			kfree(newbuf);
			return -EFAULT;
		}
		
		if(cval == '-')
		{
			if(bufcnt == 0)
			{
				sgn = -1;
			}
		}
		else if(cval < '0' || cval > '9')
		{
			newbuf[bufcnt] = 'e';
			error = 1;
		}
		else
		{
			newbuf[bufcnt] = cval;
		}
		
		i++;
		bufcnt++;
		readcount--;
		
		if(cval == '\n')
		{
			break;
		}
	}

	freq = 0;
	for(i = 0, tmp = 1; i < bufcnt; i++)
	{
		char c = newbuf[bufcnt - i - 1];
		
		if( c >= '0' && c <= '9')
		{
			freq += ( newbuf[bufcnt - i - 1]  -  '0' ) * tmp;
			tmp *= 10;
		}
	}
	
	*ret = sgn * freq; 
	
	kfree(newbuf);
	
	return bufcnt;
}

static void set_servo_freq(int freq){
	
	int dat;

	if(freq > 90){
		freq = 90;
	}else if(freq < -90){
		freq = -90;
	}

	rpi_gpio_function_set(SERVO, RPI_GPF_ALT5);
	dat = ((freq + 90) * 12 + 400) / 20;
	
	rpi_pwm_write32(RPI_PWM_RNG1, 1024);
	rpi_pwm_write32(RPI_PWM_DAT1, dat);

	printk(KERN_INFO "write: %d\n", dat);
	return;
}

static ssize_t led_write(struct file* filp, const char* buf, size_t count, loff_t* pos){
	int bufcnt = 1;
	int freq;

	bufcnt = parseFreq(buf, count, &freq);
	set_servo_freq(freq);
	printk(KERN_INFO "Finish parse: %d\n", freq);
	return bufcnt;
}

static struct file_operations led_fops = {
	.owner = THIS_MODULE,
	.write = led_write,
};

static int __init init_mod(void){
	int retval;

	if(gpio_base == NULL)
	{
		gpio_base = ioremap_nocache(RPI_GPIO_BASE, RPI_GPIO_SIZE);
	}
	
	if(pwm_base == NULL)
	{
		pwm_base = ioremap_nocache(RPI_PWM_BASE, RPI_PWM_SIZE);
	}
	
	if(clk_base == NULL)
	{
		clk_base = ioremap_nocache(RPI_CLK_BASE, RPI_CLK_SIZE);
	}

	//kill
	iowrite32(0x5a000000 | (1 << 5), clk_base + CLK_PWM_INDEX);
	udelay(1000);

	//clk set
	// 18750/50Hz = 375 
	iowrite32(0x5a000000 | (375 << 12), clk_base + CLK_PWMDIV_INDEX);
	iowrite32(0x5a000011, clk_base + CLK_PWM_INDEX);

	udelay(1000);	//1mS wait

	rpi_gpio_function_set(SERVO, RPI_GPF_OUTPUT);
	rpi_pwm_write32(RPI_PWM_CTRL, 0x00000000);
	udelay(1000);
	rpi_pwm_write32(RPI_PWM_CTRL, 0x00008181);
	printk(KERN_INFO "rpi_pwm_ctrl;%08X\n", ioread32(pwm_base + RPI_PWM_CTRL));

	retval = alloc_chrdev_region(&dev, 0, 1, "pwnservo");
	if(retval < 0){
		printk(KERN_ERR "alloc_chrdev_region failed.\n");
		return retval;
	}
	printk(KERN_INFO "%s is loaded. major:%d\n", __FILE__, MAJOR(dev));

	cdev_init(&cdv, &led_fops);
	retval = cdev_add(&cdv, dev, 1);
	if(retval < 0){
		printk(KERN_ERR "cdev_add failed. major:%d, minor:%d", MAJOR(dev), MINOR(dev));
		return retval;
	}

	cls = class_create(THIS_MODULE, "pwmservo");
	if(IS_ERR(cls)){
		printk(KERN_ERR "class_create failed.");
		return PTR_ERR(cls);
	}

	device_create(cls, NULL, dev, NULL, "pwmservo%d", MINOR(dev));

    return 0;
}

static void __exit cleanup_mod(void){
	cdev_del(&cdv);
	device_destroy(cls, dev);
	class_destroy(cls);
	unregister_chrdev_region(dev, 1);
	printk(KERN_INFO "%s is unloaded. major:%d\n", __FILE__, MAJOR(dev));
}

module_init(init_mod);
module_exit(cleanup_mod);
