#include<linux/init.h>
#include<linux/module.h>
#include<linux/cdev.h>
#include<linux/uaccess.h>
#include<linux/fs.h>
#include<linux/slab.h>

#define MEM_CLEAR 0x1
#define MEM_SIZE 0x1000
#define CDD_MAJOR 230

#define DEVICE_NUM 10

static int cdd_major = CDD_MAJOR;
module_param(cdd_major, int, S_IRUGO);//module_param在模块加载时传递参数

struct cdd_dev {
	struct cdev cdev;
	unsigned char mem[MEM_SIZE];
};

struct cdd_dev *cdd_devp;

static int cdd_open(struct inode * inode, struct file * filp)
{
	/* 将文件私有数据指向cdd_dev设备结构体，
	 * 再用read,write,ioctl,llseek来通过private_data访问设备结构体,
	 * 通常文件open时候完成,多个设备时优势体现出来。
	 * container_of第一个参数为结构体成员的指针,第二个参数是结构体的类型，第三参数时第一参数结构体成员的类型
	 */
	struct cdd_dev *dev = container_of(inode->i_cdev, struct cdd_dev, cdev);
	filp->private_data = dev;
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

	if(p >= MEM_SIZE)//偏移大于申请内存大小表示读取完毕
		return 0;
	if(count > (MEM_SIZE - p))//读取的长度大于内存大小减去相对于文件开头的偏移
		count = MEM_SIZE - p;
	
	if(copy_to_user(buf, dev->mem + p, count)) {//从内核空间复制到用户空间
		ret = -EFAULT;//错误地址
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
			ret = -EINVAL;//-EINVAL:无效参数
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
	default://从文件结尾开始seek
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
	
	cdev_init(&dev->cdev, &cdd_fops);//初始化cdd_dev的成员,并建立cdd_dev与fiel_operation的连接
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev, devno, 1);//向系统添加一个cdev，完成字符设备的注册
	if(err)
		printk(KERN_NOTICE"Error %d adding cdd%d\n", err, index);	
}

static int __init cdd_init(void)//__init表示只在模块加载时执行一次
{
	int ret, i;
	dev_t devno = MKDEV(cdd_major, 0);//通过主次设备号生成det_t

	if(cdd_major)
		ret = register_chrdev_region(devno , DEVICE_NUM, "cdd");//向系统申请设备号
	else {
		ret = alloc_chrdev_region(&devno, 0, DEVICE_NUM, "cdd");//用在设备号未知，成功向系统申请设备号后，会将设备号放进devno中
		cdd_major = MAJOR(devno);//获取主设备号，获取次设备号使用MINOR();
	}
	if(ret < 0)
		return ret;
	/* 1.kzmalloc在slab.h中定义，比kmalloc多了memset功能，将内存内容清零,不能超过128k,kfree来释放内存。
	 * 2.GFP_KERNEL:申请内存时，若不能满足，则进程会睡眠等待页，即阻塞。因此不能在中断上下文或持有自旋锁使用。
	 * 3.GFP_ATOMIC:分配内存的过程是一个原子过程，分配内存的过程不会被(高优先级进程或中断)打断。
	 */
	cdd_devp = kzalloc(sizeof(struct cdd_dev) * DEVICE_NUM, GFP_KERNEL);
	if(!cdd_devp) {
		ret = -ENOMEM; //ENOMEM表示:内存溢出,在uapi/asm-generic/errno-base.h可以查询
		goto fail_malloc;
	}
	
	for(i = 0; i < DEVICE_NUM; i++)
		cdd_setup_cdev(cdd_devp + i, i);
	
	return 0;

fail_malloc:
	unregister_chrdev_region(devno, DEVICE_NUM);//释放设备号
	return ret;
}

static void __exit cdd_exit(void) {
	int i;
	for(i = 0; i < DEVICE_NUM; i++)
		cdev_del(&(cdd_devp + i)->cdev);
	kfree(cdd_devp);
	unregister_chrdev_region(MKDEV(cdd_major, 0), DEVICE_NUM);
}

module_init(cdd_init);
module_exit(cdd_exit);
MODULE_AUTHOR("jzzh");
MODULE_DESCRIPTION("study char devices driver");
MODULE_LICENSE("GPL");
