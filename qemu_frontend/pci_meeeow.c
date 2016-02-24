
#include "hw/hw.h"
#include "hw/pci/pci.h"
#include "qemu/osdep.h"
#include<arpa/inet.h>
#include<sys/socket.h>

#define NAME "pci_meeeow"
void pci_meeeow_class_init(ObjectClass *klass,void*data);


#define meeeow_dbg(fmt,...) \
	printf("meeeow-pci:" fmt,## __VA_ARGS__)

enum meeeow_reg{
	INT_REG=0,/*non-zero will trigger an interrupt*/
	STATUS_REG=4,
	EXTRA_REG=8
};



typedef struct PCIMeeeowDevState{
	PCIDevice pci_dev;
	PCIDevice *pdev;
	MemoryRegion mr_io;
	char *cute;
	uint32_t int_reg;
	uint32_t status_reg;
	uint32_t extra_reg;
}PCIMeeeowDevState;

PCIDevice*gpdev=NULL;
PCIMeeeowDevState *gpstate;

uint64_t pci_meeeow_mr_io_read(void*opaque,hwaddr addr,unsigned size)
{
	uint32_t ret=0xfffffffe;
	PCIMeeeowDevState*pstate=(struct PCIMeeeowDevState*)opaque;
	switch (addr)
	{
		case INT_REG:
			ret=pstate->int_reg;
			break;
		case STATUS_REG:
			ret=pstate->status_reg;
			break;
		case EXTRA_REG:
			ret=pstate->extra_reg;
			break;
		default:
			break;
	}
	return ret;
}
void pci_meeeow_mr_io_write(void * opaque,hwaddr addr,uint64_t val,unsigned size)
{
	PCIMeeeowDevState*pstate=(struct PCIMeeeowDevState*)opaque;
	switch (addr)
	{
		case INT_REG:
			pstate->int_reg=val?1:0;
			pci_set_irq(pstate->pdev,!!pstate->int_reg);
			
			break;
		case STATUS_REG:
			meeeow_dbg("status reg:%08x",pstate->status_reg);
			
			pstate->status_reg=val;
			
			break;
		case EXTRA_REG:
			pstate->extra_reg=val;
			break;
		default:
			break;
	}
}
struct MemoryRegionOps pci_meeeow_mr_io_ops={
	.read=pci_meeeow_mr_io_read,
	.write=pci_meeeow_mr_io_write,
	.endianness=DEVICE_NATIVE_ENDIAN,
	.impl={
		.min_access_size=4,
		.max_access_size=4
	}
};
pthread_t pid;
void * msg_worker(void*arg)
{	int fd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	int rc;
	struct sockaddr_in caddr;
	int clen;
	int cfd;
	struct sockaddr_in saddr;

	saddr.sin_family=AF_INET;
	saddr.sin_port=htons(5070);
	saddr.sin_addr.s_addr=0;
	assert(fd>0);
	rc=bind(fd,(struct sockaddr*)&saddr,sizeof(struct sockaddr_in));
	assert(rc==0);
	listen(fd,1);
	
	while(1){
		clen=sizeof(struct sockaddr_in);
		cfd=accept(fd,(struct sockaddr*)&caddr,&clen);
		
		printf("trigger an interrupt\n");


		gpstate->int_reg=!0;
		pci_irq_assert(gpdev);

	}
	return NULL;
}
pthread_t  create_msg_thread()
{
	pthread_t fd=0;
	pthread_create(&fd,NULL,msg_worker,NULL);
	return fd;
}

int pci_meeeow_dev_init(PCIDevice * pdev)
{

	
	int rc;
	int idx;
	uint8_t  * conf=pdev->config;
	
	PCIMeeeowDevState*pstate=OBJECT_CHECK(PCIMeeeowDevState,pdev,NAME);
	gpdev=pdev;
	gpstate=pstate;
	
	pstate->pdev=pdev;
	printf("pci-dev-address:%s\n",pstate->cute);
	
	//pdev->config[PCI_INTERRUPT_PIN]=0;
	//pci_config_set_interrupt_pin(conf,1);
	pdev->config[PCI_INTERRUPT_PIN]=0x02;
	conf[PCI_COMMAND]=PCI_COMMAND_IO|PCI_COMMAND_MEMORY;
	
	memory_region_init_io(&pstate->mr_io,OBJECT(pstate),&pci_meeeow_mr_io_ops,pstate,"pci_meeeow_mr_io",0x100);
	
	pci_register_bar(pdev,0,PCI_BASE_ADDRESS_SPACE_MEMORY,&pstate->mr_io);
	pdev->config_write=pci_default_write_config;


	//pdev->cap_present|=QEMU_PCI_CAP_MSIX;
	rc=msix_init_exclusive_bar(pdev,4,1);

	printf("init msix:%d\n",msix_present(pdev));
	for(idx=0;idx<4;idx++){
		msix_vector_use(pdev,idx);
	}
	//pid=create_msg_thread();
	return 0;
	

}
void pci_meeeow_dev_exit(PCIDevice*pdev)
{
	meeeow_dbg("meeeow exit");
	
}
void dev_reset(DeviceState*dev){

	meeeow_dbg("dev reset");

}
static Property meeeow_property[]={
DEFINE_PROP_STRING("meeeow",PCIMeeeowDevState,cute),
DEFINE_PROP_END_OF_LIST(),
};

void pci_meeeow_class_init(ObjectClass *klass,void*data)
{
	DeviceClass*dc=DEVICE_CLASS(klass);
	PCIDeviceClass *pdc=PCI_DEVICE_CLASS(klass);
	printf("meeeow class init\n");
	pdc->init=pci_meeeow_dev_init;
	pdc->exit=pci_meeeow_dev_exit;
	pdc->vendor_id=0xcccc;
	pdc->device_id=0x2222;
	pdc->revision=0;
	dc->props=meeeow_property;
	pdc->class_id=PCI_CLASS_OTHERS;
	dc->desc="pci demo of meeeow";
	set_bit(DEVICE_CATEGORY_MISC,dc->categories);
	dc->reset=dev_reset;
	
}


TypeInfo pci_meeeow_info={
	.name=NAME,
	.parent=TYPE_PCI_DEVICE,
	.instance_size=sizeof(PCIMeeeowDevState),
	.class_init=pci_meeeow_class_init
};
void pci_meeeow_register_types(void)
{
	type_register_static(&pci_meeeow_info);
	
}
type_init(pci_meeeow_register_types);

