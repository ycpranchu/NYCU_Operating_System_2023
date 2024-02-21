#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>

SYSCALL_DEFINE2(revstr, int, usr_length, char __user *, usr_str)
{
    char k_str[256];
    long i;

    if (usr_length <= 0 || usr_length > 256)
        return -EINVAL;

    if (copy_from_user(k_str, usr_str, sizeof(k_str)))
        return -EFAULT;
    
    printk("The origin string: %s\n", k_str);

    for (i = 0; i < usr_length / 2; ++i) {
        char temp = k_str[i];
        k_str[i] = k_str[usr_length - 1 - i];
        k_str[usr_length - 1 - i] = temp;
    }
    
    printk("The reversed string: %s\n", k_str);
    
    return 0; // Return 0 when executed successfully
}