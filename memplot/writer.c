#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/sched/signal.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Uday Gopan");
MODULE_DESCRIPTION("Kernel module to display all processes and their memory maps in /proc/mem_tree/");
MODULE_VERSION("0.4");

#define PROC_DIR "mem_tree"

struct proc_dir_entry *proc_mem_tree;

/* Function to print the memory map of a process */
static int mem_map_show(struct seq_file *m, void *v)
{
    struct task_struct *task = m->private;
    struct mm_struct *mm;
    struct vm_area_struct *vma;
    struct vma_iterator vmi;

    if (!task)
        return -ENOENT;

    mm = get_task_mm(task);
    if (!mm) {
        seq_printf(m, "No memory map available for PID %d\n", task->pid);
        return 0;
    }

    seq_printf(m, "Memory map for PID %d:\n", task->pid);

    vma_iter_init(&vmi, mm, 0);
    while ((vma = vma_next(&vmi))) {
        seq_printf(m, "  Start: %lx, End: %lx, Flags: %lx\n",
                   vma->vm_start, vma->vm_end, vma->vm_flags);
    }

    mmput(mm);
    return 0;
}

/* Open function for the proc file */
static int mem_map_open(struct inode *inode, struct file *file)
{
        return single_open(file, mem_map_show, NULL);  // If no private data needed
}

static const struct proc_ops mem_map_fops = {
    .proc_open    = mem_map_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

/* Create proc entries for all processes */
static int __init mem_tree_init(void)
{
    struct task_struct *task;
    char pid_name[16];
    struct proc_dir_entry *entry;

    /* Create /proc/mem_tree directory */
    proc_mem_tree = proc_mkdir(PROC_DIR, NULL);
    if (!proc_mem_tree) {
        printk(KERN_ERR "Failed to create /proc/%s\n", PROC_DIR);
        return -ENOMEM;
    }

    for_each_process(task) {
        snprintf(pid_name, sizeof(pid_name), "%d", task->pid);

        entry = proc_create_data(pid_name, 0444, proc_mem_tree, &mem_map_fops, task);
        if (!entry) {
            printk(KERN_ERR "Failed to create /proc/%s/%s\n", PROC_DIR, pid_name);
        }
    }

    printk(KERN_INFO "Memory Tree Module Loaded.\n");
    return 0;
}

/* Remove all proc entries on exit */
static void __exit mem_tree_exit(void)
{
    struct task_struct *task;
    char pid_name[16];

    for_each_process(task) {
        snprintf(pid_name, sizeof(pid_name), "%d", task->pid);
        remove_proc_entry(pid_name, proc_mem_tree);
    }

    remove_proc_entry(PROC_DIR, NULL);
    printk(KERN_INFO "Memory Tree Module Unloaded.\n");
}

module_init(mem_tree_init);
module_exit(mem_tree_exit);
