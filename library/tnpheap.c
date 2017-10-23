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

struct transaction_node {
    __u64 version;
    long offset;
    char* buffer;
    struct transaction_node* next;
};

struct transaction_node *head;

__u64 global_version = 0;

int insert_list(__u64 version, long offset) {
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

    tmp->next = next_node;

    return 1;
}

struct transaction_node* find_list(long offset) {
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
        fprintf(stdout, "node->version: %x, node->offset: %zu\n", tmp->version, tmp->offset);
        tmp = tmp->next;
    }
}

__u64 tnpheap_get_version(int npheap_dev, int tnpheap_dev, __u64 offset)
{
    __u64 version = ioctl(tnpheap_dev, TNPHEAP_IOCTL_GET_VERSION, &cmd);
    return 0;
}



int tnpheap_handler(int sig, siginfo_t *si)
{
    return 0;
}


void *tnpheap_alloc(int npheap_dev, int tnpheap_dev, __u64 offset, __u64 size)
{
    __u64 aligned_size = ((size + getpagesize() - 1) / getpagesize())*getpagesize();
    return npheap_alloc(npheap_dev, offset, aligned_size);
}

__u64 tnpheap_start_tx(int npheap_dev, int tnpheap_dev)
{	
    struct tnpheap_cmd cmd;
	__u64 offset = ioctl(tnpheap_dev, TNPHEAP_IOCTL_START_TX, &cmd);
    fprintf(stdout,"Node added %zu \n", res);
    tnpheap_get_version(npheap_dev, tnpheap_dev, offset);
	return 0;
}

int tnpheap_commit(int npheap_dev, int tnpheap_dev)
{
	struct tnpheap_cmd cmd;
	return 0;
}
