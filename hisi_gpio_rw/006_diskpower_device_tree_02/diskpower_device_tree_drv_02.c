/************************************************************
*Copyright (C),lcb0281at163.com lcb0281atgmail.com
*FileName: diskpower_device_tree_drv_02.c
*BlogAddr: caibiao-lee.blog.csdn.net
*Description: 使用设备树的方式控制硬盘上下电
*Date:     2019-10-02
*Author:   Caibiao Lee
*Version:  V1.0
*Others:   1.该设备模块适用于海思HI3520X系列芯片
           2.硬盘上下电的引脚为GPIO_0_2
           3.设备树通过引脚的方式将值传递给驱动
*History:
***********************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#define DIR_REG_OFFSET             (0x400) /**方向寄存器偏移地址**/
#define DATA_REG_OFFSET            (0x3fc) /**数据寄存器偏移地址**/
#define DISK_POWER_BASE_PHYADDR    (0x12150000) /**GPIO 0 的基地址**/
#define DISK_POWER_DATA_PHYADDR    (DISK_POWER_BASE_PHYADDR + DATA_REG_OFFSET) /**GPIO 0 数据寄存器地址**/
#define DISK_POWER_DIR_PHYADDR     (DISK_POWER_BASE_PHYADDR + DIR_REG_OFFSET)  /**GPIO 0 方向寄存器地址**/

static int disk_power_pin = 0;
static volatile unsigned int *power_gpio_dir  = NULL;
static volatile unsigned int *power_gpio_data = NULL;

static int major;
static struct class *disk_power_class;

static int diskpower_open (struct inode *node, struct file *filp)
{
    int dir_base = 0;
    int data_base = 0;

    if(NULL==power_gpio_dir)
    {
        dir_base  = DISK_POWER_DIR_PHYADDR;    
        power_gpio_dir = ioremap(dir_base, 4);
        if (power_gpio_dir) {
            printk("ioremap(0x%x) = 0x%x\n", dir_base, power_gpio_dir);
        }
        else {
            return -EINVAL;
        }
    }

    if(NULL==power_gpio_data)
    {
        data_base = DISK_POWER_DATA_PHYADDR;
        power_gpio_data = ioremap(data_base, 4);
        if (power_gpio_data) {
            printk("ioremap(0x%x) = 0x%x\n", data_base, power_gpio_data);
        }
        else {
            return -EINVAL;
        }
    }
    return 0;

}

static ssize_t diskpower_write (struct file *filp, const char __user *buf, size_t size, loff_t *off)
{
    unsigned char val = 0;
    unsigned int old_reg_val = 0;
    unsigned int new_reg_val = 0;
    
    /**set gpio_0_2 output**/
    old_reg_val = *(unsigned int*)power_gpio_dir;
    new_reg_val = (1 << disk_power_pin) |old_reg_val;
    *power_gpio_dir = new_reg_val;

    copy_from_user(&val, buf, 1);
    if (val)
    {
        /** power on **/
        old_reg_val = *(unsigned int*)power_gpio_data;
        new_reg_val = (1 << disk_power_pin) |old_reg_val;
        *power_gpio_data = new_reg_val;
        printk("set gpio_0_2 0; power on\n");
    }
    else
    {
        /** power off **/
        old_reg_val = *(unsigned int*)power_gpio_data;
        new_reg_val = (~(1 << disk_power_pin)) & old_reg_val;
        *power_gpio_data = new_reg_val;
        printk("set gpio_0_2 1; power off\n");
    }

    /* write 1 byte data */
    return 1; 

}

static int diskpower_release (struct inode *node, struct file *filp)
{
    if(NULL!=power_gpio_dir)
    {
        printk("iounmap(0x%x)\n", power_gpio_dir);
        iounmap(power_gpio_dir);
        power_gpio_dir = NULL;
    }

    if(NULL!=power_gpio_data)
    {
        printk("iounmap(0x%x)\n", power_gpio_data);
        iounmap(power_gpio_data);
        power_gpio_data = NULL;
    }
    
    return 0;
}

static struct file_operations diskpower_oprs = {
    .owner   = THIS_MODULE,
    .open    = diskpower_open,
    .write   = diskpower_write,
    .release = diskpower_release,
};

static int diskpower_probe(struct platform_device *pdev)
{
    struct resource *res;

    /* 根据platform_device的资源进行ioremap */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (res) {
        disk_power_pin = res->start;
    }
    else {
        /* 获得pin属性 */
        of_property_read_u32(pdev->dev.of_node, "pin", &disk_power_pin);
    }

    if (!disk_power_pin) 
    {
        printk("can not get pin for led\n");
        return -EINVAL;
    }

    major = register_chrdev(0, "diskpower", &diskpower_oprs);

    disk_power_class = class_create(THIS_MODULE, "diskpower");
    device_create(disk_power_class, NULL, MKDEV(major, 0), NULL, "disk_power"); /* /dev/disk_power */

    return 0;
}

static int diskpower_remove(struct platform_device *pdev)
{
    unregister_chrdev(major, "diskpower");
    device_destroy(disk_power_class,  MKDEV(major, 0));
    class_destroy(disk_power_class);

    return 0;
}

static const struct of_device_id of_match_diskpower[] = {
    { .compatible = "hi3520dv400_diskpower02", .data = NULL },
    { /* sentinel */ }
};


struct platform_driver diskpower_drv = {
    .probe  = diskpower_probe,
    .remove = diskpower_remove,
    .driver = {
        .name   = "diskpower",
        .of_match_table = of_match_diskpower, /* 能支持哪些来自于dts的platform_device */
    }
};

static int diskpower_drv_init(void)
{
    platform_driver_register(&diskpower_drv);
    return 0;
}

static void diskpower_drv_exit(void)
{
    platform_driver_unregister(&diskpower_drv);
}

module_init(diskpower_drv_init);
module_exit(diskpower_drv_exit);

MODULE_AUTHOR("Caibiao Lee");
MODULE_DESCRIPTION("disk power control");
MODULE_LICENSE("GPL");
