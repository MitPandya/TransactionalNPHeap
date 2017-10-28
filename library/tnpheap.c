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


struct transaction_node *head = NULL;

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

        fprintf(stdout,"inserted head, offset is %llu\n",head->offset);
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
    fprintf(stdout,"inserted node, offset is %llu\n",next_node->offset);

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
            fprintf(stdout, "found node %llu\n", tmp->offset);
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
        fprintf(stdout, "node->version: %llu, node->offset: %llu\n", tmp->version, tmp->offset);
        tmp = tmp->next;
    }
}

__u64 tnpheap_get_version(int npheap_dev, int tnpheap_dev, __u64 offset)
{
    struct tnpheap_cmd cmd;
    cmd.offset = offset;

    __u64 version = ioctl(tnpheap_dev, TNPHEAP_IOCTL_GET_VERSION, &cmd);

    struct transaction_node *tmp = find_list(offset);

    if(tmp == NULL){
        insert_list(version, offset);
    }
    //print_list();
    
    fprintf(stdout,"Offest is %llu and version is %llu\n",offset,version);
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
    pthread_mutex_lock(&lock);
    fprintf(stdout,"inside alloc offset is %llu\n",offset);

    __u64 version = tnpheap_get_version(npheap_dev, tnpheap_dev, offset);

    __u64 aligned_size = ((size + getpagesize() - 1) / getpagesize())*getpagesize();

    fprintf(stdout,"aligned size is %llu %llu\n",aligned_size,size);

    struct transaction_node *tmp = find_list(offset);
    if(tmp == NULL){
        fprintf(stderr,"error in alloc\n");
        pthread_mutex_unlock(&lock);
        return NULL;
    }

    //tmp->kmem_ptr = ptr;
    if(aligned_size != 0)
        tmp->size = aligned_size;

    tmp->buffer = (char *)malloc(tmp->size);

    if(tmp->buffer == NULL){
        fprintf(stderr,"error in user malloc\n");
        pthread_mutex_unlock(&lock);
        return NULL;
    }

    fprintf(stdout,"usable size is %llu\n",malloc_usable_size(tmp->buffer));
    memset(tmp->buffer, 0, tmp->size);
    fprintf(stdout,"exit alloc\n");
    pthread_mutex_unlock(&lock);
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
    print_list();
    fprintf(stdout,"inside commit\n");
    pthread_mutex_lock(&lock);
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
        }

        tmp = tmp->next;
        nodes++;
    }
    tmp = head;
    
    if(match == nodes && match > 0){
        while(tmp != NULL){
                cmd.offset = tmp->offset;
                cmd.version = tmp->version;
                npheap_lock(npheap_dev,cmd.offset);
                void *ptr = npheap_alloc(npheap_dev, cmd.offset, cmd.size);

                if(memcmp((char *) ptr, tmp->buffer, tmp->size) != 0){
                    fprintf(stdout,"write to k memory %llu\n",cmd.offset);

                    memset((char *)ptr, 0, tmp->size);
                    memcpy((char *)ptr, tmp->buffer, tmp->size);

                    npheap_unlock(npheap_dev,cmd.offset); 

                     __u64 commit = ioctl(tnpheap_dev, TNPHEAP_IOCTL_COMMIT, &cmd);

                    if(commit == 1){

                        memset((char *)ptr, 0, tmp->size);
                        pthread_mutex_unlock(&lock);
                        return 1;
                    }   
                }else{
                    npheap_unlock(npheap_dev,cmd.offset);
                    fprintf(stdout,"same value in memory %llu\n",cmd.offset);
                }           
                
                fprintf(stdout,"done\n");
                fprintf(stdout, "Commit Successful\n");
                tmp = tmp->next;
        }

    }
    else{
        fprintf(stdout,"version mismatch %d %d\n",nodes,match);
        pthread_mutex_unlock(&lock);
        return 1;
    }
    //head = NULL;
    pthread_mutex_unlock(&lock);
    //pthread_mutex_destroy(&lock);
    fprintf(stdout,"all commit should exit\n");
    return 0;
}
