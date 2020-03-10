#include<linux/init.h>
#include<linux/kernel.h>
#include<linux/module.h>

static int hello_init(void)
{
	printk(KERN_ALERT "szbaijie hello init\n");
	return 0;
}

static void hello_exit(void)
{
	printk(KERN_ALERT "szbaijie hello exit\n");
}

module_init(hello_init);
module_exit(hello_exit);                                                                                                                                                                                           
MODULE_AUTHOR("baijie");
MODULE_LICENSE("GPL");
