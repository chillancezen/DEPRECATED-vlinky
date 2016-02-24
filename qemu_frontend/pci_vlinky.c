#include "hw/hw.h"
#include "hw/pci/pci.h"
#include "qemu/osdep.h"
#include "./queue.h" //absolute path,since we have several  queue.h in qemu projection
#include "./shm_util.h"
#include "./vswitch_vlink.h"


#define VLINKY_DEVCIE_NAME "pci_vlinky"
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
};
typedef struct PCIVlinkyPrivate{
	PCIDevice pci_dev;
	char * ctrl_channel_file;/*it should be any shm file,and it should be initilized by any other DPDK process*/
	int nr_queues;/*number of queues per device,*/
	int queue_length;/*length of queue ,not including stub space*/

	//struct queue_stub* *stub_array;
	struct queue_channel* stub_array;
	void * dpdk_huge_base;
	int dpdk_huge_size;
	MemoryRegion dpdk_memory_bar;/*will be registered into BAR2*/
	MemoryRegion control_bar;/*will be registered into BAR0*/
	int dummy[100];
}PCIVlinkyPrivate;
static Property vlinky_property[]={
	DEFINE_PROP_STRING("ctrl_channel",PCIVlinkyPrivate,ctrl_channel_file),
	DEFINE_PROP_INT32("queues",PCIVlinkyPrivate,nr_queues,0),
	DEFINE_PROP_INT32("queue_length",PCIVlinkyPrivate,queue_length,0),
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
int pci_vlinky_dev_init(PCIDevice *pdev)
{
	int rc;
	PCIVlinkyPrivate *private=OBJECT_CHECK(PCIVlinkyPrivate,pdev,VLINKY_DEVCIE_NAME);
	pdev->config[PCI_INTERRUPT_PIN]=0x2;
	pdev->config[PCI_COMMAND]=PCI_COMMAND_IO|PCI_COMMAND_MEMORY;

	printf("[x]ctrl shm file:%s\n",private->ctrl_channel_file);
	printf("[x]ctrl queues  :%d\n",private->nr_queues);
	printf("[x]ctrl queue len:%d\n",private->queue_length);
	/*1st, we got dpdk hugepae mapping address,and makeup BAR 2*/
	rc=attach_to_dpdk_hugepage_memory(&private->dpdk_huge_base,&private->dpdk_huge_size);
	printf("[x] rx:%d dpdk huge base:%llx (%d)\n",rc,private->dpdk_huge_base,private->dpdk_huge_size);
	if(rc){
		printf("[x]something maybe wrong with DPDK memory mapping\n");
		return -1;
	}
	memory_region_init(&private->dpdk_memory_bar,OBJECT(private),"dpdk_memory_bar",private->dpdk_huge_size);
	pci_register_bar(pdev,2,PCI_BASE_ADDRESS_SPACE_MEMORY|PCI_BASE_ADDRESS_MEM_PREFETCH,&private->dpdk_memory_bar);
	/*2nd,register BAR 0 for IO ports*/
	memory_region_init_io(&private->control_bar,OBJECT(private),&pci_vlinky_io_ops,private,"contron_bar",0x100);/*suppose 0x100 is large enough*/
	pci_register_bar(pdev,0,PCI_BASE_ADDRESS_SPACE_IO,&private->control_bar);
	
	/*3rd reuster MSI-X interrupts into BAR 1*/
	msix_init_exclusive_bar(pdev,4,1);

	/*4th,register queue channel into BAR 3,first we should make sure this shm area is initialized first by DPDK pmd driver*/
	//private->stub_array=(struct queue_stub **)g_malloc0(private->nr_queues*4*sizeof(struct queue_stub *));
	
	printf("%d %d\n",sizeof(struct queue_element),sizeof(struct queue_stub));
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

	printf("[x]vlinky meeeow\n");
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



