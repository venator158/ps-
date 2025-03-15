#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("UDAY GOPAN");
MODULE_DESCRITION("Hello World");
MODULE_VERSION("1.0");


static int  __init hello_init(void){
    printk(KERN_INFO "Hello World \n");;
    return 0;
}

static void __exit hello_exit(void){
    printk(KERN_INFO, "Goodbye \n");
}

module_init(hello_init);
module_exit(hello_exit);


