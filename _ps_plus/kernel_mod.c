#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/sched/signal.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Uday Gopan");
MODULE_DESCRIPTION("Linux Kernel Module for graphical representation of process tree"); 
MODULE_VERSION("0.1");

static void print_process_tree(struct seq_file *m, struct task_struct *task, int level)
{
    struct task_struct *child;
    struct list_head *list;

    seq_printf(m, "%*s%s [%d]\n", level * 2, "", task->comm, task->pid);

    list_for_each(list, &task->children) {
        child = list_entry(list, struct task_struct, sibling);
        print_process_tree(m, child, level + 1);
    }
}

static int seq_show(struct seq_file *m, void *v)
{
    print_process_tree(m, &init_task, 0);
    return 0;
}

static int proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, seq_show, NULL);
}

static const struct proc_ops proc_fops = {
    .proc_open    = proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release
};

static int __init ps_plus_init(void)
{
    struct proc_dir_entry *entry = proc_create("process_tree", 0, NULL, &proc_fops);
    if (!entry) {
        printk(KERN_ERR "Failed to create /proc/process_tree\n");
        return -ENOMEM;
    }
    printk(KERN_INFO "Loading Process Tree Module...\n");
    return 0;
}

static void __exit ps_plus_exit(void)
{
    remove_proc_entry("process_tree", NULL);
    printk(KERN_INFO "Kernel Module Removed Successfully\n");
}

module_init(ps_plus_init);
module_exit(ps_plus_exit);