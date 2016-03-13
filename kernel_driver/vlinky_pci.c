#include <linux/pci.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include "queue.h"

#define VENDOR_ID 0xcccc
#define DEVICE_ID 0x2222

struct pci_device_id vlinky_id_tbl[]={
	{VENDOR_ID,DEVICE_ID,PCI_ANY_ID,PCI_ANY_ID,0,0,0},
	{0}

};
struct channel_private{
	struct queue_stub * rx_stub;
	struct queue_stub * free_stub;
	struct queue_stub * tx_stub;
	struct queue_stub * alloc_stub;
};
struct link_private{
	int link_index;
	int link_channels;
	struct channel_private *channels_array;
};
enum vlink_bar0_register{/*io ports registers which reside in BAR 0*/
	 VLINKY_DEVICE_STATUS=0,
	 VLINKY_LINKS_COUNT=4,/*how many links do we have*/
	 VLINKY_VECTORS=8,/*how many msi-x vectors */
	 VLINKY_LINK_INDEX=12,/*this is the link index register*/
	 VLINKY_LINK_CHANNEL_COUNT=16,/*how many channels one link have*/
	 VLINKY_CHANNEL_INDEX=20,/*this is the link channel register*/
	 VLINKY_CHANNEL_RX_OFFSET=24,
	 VLINKY_CHANNEL_FREE_OFFSET=28,
	 VLINKY_CHANNEL_TX_OFFSET=32,
	 VLINKY_CHANNEL_ALLOC_OFFSET=36,
	 
	 #if 0
	 VLINKY_QUEUE_LENGTH=12,
	 
	 VLINKY_LINK_INDEX=16,
	 VLINKY_LINK_INSTRUCTION=20,
	 VLINKY_LINK_DATA=24
	 #endif
};

struct pci_private{
	struct pci_dev * pdev;
	struct msix_entry *msix_entries;
	char (*msix_name)[256];
	int nvectors;/*the total msix vectors numbers*/
	int nr_links;/*total links number,each link corresponds to one ethernet device*/
	
	int pci_msxi_enabled;

	/*bar0 region context*/
	unsigned int reg_addr;
	unsigned int reg_size;
	void * __iomem bar0_mapped_addr;
	struct link_private * link_pri_arr;


	/*bar3 region context*/
	unsigned int bar3_reg_addr;
	unsigned int bar3_reg_size;
	void * __iomem bar3_mapped_addr;
	
};

