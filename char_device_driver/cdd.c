#include<linux/init.h>
#include<linux/module.h>
#include<linux/cdev.h>
#include<linux/uaccess.h>
#include<linux/fs.h>
#include<linux/slab.h>

#define MEM_CLEAR 0x1
#define MEM_SIZE 0x1000
#define CDD_MAJOR 230

static int cdd_major = CDD_MAJOR;
module_param(cdd_major, int, S_IRUGO);//module_param在模块加载时传递参数

struct cdd_dev {
	struct cdev cdev;
	unsigned char mem[MEM_SIZE];
};

struct cdd_dev *cdd_devp;

static int cdd_open(struct inode * inode, struct file * filp)
{
	filp->private_data = cdd_devp;
	return 0;
}

static int cdd_release(struct inode * inode, struct file * filp)
{
	return 0;
}

static long cdd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct cdd_dev *dev = filp->private_data;

	switch(cmd) {
	case MEM_CLEAR:
		memset(dev->mem, 0, MEM_SIZE);
		printk(KERN_INFO "cdd is set to zero \n");
		break;
	default:
		return -EINVAL;
	}
	
	return 0;
}

static ssize_t cdd_write(struct file *filp, const char __user * buf, size_t size, loff_t * ppos)
{
	unsigned long p = *ppos;
	unsigned int count = size;
	int ret = 0;
	struct cdd_dev *dev = filp->private_data;

	if(p >= MEM_SIZE)
		return 0;
	if(count > (MEM_SIZE - p))
		count = MEM_SIZE - p;
	
	if(copy_from_user(dev->mem + p, buf, count)) {
		ret = -EFAULT;
	}else {
		*ppos +=count;
		ret =count;

		printk(KERN_INFO "write %u byte(s) from %lu\n", count, p);
	}
	return ret;
}

static ssize_t cdd_read(struct file *filp, char __user * buf, size_t size, loff_t * ppos)
{
	unsigned long p = *ppos;
	unsigned int count = size;
	int ret = 0;
	struct cdd_dev *dev = filp->private_data;

	if(p >= MEM_SIZE)
		return 0;
	if(count > (MEM_SIZE - p))
		count = MEM_SIZE - p;
	
	if(copy_to_user(buf, dev->mem + p, count)) {
		ret = -EFAULT;
	}else {
		*ppos +=count;
		ret =count;

		printk(KERN_INFO "read %u byte(s) from %lu\n", count, p);
	}
	return ret;
}

static loff_t cdd_llseek(struct file *filp, loff_t offset, int orig)
{
	loff_t ret = 0;
	switch(orig) {
	case 0: //从文件开头位置seek
		if(offset < 0) {
			ret = -EINVAL;//-EINVAL:
			break;
		}
		if(offset > MEM_SIZE) {
			ret = -EINVAL;
			break;
		}
		filp->f_pos = offset;
		ret = filp->f_pos;
		break;
	case 1: //从文件当前位置开始seek
		if((filp->f_pos + offset) < 0) {
			ret = -EINVAL;
			break;
		}
		if((filp->f_pos + offset) > MEM_SIZE) {
			ret = -EINVAL;
			break;
		}
		filp->f_pos += offset;
		ret = filp->f_pos;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static const struct file_operations cdd_fops = {
	.owner = THIS_MODULE,
	.llseek = cdd_llseek,
	.read = cdd_read,
	.write = cdd_write,
	.unlocked_ioctl = cdd_ioctl,
	.open = cdd_open,
	.release = cdd_release,
};

static void cdd_setup_cdev(struct cdd_dev *dev, int index)
{
	int err, devno = MKDEV(cdd_major, index);
	
	cdev_init(&dev->cdev, &cdd_fops);//初始化cdd_dev,fops
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev, devno, 1);
	if(err)
		printk(KERN_NOTICE"Error %d adding cdd%d\n", err, index);	
}

static int __init cdd_init(void)//__init表示只在模块加载时执行一次
{
	int ret;
	dev_t devno = MKDEV(cdd_major, 0);//分配设备号

	if(cdd_major)
		ret = register_chrdev_region(devno , 1, "cdd");//分配设备号
	else {
		ret = alloc_chrdev_region(&devno, 0, 1, "cdd");//用在设备号未知，成功分配设备号后，会将设备号放进devno中
		cdd_major = MAJOR(devno);//获取主设备号，获取次设备号使用MINOR();
	}
	if(ret < 0)
		return ret;
	
	cdd_devp = kzalloc(sizeof(struct cdd_dev), GFP_KERNEL);//在内核中分配内存，GFP_KERNEL表示:
	if(!cdd_devp) {
		ret = -ENOMEM; //ENOMEM表示:
		goto fail_malloc;
	}

	cdd_setup_cdev(cdd_devp, 0);
	return 0;

fail_malloc:
	unregister_chrdev_region(devno, 1);//释放设备号
	return ret;
}

static void __exit cdd_exit(void) {
	cdev_del(&cdd_devp->cdev);
	kfree(cdd_devp);
	unregister_chrdev_region(MKDEV(cdd_major, 0), 1);
}

module_init(cdd_init);
module_exit(cdd_exit);
MODULE_AUTHOR("jzzh");
MODULE_DESCRIPTION("study char devices driver");
MODULE_LICENSE("GPL");
