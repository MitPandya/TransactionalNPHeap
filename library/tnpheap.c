#include <npheap/tnpheap_ioctl.h>
#include <npheap/npheap.h>
#include <npheap.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include <malloc.h>
#include <string.h>
#include <pthread.h>


struct transaction_node {
    __u64 version;
    __u64 offset;
    char* buffer;
    //void* kmem_ptr;
    __u64 size;
    struct transaction_node* next;
};

pthread_mutex_t lock;


struct transaction_node *head;

__u64 global_version = 0;

int insert_list(__u64 version, __u64 offset) {
    if(head == NULL) {
        head = (struct transaction_node*)malloc(sizeof(struct transaction_node));
        if(head == NULL){
            fprintf(stdout,"error in malloc, node creation failed\n");
            return -1;
        }
        head->offset = offset;
        head->version = version;
        head->buffer = NULL;
        head->next = NULL;
        //head->kmem_ptr = NULL;
        head->size = 0;

        fprintf(stdout,"inserted head, offset is %x\n",head->offset);
        return 1;
    }

    struct transaction_node* tmp = head;

    while(tmp->next != NULL) {
        tmp = tmp->next;
    }

    struct transaction_node* next_node = (struct transaction_node*)malloc(sizeof(struct transaction_node));
    if(next_node == NULL){
        fprintf(stdout,"error in malloc, node creation failed\n");
        return -1;
    }
    next_node->offset = offset;
    next_node->version = version;
    next_node->buffer = NULL;
    next_node->next = NULL;
    //next_node->kmem_ptr = NULL;
    next_node->size = 0;

    tmp->next = next_node;
    fprintf(stdout,"inserted node, offset is %x\n",next_node->offset);

    return 1;
}

struct transaction_node* find_list(__u64 offset) {
    //print_list(); 
    if(head == NULL){
        fprintf(stdout,"list is empty, inside find\n");
    }

    struct transaction_node* tmp = head;
    while(tmp != NULL) {
        if(tmp->offset == offset) {
            fprintf(stdout, "found node %x\n", tmp->offset);
            return tmp;
        }
        tmp = tmp->next;
    }
    //failed - not found
    return NULL;
}

void print_list() {
    if(head == NULL){
        fprintf(stdout,"list is empty, inside print_list\n");
    }
    struct transaction_node* tmp = head;
    while(tmp != NULL) {
        fprintf(stdout, "node->version: %zu, node->offset: %zu\n", tmp->version, tmp->offset);
        tmp = tmp->next;
    }
}

__u64 tnpheap_get_version(int npheap_dev, int tnpheap_dev, __u64 offset)
{
    struct tnpheap_cmd cmd;
    cmd.offset = offset;

    __u64 version = ioctl(tnpheap_dev, TNPHEAP_IOCTL_GET_VERSION, &cmd);

    //struct transaction_node *tmp = find_list(offset);

    //if(tmp == NULL){
        insert_list(version, offset);
    //}
    //print_list();
    
    fprintf(stdout,"Offest is %zu and version is %zu\n",offset,version);
    return version;
}



int tnpheap_handler(int sig, siginfo_t *si)
{
    printf("Inside TNPHeap Handler sig: %d, error_no: %d, pid: %d\n\n", sig, si->si_errno, getpid());

    signal(sig, SIGSEGV);
    kill(getpid(), 9);

    return 0;
}


void *tnpheap_alloc(int npheap_dev, int tnpheap_dev, __u64 offset, __u64 size)
{
    fprintf(stdout,"inside alloc\n");

    __u64 version = tnpheap_get_version(npheap_dev, tnpheap_dev, offset);

    __u64 aligned_size = ((size + getpagesize() - 1) / getpagesize())*getpagesize();
    fprintf(stdout,"aligned size is %zu\n",aligned_size);

    //void *ptr = npheap_alloc(npheap_dev, offset, aligned_size);

    /*if(ptr == NULL){
        fprintf(stderr,"error in kmalloc\n");
        return NULL;
    }

    fprintf(stdout,"size is %zu %zu \n", sizeof(ptr), sizeof(&ptr));*/

    struct transaction_node *tmp = find_list(offset);
    if(tmp == NULL){
        fprintf(stderr,"error in alloc\n");
        return NULL;
    }

    //tmp->kmem_ptr = ptr;
    tmp->size = aligned_size;
    tmp->buffer = (char *)malloc(sizeof(aligned_size));

    if(tmp->buffer == NULL){
        fprintf(stderr,"error in user malloc\n");
        return NULL;
    }

    fprintf(stdout,"usable size is %zu\n",malloc_usable_size(tmp->buffer));
    memset(tmp->buffer, 0, sizeof(aligned_size));
    fprintf(stdout,"exit alloc\n");
    return tmp->buffer;
}

__u64 tnpheap_start_tx(int npheap_dev, int tnpheap_dev)
{	
    struct tnpheap_cmd cmd;
	__u64 transaction_id = ioctl(tnpheap_dev, TNPHEAP_IOCTL_START_TX, &cmd);

    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        fprintf(stderr,"\n mutex init failed\n");
        return -1;
    }
 
	return transaction_id;
}

int tnpheap_commit(int npheap_dev, int tnpheap_dev)
{
    fprintf(stdout,"inside commit\n");
    pthread_mutex_lock(&lock);
    struct transaction_node *tmp = head;
	struct tnpheap_cmd cmd;

    while(tmp != NULL) {
        cmd.offset = tmp->offset;
        cmd.version = tmp->version;

        __u64 version = tnpheap_get_version(npheap_dev, tnpheap_dev, cmd.offset);

        if(cmd.version == version){

            //memcpy((char *)tmp->kmem_ptr, tmp->buffer, tmp->size);
            npheap_lock(npheap_dev,cmd.offset);
            /*if(sprintf((char *)tmp->kmem_ptr, "%s",tmp->buffer) <= 0){
                fprintf(stdout,"error writing to npheap\n");
                pthread_mutex_unlock(&lock);
                pthread_mutex_destroy(&lock);
                npheap_unlock(npheap_dev,cmd.offset);
                return 1;
            }*/
            void *ptr = npheap_alloc(npheap_dev, cmd.offset, tmp->size);
            /*fprintf(stdout,"memset");
            memset((char *)ptr, 0, tmp->size);
            fprintf(stdout,"memcpy");*/
            memcpy((char *)ptr, tmp->buffer, tmp->size);
            /*if (copy_from_user(ptr, tmp->buffer, tmp->size) != 0){
                return -EFAULT;
            }*/
            fprintf(stdout,"done\n");
            npheap_unlock(npheap_dev,cmd.offset);
            __u64 commit = ioctl(tnpheap_dev, TNPHEAP_IOCTL_COMMIT, &cmd);

            if(commit == 1){

                memset((char *)ptr, 0, tmp->size);
                pthread_mutex_unlock(&lock);
                pthread_mutex_destroy(&lock);
                return 1;
            }

           // head = NULL;
            fprintf(stdout, "Commit Successful\n");
            pthread_mutex_unlock(&lock);
            pthread_mutex_destroy(&lock);
            return 0;


        }
        else{
            tmp = tmp->next;
        }
        /*__u64 commit = ioctl(tnpheap_dev, TNPHEAP_IOCTL_COMMIT, &cmd);

        if(commit == 1) {
            fprintf(stderr, "kernel sent 1 as commit message\n");
            return commit;
        }*/
    }
    pthread_mutex_unlock(&lock);
    pthread_mutex_destroy(&lock);
    return 0;
}
