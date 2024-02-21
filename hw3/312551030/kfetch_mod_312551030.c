#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/utsname.h>
#include <linux/sysinfo.h>
#include <linux/smp.h>
#include <linux/mm.h>
#include <linux/utsname.h>
#include <linux/ktime.h>

MODULE_LICENSE("GPL");

#define SUCCESS 0 
#define DEVICE_NAME "kfetch"
#define KFETCH_NUM_INFO 6
#define KFETCH_BUF_SIZE 1024

static ssize_t kfetch_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t kfetch_write(struct file *, const char __user *, size_t, loff_t *);
static int kfetch_open(struct inode *, struct file *);
static int kfetch_release(struct inode *, struct file *);

int KFETCH_RELEASE = 1;
int KFETCH_NUM_CPUS = 1;
int KFETCH_CPU_MODEL = 1;
int KFETCH_MEM = 1;
int KFETCH_UPTIME = 1;
int KFETCH_NUM_PROCS = 1;

static const char *template[8] = {
    "                   ",
    "        .-.        ",
    "       (.. |       ",
    "       <>  |       ",
    "      / --- \\      ",
    "     ( |   | |     ",
    "   |\\_)___/\\)/\\    ",
    "  <__)------(__/   "
};

const static struct file_operations kfetch_ops = {
    .owner   = THIS_MODULE,
    .read    = kfetch_read,
    .write   = kfetch_write,
    .open    = kfetch_open,
    .release = kfetch_release,
};

enum { 
    CDEV_NOT_USED = 0, 
    CDEV_EXCLUSIVE_OPEN = 1, 
}; 

static atomic_t already_open = ATOMIC_INIT(CDEV_NOT_USED); 

static struct class *cls;
struct cdev *device_cdev;
int Major;

static struct mutex lock;

static int __init kfetch_init(void) 
{ 
    Major = register_chrdev(0, DEVICE_NAME, &kfetch_ops);

    if (Major < 0)
    {
        printk(DEVICE_NAME " registering char device failed with %d\n", Major);
        return -1;
    }

    mutex_init(&lock);

    printk(DEVICE_NAME " initialized %d\n", Major);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    cls = class_create(DEVICE_NAME); 
#else 
    cls = class_create(THIS_MODULE, DEVICE_NAME); 
#endif 
    device_create(cls, NULL, MKDEV(Major, 0), NULL, DEVICE_NAME);

    return SUCCESS; 
}

static void __exit kfetch_exit(void)
{ 
    device_destroy(cls, MKDEV(Major, 0)); 
    class_destroy(cls); 
 
    /* Unregister the device */ 
    unregister_chrdev(Major, DEVICE_NAME); 
    printk(DEVICE_NAME " unloaded\n");
}

