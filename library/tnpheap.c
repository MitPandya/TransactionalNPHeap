//// Project 2: Mitkumar Pandya, mhpandya; Rachit Shrivastava, rshriva; Yash Vora, yvora;
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

//pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;


__u64 curr_transaction_id = -1;

struct transaction_node *head = NULL;

__u64 global_version = 0;

// Method to add a node to a list
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

        fprintf(stdout,"inserted head, offset is %llu\n",head->offset);
        return 1;
    }

    struct transaction_node* tmp = head;

    while(tmp->next != NULL) {
        tmp = tmp->next;
    }

    struct transaction_node* next_node = (struct transaction_node*)malloc(sizeof(struct transaction_node));
    if(next_node == NULL){
        fprintf(stderr,"error in malloc, node creation failed\n");
        return -1;
    }
    next_node->offset = offset;
    next_node->version = version;
    next_node->buffer = NULL;
    next_node->next = NULL;
    //next_node->kmem_ptr = NULL;
    next_node->size = 0;

    tmp->next = next_node;
    //fprintf(stdout,"inserted node, offset is %llu\n",next_node->offset);

    return 1;
}


// Method to find a node in the list based on Offset
struct transaction_node* find_list(__u64 offset) {
    //print_list(); 
    if(head == NULL){
        fprintf(stderr,"list is empty, inside find\n");
    }

    struct transaction_node* tmp = head;
    while(tmp != NULL) {
        if(tmp->offset == offset) {
            //fprintf(stdout, "found node %llu\n", tmp->offset);
            return tmp;
        }
        tmp = tmp->next;
    }
    //failed - not found
    return NULL;
}
// Method to print user space linked list
void print_list() {
    if(head == NULL){
        fprintf(stderr,"list is empty, inside print_list\n");
    }
    struct transaction_node* tmp = head;
    while(tmp != NULL) {
        fprintf(stdout, "node->version: %llu, node->offset: %llu\n", tmp->version, tmp->offset);
        tmp = tmp->next;
    }
}

// Method to iterate and free the nodes of user space list
void free_list(){
    fprintf(stdout,"inside free list\n");
    if(head == NULL){
        fprintf(stderr,"list is empty, inside free_list\n");
    }
    struct transaction_node* tmp;
    while(head != NULL) {
        tmp = head;
        head = head->next;
        if(tmp->buffer != NULL)
            free(tmp->buffer);
        free(tmp);
    }
    head = NULL;
    fprintf(stdout,"done free list\n");

}

// Method to get version of the node identified by offset from the kernel
// also adds a node if it does not exist
__u64 tnpheap_get_version(int npheap_dev, int tnpheap_dev, __u64 offset)
{
    //pthread_mutex_lock(&lock);
    struct tnpheap_cmd cmd;
    cmd.offset = offset;

    __u64 version = ioctl(tnpheap_dev, TNPHEAP_IOCTL_GET_VERSION, &cmd);

    struct transaction_node *tmp = find_list(offset);

    if(tmp == NULL){
        insert_list(version, offset);
    }
    //print_list();
    
    //fprintf(stdout,"Offest is %llu and version is %llu\n",offset,version);
    //pthread_mutex_unlock(&lock);
    return version;
}


// Handler which kills a process on segfault, should not come here
int tnpheap_handler(int sig, siginfo_t *si)
{
    fprintf(stderr,"Inside TNPHeap Handler sig: %d, error_no: %d, pid: %d\n\n", sig, si->si_errno, getpid());

    signal(sig, SIGSEGV);
    kill(getpid(), 9);

    return 0;
}

// Method to allocate user buffer data based on size and offset
void *tnpheap_alloc(int npheap_dev, int tnpheap_dev, __u64 offset, __u64 size)
{
    //pthread_mutex_lock(&lock);
    //fprintf(stdout,"inside alloc offset is %llu\n",offset);

    __u64 version = tnpheap_get_version(npheap_dev, tnpheap_dev, offset);

    __u64 aligned_size = ((size + getpagesize() - 1) / getpagesize())*getpagesize();

    //fprintf(stdout,"aligned size is %llu %llu\n",aligned_size,size);

    struct transaction_node *tmp = find_list(offset);
    if(tmp == NULL){
        fprintf(stderr,"error in alloc\n");
        //pthread_mutex_unlock(&lock);
        return NULL;
    }
    if(tmp->buffer != NULL)
        free(tmp->buffer);
    //tmp->kmem_ptr = ptr;
    if(aligned_size != 0)
        tmp->size = aligned_size;

    tmp->buffer = (char *)malloc(tmp->size);

    if(tmp->buffer == NULL){
        fprintf(stderr,"error in user malloc\n");
        //pthread_mutex_unlock(&lock);
        return NULL;
    }

    //fprintf(stdout,"usable size is %llu\n",malloc_usable_size(tmp->buffer));
    memset(tmp->buffer, 0, tmp->size);
    //fprintf(stdout,"exit alloc\n");
    //pthread_mutex_unlock(&lock);
    return tmp->buffer;
}