struct pci_private * global_pci_private=NULL;
irqreturn_t vlinky_pci_interrupt_handler(int irq,void*dev_instance)
{
	printk("vlinky irq:%d\n",irq);
	
	return IRQ_HANDLED;
}
int request_msix_vectors(struct pci_private *g_pci_pri,int nvectors)
{
	int idx;
	int rc;
	
	g_pci_pri->nvectors=nvectors;
	g_pci_pri->msix_entries=kmalloc(g_pci_pri->nvectors*sizeof(struct msix_entry),GFP_KERNEL);
	g_pci_pri->msix_name=kmalloc(g_pci_pri->nvectors*sizeof(*g_pci_pri->msix_name),GFP_KERNEL);
	
	//g_pci_pri->msix_name_arr=kmalloc(g_pci_pri->nvectors*sizeof(char*),GFP_KERNEL);
	for(idx=0;idx<nvectors;idx++){
		g_pci_pri->msix_entries[idx].entry=idx;	
		sprintf(g_pci_pri->msix_name[idx],"vlinky-config",idx);
	}
	g_pci_pri->pci_msxi_enabled=0;
	
	rc=pci_enable_msix(g_pci_pri->pdev,g_pci_pri->msix_entries,g_pci_pri->nvectors);
	if(rc){
		printk("enabled msix:%d\n",rc);
		return rc;
	}
	g_pci_pri->pci_msxi_enabled=!0;
	
	#if 1
	for(idx=0;idx<g_pci_pri->nvectors;idx++){
		
		rc=request_irq(g_pci_pri->msix_entries[idx].vector,vlinky_pci_interrupt_handler,0/*even not shared*/,g_pci_pri->msix_name[idx],NULL);
		printk("vlinky_pci msix number:%d %d %s\n",g_pci_pri->msix_entries[idx].vector,rc,g_pci_pri->msix_name[idx]);
	}
	
	#endif
	return 0;
}
void release_msix_vectors(struct pci_private *g_pci_pri)
{
	int idx;
	for(idx=0;idx<g_pci_pri->nvectors;idx++){
		free_irq(g_pci_pri->msix_entries[idx].vector,NULL);
	}
	
	if(g_pci_pri->pci_msxi_enabled)
		pci_disable_msix(g_pci_pri->pdev);
	if(g_pci_pri->msix_entries)
		kfree(g_pci_pri->msix_entries);
	if(g_pci_pri->msix_name)
		kfree(g_pci_pri->msix_name);
}
int vlinky_pci_probe(struct pci_dev *pdev,const struct pci_device_id *ent)
{
	
	int rc;
	int idx,idx_tmp;
	struct pci_private*private=kmalloc(sizeof(struct pci_private),GFP_KERNEL);
	memset(private,0x0,sizeof(struct pci_private));
	global_pci_private=private;
	private->pdev=pdev;

	
	rc=pci_enable_device(pdev);
	if(rc){
		printk("can not enable pci device\n");
		return rc;
	}
	rc=pci_request_regions(pdev,"vlinky_pci");
	if(rc<0){
		printk("can not request pci region\n");
		goto pci_disable;
		
	}
	/*0.start PCI resource*/

	private->reg_addr=pci_resource_start(pdev,0);
	private->reg_size=pci_resource_len(pdev,0);
	private->bar0_mapped_addr=pci_ioremap_bar(pdev,0);
	
	printk("[x]bar 0:reg addr:%llx\n",private->reg_addr);
	printk("[x]bar 0:reg len :%lld\n",private->reg_size);
	printk("[x]bar 0:map addr:%llx\n",private->bar0_mapped_addr);
	private->nvectors=readl(private->bar0_mapped_addr+VLINKY_VECTORS);
	private->nr_links=readl(private->bar0_mapped_addr+VLINKY_LINKS_COUNT);
	
	printk("[x]bar 0:nvectors:%lld\n",private->nvectors);
	printk("[x]bar 0:nr-links:%lld\n",private->nr_links);

	if(private->nr_links)
		private->link_pri_arr=kmalloc(private->nr_links*sizeof(struct link_private),GFP_KERNEL);
	
	if(!private->link_pri_arr)
		goto pci_release;
	
	for(idx=0;idx<private->nr_links;idx++){
		private->link_pri_arr[idx].link_index=idx;
		writel(idx,private->bar0_mapped_addr+VLINKY_LINK_INDEX);
		mb();
		private->link_pri_arr[idx].link_channels=(int)readl(private->bar0_mapped_addr+VLINKY_LINK_CHANNEL_COUNT);
		private->link_pri_arr[idx].channels_array=kmalloc(private->link_pri_arr[idx].link_channels*sizeof(struct channel_private),GFP_KERNEL);
		printk("[x]link %d has %d channels\n",idx,private->link_pri_arr[idx].link_channels);
	}

	/*3 initialize bar3 region*/
	private->bar3_reg_addr=pci_resource_start(pdev,3);
	private->bar3_reg_size=pci_resource_len(pdev,3);
	private->bar3_mapped_addr=pci_ioremap_bar(pdev,3);
	printk("[x]bar 3:reg addr:%llx\n",private->bar3_reg_addr);
	printk("[x]bar 3:reg len :%lld\n",private->bar3_reg_size);
	printk("[x]bar 3:map addr:%llx\n",private->bar3_mapped_addr);
	/*again ,we need control channel base for queue stubs*/

	
	for(idx=0;idx<private->nr_links;idx++){
		
		writel(idx,private->bar0_mapped_addr+VLINKY_LINK_INDEX);
		wmb();
		for(idx_tmp=0;idx_tmp<private->link_pri_arr[idx].link_channels;idx_tmp++){
			writel(idx_tmp,private->bar0_mapped_addr+VLINKY_CHANNEL_INDEX);
			wmb();
			
			private->link_pri_arr[idx].channels_array[idx_tmp].rx_stub=(struct queue_stub*)((char*)private->bar3_mapped_addr+readl(private->bar0_mapped_addr+VLINKY_CHANNEL_RX_OFFSET));
			private->link_pri_arr[idx].channels_array[idx_tmp].free_stub=(struct queue_stub*)((char*)private->bar3_mapped_addr+readl(private->bar0_mapped_addr+VLINKY_CHANNEL_FREE_OFFSET));
			private->link_pri_arr[idx].channels_array[idx_tmp].tx_stub=(struct queue_stub*)((char*)private->bar3_mapped_addr+readl(private->bar0_mapped_addr+VLINKY_CHANNEL_TX_OFFSET));
			private->link_pri_arr[idx].channels_array[idx_tmp].alloc_stub=(struct queue_stub*)((char*)private->bar3_mapped_addr+readl(private->bar0_mapped_addr+VLINKY_CHANNEL_ALLOC_OFFSET));

			printk("[x] :%d.%d.%d.%d\n",queue_quantum(private->link_pri_arr[idx].channels_array[idx_tmp].rx_stub)
				,queue_quantum(private->link_pri_arr[idx].channels_array[idx_tmp].free_stub)
				,queue_quantum(private->link_pri_arr[idx].channels_array[idx_tmp].tx_stub)
				,queue_quantum(private->link_pri_arr[idx].channels_array[idx_tmp].alloc_stub));
		}
	}
/*
	writel(1,private->bar0_mapped_addr+VLINKY_LINK_INDEX);
	writel(1,private->bar0_mapped_addr+VLINKY_CHANNEL_INDEX);
	
	printk("[x] meeeow:%d\n",readl(private->bar0_mapped_addr+VLINKY_CHANNEL_RX_OFFSET));

	
	printk("[x] meeeow:%d\n",readl(private->bar0_mapped_addr+VLINKY_CHANNEL_FREE_OFFSET));

	
	printk("[x] meeeow:%d\n",readl(private->bar0_mapped_addr+VLINKY_CHANNEL_TX_OFFSET));

	
	printk("[x] meeeow:%d\n",readl(private->bar0_mapped_addr+VLINKY_CHANNEL_ALLOC_OFFSET));

	

	printk("[x] size :%d %d\n",sizeof(struct queue_element),sizeof(struct queue_stub));

	*/
	
	/*1.read register and get vectore count and request msi-x interrupt,here we only support MSI-X interrupt mode*/
	
	rc=request_msix_vectors(private,private->nvectors);
	if(rc)
		goto pci_release;
	pci_set_master(pdev);/*I do not know why this is essential ,but without this ,the IRQ message can not be routed correctly ,this issue confused me for several days*/

	
	
	#if 0
	bar2_ioaddr=pci_resource_start(pdev,2);
	bar2_iosize=pci_resource_len(pdev,2);
	bar2_flag=pci_resource_flags(pdev,2);
	bar2_baseaddr=pci_iomap(pdev,2,bar2_iosize);
	printk("bar2:%x(%d) %x\n",bar2_ioaddr,bar2_iosize,bar2_flag);
	printk("bar2_addr:%x\n",bar2_baseaddr);
	//printk("[x]%s\n",bar2_baseaddr);
	printk("[x]%s\n",176857664+(unsigned char*)bar2_baseaddr);
	
	
	bar2_ioaddr=pci_resource_start(pdev,2);
	bar2_iosize=pci_resource_len(pdev,2);
	bar2_flag=pci_resource_flags(pdev,2);
	bar2_baseaddr=pci_iomap(pdev,2,bar2_iosize);
	
	bar0_ioaddr=pci_resource_start(pdev,0);
	bar0_iosize=pci_resource_len(pdev,0);
	bar0_flag=pci_resource_flags(pdev,0);
	bar0_baseaddr=pci_iomap(pdev,0,bar0_iosize);
	
	
	//printk("read:%x\n",readl(bar0_baseaddr+4));
	//writel(0xffffffff,bar0_baseaddr);
	
	//printk("bar2:%x(%d) %x\n",bar2_ioaddr,bar2_iosize,bar2_flag);
	printk("bar0:%x(%d) %x\n",bar0_ioaddr,bar0_iosize,bar0_flag);
	//printk("bar2_addr:%x\n",bar2_baseaddr);
	printk("bar0_addr:%x\n",bar0_baseaddr);
	//memcpy(bar2_baseaddr,"hello ivshmem!",14);

	/*register irq through INTx*/
	
	rc=request_irq(pdev->irq,ivshmem_irq_handler,IRQF_SHARED,"ivshmem_pci",DRIVER_NAME);
	printk("irq_number:%d.%d\n",pdev->irq,rc);
	//writel(0x12345678,bar0_baseaddr);
	#endif 
	
	return 0;
	pci_release:
		pci_release_regions(pdev);
	pci_disable:
		pci_disable_device(pdev);
		return rc;
	

	return 0;
}
void vlinky_pci_remove(struct pci_dev *pdev)
{
	if(global_pci_private->link_pri_arr){
		kfree(global_pci_private->link_pri_arr);
		global_pci_private->link_pri_arr=NULL;
	}
	if(global_pci_private->bar0_mapped_addr)
		pci_iounmap(global_pci_private->pdev,global_pci_private->bar0_mapped_addr);
	if(global_pci_private->bar3_mapped_addr)
		pci_iounmap(global_pci_private->pdev,global_pci_private->bar3_mapped_addr);
	release_msix_vectors(global_pci_private);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
	
	
}
struct pci_driver vlinky_pci_driver={
	.name="vlinky-net",
	.id_table=vlinky_id_tbl,
	.probe=vlinky_pci_probe,
	.remove=vlinky_pci_remove
	
};
int vlinky_pci_init(void)
{
	int rc=pci_register_driver(&vlinky_pci_driver);
	printk("vlinkty_pci init\n");
	return 0;
}
void vlinky_pci_exit(void)
{
	printk("vlinkty_pci exit\n");
	pci_unregister_driver(&vlinky_pci_driver);
	
}

module_init(vlinky_pci_init);
module_exit(vlinky_pci_exit);

MODULE_AUTHOR("jzheng from bjtu");
MODULE_LICENSE("GPL");