static ssize_t kfetch_read(struct file *filp, char __user *buffer, size_t length, loff_t *offset)
{
    /* fetching the information */

    char *kfetch_buf[8];
    char *message = kmalloc(KFETCH_BUF_SIZE, GFP_KERNEL);
    
    size_t total_length = 0;
    int count = 2, task_count = 0;

    s64 uptime;

    char *line = kmalloc(strlen(utsname()->nodename) + 1, GFP_KERNEL);
    for (int i = 0; i < strlen(utsname()->nodename); i++) line[i] = '-';

    for (int i = 0; i < 8; i++) {
        kfetch_buf[i] = kmalloc(KFETCH_BUF_SIZE, GFP_KERNEL);
    }

    snprintf(kfetch_buf[0], KFETCH_BUF_SIZE, "%s%s", template[0], utsname()->nodename);
    snprintf(kfetch_buf[1], KFETCH_BUF_SIZE, "%s%s", template[1], line);

    if (KFETCH_RELEASE == 1) 
    {
        snprintf(kfetch_buf[count], KFETCH_BUF_SIZE, "%sKernel:    %s", template[count], utsname()->release);
        count += 1;
    }

    ////////////////////////////////////////////////////////////////
    
    if (KFETCH_CPU_MODEL == 1) 
    {
        struct cpuinfo_x86 *cinfo;
        cinfo = &cpu_data(0);

        snprintf(kfetch_buf[count], KFETCH_BUF_SIZE, "%sCPU:       %s", template[count], cinfo->x86_model_id);
        count += 1;
    }

    ////////////////////////////////////////////////////////////////
    
    if (KFETCH_NUM_CPUS == 1) 
    {
        unsigned int online_cpus = num_online_cpus();
        unsigned int total_cpus = num_possible_cpus();

        snprintf(kfetch_buf[count], KFETCH_BUF_SIZE, "%sCPUs:      %u / %u", template[count], online_cpus, total_cpus);
        count += 1;
    }

    ////////////////////////////////////////////////////////////////
    
    if (KFETCH_MEM == 1) 
    {
        struct sysinfo i;
        si_meminfo(&i);

        snprintf(kfetch_buf[count], KFETCH_BUF_SIZE, "%sMem:       %lu MB / %lu MB", template[count], i.freeram * i.mem_unit / 1024 / 1024, i.totalram * i.mem_unit / 1024 / 1024);
        count += 1;
    }

    ////////////////////////////////////////////////////////////////

    if (KFETCH_NUM_PROCS == 1) 
    {
        struct task_struct *task;
        for_each_process(task) task_count++;

        snprintf(kfetch_buf[count], KFETCH_BUF_SIZE, "%sProcs:     %d", template[count], task_count);
        count += 1;
    }

    ////////////////////////////////////////////////////////////////

    if (KFETCH_UPTIME == 1) 
    {
        uptime = ktime_divns(ktime_get_coarse_boottime(), NSEC_PER_SEC);

        snprintf(kfetch_buf[count], KFETCH_BUF_SIZE, "%sUptime:    %lld mins", template[count], uptime / 60);
        count += 1;
    }

    ////////////////////////////////////////////////////////////////

    for (int i = count; i < 8; i++)
        snprintf(kfetch_buf[i], KFETCH_BUF_SIZE, "%s", template[i]);

    message[0] = '\n';
    for (int i = 0; i < 8; i++) 
        snprintf(message + strlen(message), KFETCH_BUF_SIZE - strlen(message), "%s\n", kfetch_buf[i]);

    total_length = strlen(message);

    if (copy_to_user(buffer, message, total_length)) {
        pr_alert("Failed to copy data to user");
        return -1;
    }

    for (int i = 0; i < 8; i++) kfree(kfetch_buf[i]);
    kfree(message);

    return total_length;
}

static ssize_t kfetch_write(struct file *filp, const char __user *buffer, size_t length, loff_t *offset)
{
    int mask_info;

    if (copy_from_user(&mask_info, buffer, length)) {
        pr_alert("Failed to copy data from user");
        return -1;
    }

    /* setting the information mask */

    for (int i = 0; i < KFETCH_NUM_INFO; i++) 
    {
        if (i == 0) KFETCH_RELEASE = mask_info % 2;
        if (i == 1) KFETCH_NUM_CPUS = mask_info % 2;
        if (i == 2) KFETCH_CPU_MODEL = mask_info % 2;
        if (i == 3) KFETCH_MEM = mask_info % 2;
        if (i == 4) KFETCH_UPTIME = mask_info % 2;
        if (i == 5) KFETCH_NUM_PROCS = mask_info % 2;

        mask_info = mask_info >> 1;
    }

    return SUCCESS; 
}

static int kfetch_open(struct inode *inode, struct file *file)
{
    if (atomic_cmpxchg(&already_open, CDEV_NOT_USED, CDEV_EXCLUSIVE_OPEN)) 
        return -EBUSY; 

    printk(DEVICE_NAME " open\n");
    return SUCCESS;
}

static int kfetch_release(struct inode *inode, struct file *file)
{
    atomic_set(&already_open, CDEV_NOT_USED);
    
    printk(DEVICE_NAME " release\n");
    return SUCCESS; 
}

module_init(kfetch_init);
module_exit(kfetch_exit);
