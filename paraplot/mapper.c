#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/sched/signal.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/rcupdate.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Uday Gopan");
MODULE_DESCRIPTION("Linux Kernel Module for displaying process ID and memory maps of child processes");
MODULE_VERSION("0.3");

static int parent_pid = 1;
module_param(parent_pid, int, 0444);
MODULE_PARM_DESC(parent_pid, "PID of the parent process");

static ssize_t proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
    char kbuf[16];
    long new_pid;

    if (count >= sizeof(kbuf)) 
        return -EINVAL;

    if (copy_from_user(kbuf, buffer, count))
        return -EFAULT;

    kbuf[count] = '\0';
    if (kstrtol(kbuf, 10, &new_pid) < 0)
        return -EINVAL;

    parent_pid = (int)new_pid;
    printk(KERN_INFO "Updated parent PID: %d\n", parent_pid);
    return count;
}

static void print_memory_map(struct seq_file *m, struct task_struct *task, int indent)
{
    struct mm_struct *mm;
    struct vm_area_struct *vma;
    struct vma_iterator vmi;
    int i;

    if (!task)
        return;

    mm = get_task_mm(task);
    if (!mm) {
        for (i = 0; i < indent; i++)
            seq_putc(m, ' ');
        seq_printf(m, "No memory map for process %d\n", task->pid);
        return;
    }

    for (i = 0; i < indent; i++)
        seq_putc(m, ' ');
    seq_printf(m, "Memory map for process %d:\n", task->pid);

    vma_iter_init(&vmi, mm, 0);
    while ((vma = vma_next(&vmi))) {
        for (i = 0; i < indent; i++)
            seq_putc(m, ' ');
        seq_printf(m, "  Start: %lx, End: %lx, Flags: %lx\n",
                   vma->vm_start, vma->vm_end, vma->vm_flags);
    }

    mmput(mm);
}

static void print_child_processes(struct seq_file *m, struct task_struct *task, int indent)
{
    struct task_struct *child;
    int i;

    if (!task)
        return;

    rcu_read_lock();
    if (list_empty(&task->children)) {
        for (i = 0; i < indent; i++)
            seq_putc(m, ' ');
        seq_printf(m, "|- No children for PID: %d\n", task->pid);
    } else {
        list_for_each_entry_rcu(child, &task->children, sibling) {
            if (!child || child->exit_state == EXIT_DEAD)
                continue;

            for (i = 0; i < indent; i++)
                seq_putc(m, ' ');
            seq_printf(m, "|- Child PID: %d (Parent PID: %d)\n", child->pid, task->pid);

            print_memory_map(m, child, indent + 2);
            print_child_processes(m, child, indent + 2);
        }
    }
    rcu_read_unlock();
}


static int seq_show(struct seq_file *m, void *v)
{
    struct pid *pid_struct;
    struct task_struct *parent_task;

    pid_struct = find_get_pid(parent_pid);
    if (!pid_struct) {
        seq_printf(m, "Invalid parent PID: %d\n", parent_pid);
        return 0;
    }

    parent_task = pid_task(pid_struct, PIDTYPE_PID);
    if (!parent_task) {
        seq_printf(m, "No task found for PID: %d\n", parent_pid);
        put_pid(pid_struct);
        return 0;
    }

    seq_printf(m, "Parent process ID: %d\n", parent_task->pid);
    print_child_processes(m, parent_task, 2);

    put_pid(pid_struct);
    return 0;
}

static int proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, seq_show, NULL);
}

static const struct proc_ops proc_fops = {
    .proc_open    = proc_open,
    .proc_read    = seq_read,
    .proc_write   = proc_write,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release
};

static int __init mapper_init(void)
{
    struct proc_dir_entry *entry = proc_create("child_tree", 0666, NULL, &proc_fops);
    if (!entry) {
        printk(KERN_ERR "Failed to create /proc/child_tree\n");
        return -ENOMEM;
    }
    printk(KERN_INFO "Mapper Module Loaded: Monitoring parent PID: %d\n", parent_pid);
    return 0;
}

static void __exit mapper_exit(void)
{
    remove_proc_entry("child_tree", NULL);
    printk(KERN_INFO "Mapper Module Unloaded\n");
}

module_init(mapper_init);
module_exit(mapper_exit);
