//////////////////////////////////////////////////////////////////////
//                             North Carolina State University
//
//
//
//                             Copyright 2016
//
////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or modify it
// under the terms and conditions of the GNU General Public License,
// version 2, as published by the Free Software Foundation.
//
// This program is distributed in the hope it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
//
////////////////////////////////////////////////////////////////////////
//
//   Author:  Hung-Wei Tseng
//
//   Description:
//     Skeleton of NPHeap Pseudo Device
//
////////////////////////////////////////////////////////////////////////

#include "tnpheap_ioctl.h"

#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/time.h>
#include <linux/list.h>


struct miscdevice tnpheap_dev;

DEFINE_MUTEX(lock);

__u64 transaction_id = 0;
__u64 global_version = 100;

struct linked_list{
    struct list_head list; /* kernel's list structure */
    __u64 version;
    __u64 offset;
};

struct linked_list linkedList;

struct linked_list* add_node(__u64 offset){
    struct linked_list *tmp;
    /* adding elements to list */
    tmp= (struct linked_list *)kmalloc(sizeof(struct linked_list), GFP_KERNEL);
    global_version++;
    tmp->version = global_version;
    tmp->offset = offset;
    /* add the new item 'tmp' to the list of items in linked_list */
    list_add(&(tmp->list), &(linkedList.list));
    return tmp;
}

struct linked_list* find_node(__u64 offset) {
    struct linked_list *tmp;
    struct list_head *pos, *q;
    // traverse through list and lock the current unocked node
    // https://isis.poly.edu/kulesh/stuff/src/klist/
    list_for_each_safe(pos, q, &linkedList.list) {
        tmp = list_entry(pos, struct linked_list, list);
        if(offset == tmp->offset) {
            return tmp;
        }
    }
    return NULL;
}



__u64 tnpheap_get_version(struct tnpheap_cmd __user *user_cmd)
{
    printk("inside get version\n");
    //mutex_lock(&lock);
    struct tnpheap_cmd cmd;
    if (copy_from_user(&cmd, user_cmd, sizeof(cmd)))
    {
        //mutex_unlock(&lock);
        return -1 ;
    }
    struct linked_list *node = find_node(cmd.offset);

    if(node == NULL){
        node = add_node(cmd.offset);
    }

    printk(KERN_INFO "Offest is %llu and version is %llu\n",cmd.offset,node->version);
    //mutex_unlock(&lock);
    return node->version;
}

__u64 tnpheap_start_tx(struct tnpheap_cmd __user *user_cmd)
{
    struct tnpheap_cmd cmd;
    __u64 ret=0;
    if (copy_from_user(&cmd, user_cmd, sizeof(cmd)))
    {
        return -1 ;
    }
    mutex_lock(&lock);
    transaction_id++;
    mutex_unlock(&lock);
    return transaction_id;
}

__u64 tnpheap_commit(struct tnpheap_cmd __user *user_cmd)
{
    printk(KERN_INFO "inside commit\n");
    //mutex_lock(&lock);
    struct tnpheap_cmd cmd;
    //__u64 ret=0;
    if (copy_from_user(&cmd, user_cmd, sizeof(cmd)))
    {
        //mutex_unlock(&lock);
        return 1;
    }
    struct linked_list* node = find_node(cmd.offset);
    if(node == NULL) {
        //node not found
        printk(KERN_ERR "Node not found! %llu\n", cmd.offset);
        //mutex_unlock(&lock);
        return 1;
    }
    if(cmd.version == node->version) {
        //version matches, write to kernel memory
        struct linked_list *tmp;
        /* adding elements to list */
        tmp= (struct linked_list *)kmalloc(sizeof(struct linked_list), GFP_KERNEL);
        global_version++;
        tmp->version = global_version;
        tmp->offset = cmd.offset;

        list_replace(&(node->list),&(tmp->list));

        kfree(node);
        printk(KERN_INFO "Commit success\n");
        //mutex_unlock(&lock);
        return 0;
    }
    printk(KERN_INFO "Commit error");
    //mutex_unlock(&lock);
    return 1;
}



__u64 tnpheap_ioctl(struct file *filp, unsigned int cmd,
                                unsigned long arg)
{
    switch (cmd) {
    case TNPHEAP_IOCTL_START_TX:
        return tnpheap_start_tx((void __user *) arg);
    case TNPHEAP_IOCTL_GET_VERSION:
        return tnpheap_get_version((void __user *) arg);
    case TNPHEAP_IOCTL_COMMIT:
        return tnpheap_commit((void __user *) arg);
    default:
        return -ENOTTY;
    }
}

static const struct file_operations tnpheap_fops = {
    .owner                = THIS_MODULE,
    .unlocked_ioctl       = tnpheap_ioctl,
};

struct miscdevice tnpheap_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "tnpheap",
    .fops = &tnpheap_fops,
};

static int __init tnpheap_module_init(void)
{
    int ret = 0;
    if ((ret = misc_register(&tnpheap_dev)))
        printk(KERN_ERR "Unable to register \"npheap\" misc device\n");
    else{
        printk(KERN_ERR "\"npheap\" misc device installed\n");
        INIT_LIST_HEAD(&linkedList.list);
        //mutex_init(&lock);  
    }
    return 1;
}

static void __exit tnpheap_module_exit(void)
{
    misc_deregister(&tnpheap_dev);
    return;
}

MODULE_AUTHOR("Hung-Wei Tseng <htseng3@ncsu.edu>");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
module_init(tnpheap_module_init);
module_exit(tnpheap_module_exit);