// Method to start a transaction and returns a transaction id
__u64 tnpheap_start_tx(int npheap_dev, int tnpheap_dev)
{	
    struct tnpheap_cmd cmd;
	__u64 transaction_id = ioctl(tnpheap_dev, TNPHEAP_IOCTL_START_TX, &cmd);

    /*if (pthread_mutex_init(&lock, NULL) != 0)
    {
        fprintf(stderr,"\n mutex init failed\n");
        return -1;
    }
    lock_initialized = 0;*/
    curr_transaction_id = transaction_id;
	return transaction_id;
}

// Method to commit a transaction, return 0 on success 1 on failure
int tnpheap_commit(int npheap_dev, int tnpheap_dev)
{
    //print_list();
    npheap_lock(npheap_dev,7);
    fprintf(stdout,"inside commit , transaction is %llu\n",curr_transaction_id);
    //pthread_mutex_lock(&lock);
    struct transaction_node *tmp = head;
	struct tnpheap_cmd cmd;

    int nodes = 0;
    int match = 0;

    if(tmp == NULL)
        return 1;

    while(tmp != NULL) {
        __u64 version = tnpheap_get_version(npheap_dev, tnpheap_dev, tmp->offset);

        if(tmp->version == version){
            match++;
        }else{
            fprintf(stderr,"Node version diff %llu %llu %llu\n",tmp->offset, tmp->version, version);
        }

        tmp = tmp->next;
        nodes++;
    }
    tmp = head;
    
    if(match == nodes && match > 0){
        while(tmp != NULL){
                cmd.offset = tmp->offset;
                cmd.version = tmp->version;

                npheap_delete(npheap_dev, cmd.offset);
                
                void *ptr = npheap_alloc(npheap_dev, cmd.offset, tmp->size); //tmp->size

                void *tmp_ptr = (char *)malloc(tmp->size);
                memset(tmp_ptr,0,tmp->size);

                int cmp = memcmp(tmp_ptr, tmp->buffer, tmp->size);

                free(tmp_ptr);

                if(cmp != 0){
                    fprintf(stdout,"write to k memory data is %llu %d %.8s %llu %llu\n",cmd.offset,cmp,tmp->buffer,curr_transaction_id,tmp->size);

                    memset((char *)ptr, 0, tmp->size);
                    memcpy((char *)ptr, tmp->buffer, tmp->size);

                     __u64 commit = ioctl(tnpheap_dev, TNPHEAP_IOCTL_COMMIT, &cmd);

                    if(commit == 1){

                        memset((char *)ptr, 0, tmp->size);
                        //pthread_mutex_unlock(&lock);
                        npheap_unlock(npheap_dev,7); 
                        return 1;
                    }

                    memset(tmp->buffer, 0, tmp->size);

                }else{
                    //npheap_unlock(npheap_dev,cmd.offset);
                    fprintf(stdout,"buffer is empty %llu\n",cmd.offset);
                }           
                
                fprintf(stdout,"done\n");
                fprintf(stdout, "Commit Successful\n");
                tmp = tmp->next;
        }

    }
    else{
        fprintf(stderr,"version mismatch %d %d transaction is %llu\n",nodes, match, curr_transaction_id);
        //pthread_mutex_unlock(&lock);
        free_list();
        npheap_unlock(npheap_dev,7); 
        return 1;
    }
    //head = NULL;
    free_list();
    npheap_unlock(npheap_dev,7); 
    //pthread_mutex_unlock(&lock);
    //pthread_mutex_destroy(&lock);
    fprintf(stdout,"all commit should exit transaction %llu\n",curr_transaction_id);
    return 0;
}
