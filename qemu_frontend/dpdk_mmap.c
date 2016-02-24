
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>
#include "vswitch_vlink.h"

#define DPDK_MMAP_FILE_NAME "/tmp/dpdk-hugepage-mapping"
#define MAX_MMAP_PAGES 2048 /*for 2048kB hugepage ,these space host 4GB ,which should be large enough*/
#define MAX_HUGE_FILE_PATH
#define PAGE_SIZE 0x200000
struct mmap_record{
	long long vma_before_mmap;
	long long vma_after_mmap;
	long long vma_span;
	long long phy_addr;
	char huge_file_path[32];
};
static struct mmap_record mr_arr[MAX_MMAP_PAGES];
static int mr_num=0;

static void read_mmap_record_from_mmap_file(void)
{
	FILE* fp=fopen(DPDK_MMAP_FILE_NAME,"r");
	if(!fp)
		return ;
	char vma_before_mmap[32];
	char vma_after_mmap[32];
	char vma_span[32];
	char phy_addr[32];
	char huge_file_path[64];
	while(!feof(fp)){
		memset(vma_before_mmap,0x0,sizeof(vma_before_mmap));
		memset(vma_after_mmap,0x0,sizeof(vma_after_mmap));
		memset(vma_span,0x0,sizeof(vma_span));
		memset(phy_addr,0x0,sizeof(phy_addr));
		memset(huge_file_path,0x0,sizeof(huge_file_path));
		fscanf(fp,"%s%s%s%s%s",vma_before_mmap,vma_after_mmap,vma_span,phy_addr,huge_file_path);
		if(huge_file_path[0]=='\0')
			break;
		mr_arr[mr_num].vma_before_mmap=strtol(vma_before_mmap,NULL,16);
		mr_arr[mr_num].vma_after_mmap=strtol(vma_after_mmap,NULL,16);
		mr_arr[mr_num].vma_span=strtol(vma_span,NULL,16);
		mr_arr[mr_num].phy_addr=strtol(phy_addr,NULL,16);
		strncpy(mr_arr[mr_num].huge_file_path,huge_file_path,sizeof(mr_arr[mr_num].huge_file_path)-1);
		
		#if 0
		printf("%llx ",mr_arr[mr_num].vma_before_mmap);
		printf("%llx ",mr_arr[mr_num].vma_after_mmap);
		printf("%llx ",mr_arr[mr_num].vma_span);
		printf("%llx  %s\n",mr_arr[mr_num].phy_addr,mr_arr[mr_num].huge_file_path);
		#endif
		mr_num++;
	}
	fclose(fp);
	printf("[x] there are %d hugepages found\n",mr_num);
}

static int validate_mmap_record(void)
{
	int idx=0;
	for(idx=0;idx<mr_num;idx++){
		if(mr_arr[idx].vma_before_mmap!=mr_arr[idx].vma_after_mmap)
			return -1;
		if(idx!=0&&mr_arr[idx].vma_after_mmap!=mr_arr[idx-1].vma_span)
			return -1;
	}
	return 0;
}
static void * preserve_virtual_continuous_addr_space(int nb_hugepage,int page_size)
{
	void * addr=NULL;
	int fd=open("/dev/zero",O_RDONLY);
	if(fd<0)
		return NULL;
	addr=mmap(0,page_size*(nb_hugepage+2), PROT_READ, MAP_PRIVATE, fd, 0);
	if(addr==MAP_FAILED)
		goto error;
	printf("[x]original addr:0x%llx\n",(long long)addr);
	munmap(addr,page_size*(nb_hugepage+2));
	
	addr=(void*)((page_size+(long long)addr)&(long long)(~(page_size-1)));
	printf("[x]trimmed  addr:0x%llx\n",(long long)addr);
	
	return addr;
	error:
		close(fd);
		return NULL;
}
static int remap_huge_pages(void* addr)
{
	int fd;
	int idx=0;
	long long vma_before_map;
	long long vma_after_map;
	
	long long base_addr=(long long)addr;
	if(base_addr==0)
		return -1;
	for (idx=0;idx<mr_num;idx++){/*map a single file*/
		vma_before_map=base_addr+idx*PAGE_SIZE;
		fd=open(mr_arr[idx].huge_file_path,O_RDWR,0);
		if(fd<0)
			return -2;
		vma_after_map=(long long)mmap((void*)vma_before_map,PAGE_SIZE,PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if(vma_after_map!=vma_before_map){
			close(fd);
			return -3;
		}
		printf("[x]%llx %llx %llx %s\n",(long long)vma_before_map,(long long)vma_after_map,PAGE_SIZE+(long long)vma_after_map,mr_arr[idx].huge_file_path);
		close(fd);
			
	}
	return 0;
	
}
int attach_to_dpdk_hugepage_memory(void **base,int *total_size)
{	
	int rc=0;
	void *virt_base=NULL;
	mr_num=0;
	read_mmap_record_from_mmap_file();
	rc=validate_mmap_record();
	if(rc<0)
		return -1;
	virt_base=preserve_virtual_continuous_addr_space(mr_num,PAGE_SIZE);
	if(!virt_base)
		return -1;
	rc=remap_huge_pages(virt_base);
	*base=virt_base;
	*total_size=mr_num*PAGE_SIZE;
	return rc;

}
#if 0
int main()
{
	void *base;
	int rc;
	int size=0;
	rc=attach_to_dpdk_hugepage_memory(&base,&size);
	printf("%d %llx %x\n",rc,(long long)base,size);
	printf("%s\n",(char*)(0x1d75e80+(long long)base));
	getchar();
	return 0;
}
#endif
