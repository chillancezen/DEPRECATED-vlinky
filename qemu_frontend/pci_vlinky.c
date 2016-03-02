
#include <sys/stat.h>  
#include <sys/socket.h> 
#include <sys/un.h>

#include "hw/hw.h"
#include "hw/pci/pci.h"
#include "qemu/osdep.h"
#include "./queue.h" //absolute path,since we have several  queue.h in qemu projection
#include "./shm_util.h"
#include "./vswitch_vlink.h"

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>

#define MSI_X_MAX_LINK 256
#define MSI_X_MAX_QUEUE 64

/*messgae transaction format
link-num  | channel-num |dpdk&qemu-context(0|1)|reserved
*/
struct fd_desc{
	int fd[MSI_X_MAX_QUEUE];
};

#define VLINKY_DEVCIE_NAME "pci_vlinky"
#define MSI_X_SERVER_PATH "/tmp/msi_x_server.sock"
enum vlink_bar0_register{/*io ports registers which reside in BAR 0*/
	VLINKY_DEVICE_STATUS=0,
	 VLINKY_QUEUES=4,
	 VLINKY_QUEUE_LENGTH=8,
	 VLINKY_QUEUE_INDEX=12,
	 VLINKY_QUEUE_DATA=16
};
struct queue_channel{
	struct queue_stub *rx_stub;
	struct queue_stub *free_stub;
	struct queue_stub *tx_stub;
	struct queue_stub *alloc_stub;
	int msi_x_vector;
	int interrup_enabled;
};
struct vlink_descriptor{
	int link_index;
	char ctrl_channel_file[32];
	int queues;
	int queues_length;

	int ctrl_channel_size;
	void *ctrl_channel_base;
	int hw_offset;

