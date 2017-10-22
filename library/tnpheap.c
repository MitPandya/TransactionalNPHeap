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

char* buffer;

__u64 tnpheap_get_version(int npheap_dev, int tnpheap_dev, __u64 offset)
{
    return 0;
}



int tnpheap_handler(int sig, siginfo_t *si)
{
    return 0;
}


void *tnpheap_alloc(int npheap_dev, int tnpheap_dev, __u64 offset, __u64 size)
{
    __u64 aligned_size = ((size + getpagesize() - 1) / getpagesize())*getpagesize();

    //creating a buffer memory for transactions
    buffer = (char*) mmap(0,aligned_size,PROT_READ|PROT_WRITE,MAP_SHARED,npheap_dev,offset*getpagesize());

    return mmap(0,aligned_size,PROT_READ|PROT_WRITE,MAP_SHARED,tnpheap_dev,offset*getpagesize());
}

__u64 tnpheap_start_tx(int npheap_dev, int tnpheap_dev)
{
	struct tnpheap_cmd cmd;
	cmd.offset = npheap_dev;
	return tnpheap_ioctl(tnpheap_dev, TNPHEAP_IOCTL_START_TX, &cmd);
}

int tnpheap_commit(int npheap_dev, int tnpheap_dev)
{
    return 0;
}
