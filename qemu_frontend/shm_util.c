#include "shm_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
void* shm_alloc(char * szname,int shm_size)
{
	void* ret=NULL;
	int fd;
	char szfilename[256];
	sprintf(szfilename,"/dev/shm/%s",szname);
	
	fd=open(szfilename,O_RDWR|O_CREAT,0); 
	if(fd<0)
		return NULL;
	ftruncate(fd,shm_size);
	ret=mmap(NULL,shm_size,PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	
	close(fd);
	return ret;
}
void shm_dealloc(void * shm,int size)
{
	munmap(shm,size);
}