	struct queue_channel * stub_array;
};
typedef struct PCIVlinkyPrivate{
	PCIDevice pci_dev;

	char *config_file;/*updated configuration manner,because several vlinks share one QEMU frontend device*/
	int link_desc_number;
	struct vlink_descriptor * link_desc_arr;
	
	#if 0/*not used any more*/
	char * ctrl_channel_file;/*it should be any shm file,and it should be initilized by any other DPDK process*/
	int nr_queues;/*number of queues per device,*/
	int queue_length;/*length of queue ,not including stub space*/
	int ctrl_channel_size;
	void * ctrl_channel_base;
	#endif
	//struct queue_stub* *stub_array;
	//struct queue_channel* stub_array;
	void * dpdk_huge_base;
	int dpdk_huge_size;
	MemoryRegion dpdk_memory_bar;/*will be registered into BAR2*/
	MemoryRegion control_bar;/*will be registered into BAR0*/
	MemoryRegion ctrl_channel_bar;/*will be registered into BAR3*/

	MemoryRegion *ctrl_channel_sub_bar;
	
	int dummy[100];
}PCIVlinkyPrivate;
static Property vlinky_property[]={
	DEFINE_PROP_STRING("config_file",PCIVlinkyPrivate,config_file),/*
	DEFINE_PROP_STRING("ctrl_channel",PCIVlinkyPrivate,ctrl_channel_file),
	DEFINE_PROP_INT32("queues",PCIVlinkyPrivate,nr_queues,0),
	DEFINE_PROP_INT32("queue_length",PCIVlinkyPrivate,queue_length,0),*/
	DEFINE_PROP_END_OF_LIST(),
};
uint64_t pci_vlinky_bar0_io_read(void * opaque,hwaddr addr,unsigned size)
{

	return 0;
}
void pci_vlinky_bar0_io_write(void * opaque,hwaddr addr,uint64_t val,unsigned size)
{

}
struct MemoryRegionOps pci_vlinky_io_ops={
	.read=pci_vlinky_bar0_io_read,
	.write=pci_vlinky_bar0_io_write,
	.endianness=DEVICE_NATIVE_ENDIAN,
	.impl={
		.min_access_size=4,
		.max_access_size=4
	}

};
int pow_round(int val)
{
	int idx=0;
	int mask=0x1;int last_mask=0;
	for(idx=0;idx<31;idx++){
		
		mask=mask<<1;
		if(val&mask){
			last_mask=mask;
		}
	}
	return last_mask<<1;
	
}
int parse_config_file(PCIVlinkyPrivate *private)
{
	FILE *fp=fopen(private->config_file,"r");
	char buffer[256];
	int equal_char_index;
	char *key_ptr;
	char *val_ptr;
	char *tmp_ptr;
	int desc_index=0;
	assert(fp);
	while(!feof(fp)){
		memset(buffer,0x0,sizeof(buffer));
		fgets(buffer,sizeof(buffer),fp);
		equal_char_index=strchr(buffer,'=');
		if(!equal_char_index)
			continue;
		equal_char_index-=(int)buffer;
		
		tmp_ptr=buffer+equal_char_index;
		key_ptr=buffer;
		val_ptr=tmp_ptr+1;

		for(;*key_ptr==' ';key_ptr++);
		for(*tmp_ptr--='\x0';*tmp_ptr==' ';tmp_ptr--)
			*tmp_ptr='\x0';
		for(;*val_ptr==' ';val_ptr++);

		for(tmp_ptr=val_ptr;*tmp_ptr;tmp_ptr++);
		for(tmp_ptr--;*tmp_ptr==' '||*tmp_ptr=='\r'||*tmp_ptr=='\n';tmp_ptr--)
			*tmp_ptr='\x0';
		if(!strcmp(key_ptr,"vlinks")){
			private->link_desc_number=atoi(val_ptr);
			private->link_desc_arr=g_malloc0(atoi(val_ptr)*sizeof(struct vlink_descriptor));
			
		}else if(!strcmp(key_ptr,"record_index")){
			desc_index=atoi(val_ptr);
		}else if (!strcmp(key_ptr,"link_index")){
			
			private->link_desc_arr[desc_index].link_index=atoi(val_ptr);
			
		}else if (!strcmp(key_ptr,"ctrl_channel_file")){
			strcpy(private->link_desc_arr[desc_index].ctrl_channel_file,val_ptr);

		}else if(!strcmp(key_ptr,"queues")){
			private->link_desc_arr[desc_index].queues=atoi(val_ptr);
			
		}else if(!strcmp(key_ptr,"queues_length")){
			private->link_desc_arr[desc_index].queues_length=atoi(val_ptr);
			
		}
	}
	fclose(fp);
	return 0;
}
void* msi_x_server_thread(void*arg)
{
	PCIVlinkyPrivate* private=arg;
	int rc;
	int record_num;
	int channel_num;
	int link_num;
	int link_index_array[MSI_X_MAX_LINK];
	struct fd_desc fd_desc_arr[MSI_X_MAX_LINK];
	struct sockaddr_un un;
	char buffer[4];
	
	memset(&un,0x0,sizeof(un));
	un.sun_family=AF_UNIX;
	strcpy(un.sun_path,MSI_X_SERVER_PATH);
	int idx,idx_tmp;
	for(idx=0;idx<MSI_X_MAX_LINK;idx++){
		link_index_array[idx]=-1;
		for(idx_tmp=0;idx_tmp<MSI_X_MAX_QUEUE;idx_tmp++)
		fd_desc_arr[idx].fd[idx_tmp]=-1;
	}
	int nfds;
	struct epoll_event ev,events[256];
	int epfd=epoll_create(256);
	for(idx=0;idx<private->link_desc_number;idx++){
		link_index_array[private->link_desc_arr[idx].link_index]=idx;/*here we keep the local index symbol*/
		for(idx_tmp=0;idx_tmp<private->link_desc_arr[idx].queues;idx_tmp++){
			fd_desc_arr[idx].fd[idx_tmp]=socket(AF_UNIX,SOCK_STREAM,0);
			rc=connect(fd_desc_arr[idx].fd[idx_tmp], (struct sockaddr *)&un, sizeof(un));
			assert(rc==0);

			ev.data.fd=fd_desc_arr[idx].fd[idx_tmp];
			ev.events=EPOLLIN|EPOLLET;
			epoll_ctl(epfd,EPOLL_CTL_ADD,fd_desc_arr[idx].fd[idx_tmp],&ev);
			
			buffer[0]=private->link_desc_arr[idx].link_index;
			buffer[1]=idx_tmp;
			buffer[2]=1;
			printf("register:%d %d\n",buffer[0],buffer[1]);
			write(fd_desc_arr[idx].fd[idx_tmp],buffer,4);
		}
	}
	while(1){

		nfds=epoll_wait(epfd,events,256,100);
		for(idx=0;idx<nfds;idx++)
		if (events[idx].events&EPOLLIN){
			read(events[idx].data.fd,buffer,4);

			link_num=buffer[0];
			channel_num=buffer[1];
			record_num=link_index_array[link_num];
			
			printf("msi-x notify  %d.%d:%d\n",link_num,channel_num,private->link_desc_arr[record_num].stub_array[channel_num].msi_x_vector);
			if(private->link_desc_arr[record_num].stub_array[channel_num].interrup_enabled){
				msix_notify(&private->pci_dev,private->link_desc_arr[record_num].stub_array[channel_num].msi_x_vector);
			}
		}

	}
	
	#if 0

	PCIVlinkyPrivate* private=arg;
	int rc;
	int idx;
	int fd_client;
	int queue_num;
	int link_num;
	int fd=socket(AF_UNIX,SOCK_STREAM,0);
	struct sockaddr_un un;
	struct sockaddr_un un_client;
	int un_len;
	char buffer[4];
	unlink(MSI_X_SERVER_PATH);
	memset(&un,0x0,sizeof(un));
	un.sun_family=AF_UNIX;
	strcpy(un.sun_path,MSI_X_SERVER_PATH);
	assert(fd>=0);
	rc=bind(fd,(struct sockaddr*)&un,sizeof(struct sockaddr_un));
	assert(fd>=0);
	rc=listen(fd,5);
	assert(fd>=0);
	//using epoll-model to handle event
	struct epoll_event ev,events[256];
	int epfd=epoll_create(256);
	int nfds;
	ev.data.fd=fd;
	ev.events=EPOLLIN;
	epoll_ctl(epfd,EPOLL_CTL_ADD,fd,&ev);
	while(1){
		nfds=epoll_wait(epfd,events,256,100);
		for(idx=0;idx<nfds;idx++){
			if(events[idx].data.fd==fd){
				memset(&un_client,0x0,sizeof(un_client));
				un_len=sizeof(struct sockaddr_un);
				fd_client=accept(fd,(struct sockaddr*)&un_client,&un_len);
				ev.data.fd=fd_client;
				ev.events=EPOLLIN|EPOLLET;
				epoll_ctl(epfd,EPOLL_CTL_ADD,fd_client,&ev);
			}else if (events[idx].events&EPOLLIN){
				read(events[idx].data.fd,buffer,4);
				printf("%d %d\n",buffer[0],buffer[1]);
				
			}
		}
	}
	
	
	while(1){
		memset(&un_client,0x0,sizeof(un_client));
		un_len=sizeof(un_client);
		fd_client=accept(fd,(struct sockaddr*)&un_client,&un_len);
		
		while(1){
			rc=read(fd_client,buffer,sizeof(buffer));
			if(rc<=0)
				break;
			link_num=buffer[0];
			queue_num=buffer[1];

			/*msi-x notify*/
		
			
		}
		
	}
	#endif
	return NULL;

}
int pci_vlinky_dev_init(PCIDevice *pdev)
{
	pthread_t pid;
	int rc;
	int idx,idx_tmp;
	PCIVlinkyPrivate *private=OBJECT_CHECK(PCIVlinkyPrivate,pdev,VLINKY_DEVCIE_NAME);
	pdev->config[PCI_INTERRUPT_PIN]=0x2;
	pdev->config[PCI_COMMAND]=PCI_COMMAND_IO|PCI_COMMAND_MEMORY;
	int hw_offset=0;
	char buffer[32];

	char msi_x_entries;
	/*before we get everything started,we should parse config file */
	#if 0
	printf("[x]conf_file:%s\n",private->config_file);
	printf("[x]ctrl shm file:%s\n",private->ctrl_channel_file);
	printf("[x]ctrl queues  :%d\n",private->nr_queues);
	printf("[x]ctrl queue len:%d\n",private->queue_length);
	#endif
	parse_config_file(private);
	/*dump config*/
	for(idx=0;idx<private->link_desc_number;idx++){
		printf("[x]link index:%d\n",private->link_desc_arr[idx].link_index);
		printf("[x]link ctrl_file:%s\n",private->link_desc_arr[idx].ctrl_channel_file);
		printf("[x]link queues:%d\n",private->link_desc_arr[idx].queues);
		printf("[x]link queues_length:%d\n",private->link_desc_arr[idx].queues_length);
	}
	
	/*1st, we got dpdk hugepae mapping address,and makeup BAR 2*/
	rc=attach_to_dpdk_hugepage_memory(&private->dpdk_huge_base,&private->dpdk_huge_size);
	printf("[x] rx:%d dpdk huge base:%llx (%d)\n",rc,private->dpdk_huge_base,private->dpdk_huge_size);
	if(rc){
		printf("[x]something maybe wrong with DPDK memory mapping\n");
		return -1;
	}
	memory_region_init_ram_ptr(&private->dpdk_memory_bar,OBJECT(private),"dpdk_memory_container",private->dpdk_huge_size,private->dpdk_huge_base);
#if 0
	void * ptr1;
	void * ptr2;
	MemoryRegion mr_tmp1;
	MemoryRegion mr_tmp2;
	ptr1=shm_alloc("cute",256);
	ptr2=shm_alloc("cute1",128);
	printf("ptr1:%llx\n",(long long)ptr1);
	printf("ptr2:%llx\n",(long long)ptr2);
	Error *error_abort;
    strcpy(ptr1,"hello ");
	strcpy(ptr2,"vlinky");
	
	private->cute_bar_ptr=g_malloc(2*sizeof(struct MemoryRegion));
	
	memory_region_init_ram_ptr(&private->cute_bar_ptr[0],OBJECT(private),"cute1-meeow",256,ptr1);
	memory_region_init_ram_ptr(&private->cute_bar_ptr[1],OBJECT(private),"cute2-meeeo",128,ptr2);
	
	memory_region_add_subregion(&private->dpdk_memory_bar,0,&private->cute_bar_ptr[0]);
	memory_region_add_subregion(&private->dpdk_memory_bar,256,&private->cute_bar_ptr[1]);
#endif
         
	pci_register_bar(pdev,2,PCI_BASE_ADDRESS_SPACE_MEMORY|PCI_BASE_ADDRESS_MEM_PREFETCH,&private->dpdk_memory_bar);

	
	/*2nd,register BAR 0 for IO ports*/
	memory_region_init_io(&private->control_bar,OBJECT(private),&pci_vlinky_io_ops,private,"contron_bar",0x100);/*suppose 0x100 is large enough*/
	pci_register_bar(pdev,0,PCI_BASE_ADDRESS_SPACE_IO,&private->control_bar);
	
	
	/*4th,register queue channel into BAR 3,first we should make sure this shm area is initialized first by DPDK pmd driver*/
	//private->stub_array=(struct queue_stub **)g_malloc0(private->nr_queues*4*sizeof(struct queue_stub *));
	
	#if 0
	private->stub_array=(struct queue_channel*)g_malloc0(private->nr_queues*sizeof(struct queue_channel));
	private->ctrl_channel_size=private->nr_queues*4*(sizeof(struct queue_stub)+sizeof(struct queue_element)*(private->queue_length+1));
	private->ctrl_channel_base=shm_alloc(private->ctrl_channel_file,private->ctrl_channel_size);
	
	for(idx=0;idx<private->nr_queues;idx++){
		private->stub_array[idx].rx_stub=(struct queue_stub*)((idx*4+0)*(sizeof(struct queue_stub)+sizeof(struct queue_element)*(private->queue_length+1))+(unsigned char*)private->ctrl_channel_base);
		private->stub_array[idx].free_stub=(struct queue_stub*)((idx*4+1)*(sizeof(struct queue_stub)+sizeof(struct queue_element)*(private->queue_length+1))+(unsigned char*)private->ctrl_channel_base);
		private->stub_array[idx].tx_stub=(struct queue_stub*)((idx*4+2)*(sizeof(struct queue_stub)+sizeof(struct queue_element)*(private->queue_length+1))+(unsigned char*)private->ctrl_channel_base);
		private->stub_array[idx].alloc_stub=(struct queue_stub*)((idx*4+3)*(sizeof(struct queue_stub)+sizeof(struct queue_element)*(private->queue_length+1))+(unsigned char*)private->ctrl_channel_base);
	}
	
	for(idx=0;idx<private->nr_queues;idx++){
		printf("rx1:%d:%d %d\n",idx,(private->stub_array[idx].rx_stub->queue_channel_state&QUEUE_CHANNEL_ACTIVE),queue_quantum(private->stub_array[idx].rx_stub));
		printf("rx2:%d:%d %d\n",idx,(private->stub_array[idx].free_stub->queue_channel_state&QUEUE_CHANNEL_ACTIVE),queue_quantum(private->stub_array[idx].free_stub));
		printf("rx3:%d:%d %d\n",idx,(private->stub_array[idx].tx_stub->queue_channel_state&QUEUE_CHANNEL_ACTIVE),queue_quantum(private->stub_array[idx].tx_stub));
		printf("rx4:%d:%d %d\n",idx,(private->stub_array[idx].alloc_stub->queue_channel_state&QUEUE_CHANNEL_ACTIVE),queue_quantum(private->stub_array[idx].alloc_stub));
	}
	
	memory_region_init(&private->ctrl_channel_bar,OBJECT(private),"ctrl_channel_bar",pow_round(private->ctrl_channel_size));
	pci_register_bar(pdev,3,PCI_BASE_ADDRESS_SPACE_MEMORY|PCI_BASE_ADDRESS_MEM_PREFETCH,&private->ctrl_channel_bar);
	#endif
	for(idx=0;idx<private->link_desc_number;idx++){
		private->link_desc_arr[idx].ctrl_channel_size=private->link_desc_arr[idx].queues*4*(sizeof(struct queue_stub)+sizeof(struct queue_element)*(private->link_desc_arr[idx].queues_length+1));
		private->link_desc_arr[idx].ctrl_channel_base=shm_alloc(private->link_desc_arr[idx].ctrl_channel_file,private->link_desc_arr[idx].ctrl_channel_size);
		hw_offset+=pow_round(private->link_desc_arr[idx].ctrl_channel_size);
		
	}
	private->ctrl_channel_sub_bar=g_malloc0(private->link_desc_number*sizeof(struct MemoryRegion));
	//printf("total_length:%d\n",hw_offset);
	memory_region_init(&private->ctrl_channel_bar,OBJECT(private),"ctrl_channel_bar_container",pow_round(hw_offset));
	hw_offset=0;
	for(idx=0;idx<private->link_desc_number;idx++){
		sprintf(buffer,"ctrl_sub_bar%d",idx);
		memory_region_init_ram_ptr(&private->ctrl_channel_sub_bar[idx],OBJECT(private),buffer,private->link_desc_arr[idx].ctrl_channel_size,private->link_desc_arr[idx].ctrl_channel_base);
		memory_region_add_subregion(&private->ctrl_channel_bar,hw_offset,&private->ctrl_channel_sub_bar[idx]);
		private->link_desc_arr[idx].hw_offset=hw_offset;/*offset in the ctrl-channel-bar*/
		printf("hw offset:%d\n",private->link_desc_arr[idx].hw_offset);
		hw_offset+=pow_round(private->link_desc_arr[idx].ctrl_channel_size);
	}
	pci_register_bar(pdev,3,PCI_BASE_ADDRESS_SPACE_MEMORY|PCI_BASE_ADDRESS_MEM_PREFETCH,&private->ctrl_channel_bar);
	
	for(idx=0;idx<private->link_desc_number;idx++){
		private->link_desc_arr[idx].stub_array=g_malloc0(private->link_desc_arr[idx].queues*sizeof(struct queue_channel));
		for(idx_tmp=0;idx_tmp<private->link_desc_arr[idx].queues;idx_tmp++){
			private->link_desc_arr[idx].stub_array[idx_tmp].rx_stub=(struct queue_stub*)((idx_tmp*4+0)*(sizeof(struct queue_stub)+sizeof(struct queue_element)*(private->link_desc_arr[idx].queues_length+1))+(unsigned char*)private->link_desc_arr[idx].ctrl_channel_base);
			private->link_desc_arr[idx].stub_array[idx_tmp].free_stub=(struct queue_stub*)((idx_tmp*4+1)*(sizeof(struct queue_stub)+sizeof(struct queue_element)*(private->link_desc_arr[idx].queues_length+1))+(unsigned char*)private->link_desc_arr[idx].ctrl_channel_base);
			private->link_desc_arr[idx].stub_array[idx_tmp].tx_stub=(struct queue_stub*)((idx_tmp*4+2)*(sizeof(struct queue_stub)+sizeof(struct queue_element)*(private->link_desc_arr[idx].queues_length+1))+(unsigned char*)private->link_desc_arr[idx].ctrl_channel_base);
			private->link_desc_arr[idx].stub_array[idx_tmp].alloc_stub=(struct queue_stub*)((idx_tmp*4+3)*(sizeof(struct queue_stub)+sizeof(struct queue_element)*(private->link_desc_arr[idx].queues_length+1))+(unsigned char*)private->link_desc_arr[idx].ctrl_channel_base);
		}
	}
	/*dump queues quantum*/
	for(idx=0;idx<private->link_desc_number;idx++){
		for(idx_tmp=0;idx_tmp<private->link_desc_arr[idx].queues;idx_tmp++){
			
			printf("link %d queue %d rx %d quantum %d\n",idx,idx_tmp,private->link_desc_arr[idx].stub_array[idx_tmp].rx_stub->queue_channel_state&QUEUE_CHANNEL_ACTIVE,queue_quantum(private->link_desc_arr[idx].stub_array[idx_tmp].rx_stub));
			printf("link %d queue %d free %d quantum %d\n",idx,idx_tmp,private->link_desc_arr[idx].stub_array[idx_tmp].free_stub->queue_channel_state&QUEUE_CHANNEL_ACTIVE,queue_quantum(private->link_desc_arr[idx].stub_array[idx_tmp].free_stub));
			printf("link %d queue %d tx %d quantum %d\n",idx,idx_tmp,private->link_desc_arr[idx].stub_array[idx_tmp].tx_stub->queue_channel_state&QUEUE_CHANNEL_ACTIVE,queue_quantum(private->link_desc_arr[idx].stub_array[idx_tmp].tx_stub));
			printf("link %d queue %d alloc %d quantum %d\n",idx,idx_tmp,private->link_desc_arr[idx].stub_array[idx_tmp].alloc_stub->queue_channel_state&QUEUE_CHANNEL_ACTIVE,queue_quantum(private->link_desc_arr[idx].stub_array[idx_tmp].alloc_stub));
		}
	}
	
	/*3rd reuster MSI-X interrupts into BAR 1*/
	for(msi_x_entries=0,idx=0;idx<private->link_desc_number;idx++){
		for(idx_tmp=0;idx_tmp<private->link_desc_arr[idx].queues;idx_tmp++){
			private->link_desc_arr[idx].stub_array[idx_tmp].msi_x_vector=msi_x_entries+idx_tmp;
			private->link_desc_arr[idx].stub_array[idx_tmp].interrup_enabled=0;
			printf("msi-x vector:%d.%d:%d\n",private->link_desc_arr[idx].link_index,idx_tmp,private->link_desc_arr[idx].stub_array[idx_tmp].msi_x_vector);
		}
		msi_x_entries+=private->link_desc_arr[idx].queues;/*msi-x per queue*/
	}
	printf("msx_x_vectors:%d\n",msi_x_entries);
	
	msix_init_exclusive_bar(pdev,msi_x_entries,1);
	for(idx=0;idx<msi_x_entries;idx++)
		msix_vector_use(pdev,idx);

	
	/*start unix_domain server thread*/
	pthread_create(&pid,NULL,msi_x_server_thread,private);
	pdev->config_read=pci_default_write_config;
	pdev->config_read=pci_default_read_config;
	return 0;
}
void pci_vlinky_dev_exit(PCIDevice *pdev)
{
	
}
void pci_vlinky_dev_reset(PCIDevice *pdev)
{
	
}
void pci_vlinky_class_init(ObjectClass *kclass,void *data)
{
	DeviceClass *dc=DEVICE_CLASS(kclass);
	PCIDeviceClass *pdc=PCI_DEVICE_CLASS(kclass);

	/*printf("[x]vlinky meeeow\n");*/
	pdc->init=pci_vlinky_dev_init;
	pdc->exit=pci_vlinky_dev_exit;
	pdc->vendor_id=0xcccc;
	pdc->device_id=0x2222;
	pdc->revision=0;
	pdc->class_id=PCI_CLASS_OTHERS;
	dc->desc="vlinky frontend device";
	dc->reset=pci_vlinky_dev_reset;
	dc->props=vlinky_property;
	set_bit(DEVICE_CATEGORY_MISC,dc->categories);
	
}
TypeInfo pci_vlinky_info={
	.name=VLINKY_DEVCIE_NAME,
	.parent=TYPE_PCI_DEVICE,
	.instance_size=sizeof(PCIVlinkyPrivate),
	.class_init=pci_vlinky_class_init
};

void pci_vlinky_register_type(void)
{
	type_register_static(&pci_vlinky_info);
}

type_init(pci_vlinky_register_type);



