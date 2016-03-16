#include <linux/pci.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>

#include "queue.h"

#define VENDOR_ID 0xcccc
#define DEVICE_ID 0x2222

struct pci_device_id vlinky_id_tbl[]={
	{VENDOR_ID,DEVICE_ID,PCI_ANY_ID,PCI_ANY_ID,0,0,0},
	{0}

};
#pragma pack(1)
/*this is great ,we onluy calculate the offset of interested fields of an struct*/

struct dpdk_rte_mbuf{
	union {
		unsigned char dummy0[128];
		struct {
			unsigned char dummy1[0];
			void * buf_addr;/*offset is 0*/
		};
		struct{
			unsigned char dummy2[16];
			uint16_t buf_len;
			uint16_t data_off;
		};
		struct {
			unsigned char dummy3[36];
			uint32_t pkt_len;
			uint16_t data_len;
		};
	}
};

struct channel_private{
	int channel_index;
	struct vlinky_adapter *adapter_ptr;/*still need to reference back*/
	int interrupt_requested;/*an indicator*/
	struct napi_struct  napi;/*every channel has one*/
	
	struct queue_stub * rx_stub;
	struct queue_stub * free_stub;
	struct queue_stub * tx_stub;
	struct queue_stub * alloc_stub;
};
struct link_private{
	int link_index;
	int link_channels;
	struct channel_private *channels_array;
	struct vlinky_adapter * adapter_ptr;/*pointer back to vlinky adapter,because we have not other way to reference them when releasing pci device */
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

	unsigned int bar2_reg_addr;
	unsigned int bar2_reg_size;
	void * __iomem bar2_mapped_addr;

	/*bar2 region context*/
};
struct vlinky_adapter{
	struct pci_private *pci_pri;
	struct net_device *netdev;
	
	int link_index;
	
};


irqreturn_t vlinky_pci_interrupt_handler(int irq,void*dev_instance)
{
	struct channel_private *channel=dev_instance;
	struct vlinky_adapter *adapter=channel->adapter_ptr;
	struct pci_private *pci_pri=adapter->pci_pri;
	struct net_device *netdev=adapter->netdev;

	
	/*printk("vlinky irq:%d %d.%d %d %llx\n",irq,adapter->link_index,channel->channel_index
		,napi_schedule_prep(&channel->napi),&channel->napi);*/

	/*before we start to poll ,need to disable interrupt for the given channel*/

	
	napi_schedule(&channel->napi);/*schedule the poll api*/
	
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
		sprintf(g_pci_pri->msix_name[idx],"vlinky-config%d",idx);
	}
	g_pci_pri->pci_msxi_enabled=0;
	
	rc=pci_enable_msix(g_pci_pri->pdev,g_pci_pri->msix_entries,g_pci_pri->nvectors);
	if(rc){
		printk("enabled msix:%d\n",rc);
		return rc;
	}
	g_pci_pri->pci_msxi_enabled=!0;
	
	#if 0 /*currently we postpone to the moment when the related device is open*/
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
	#if 0
	for(idx=0;idx<g_pci_pri->nvectors;idx++){
		free_irq(g_pci_pri->msix_entries[idx].vector,NULL);
	}
	#endif
	
	if(g_pci_pri->pci_msxi_enabled)
		pci_disable_msix(g_pci_pri->pdev);
	if(g_pci_pri->msix_entries)
		kfree(g_pci_pri->msix_entries);
	if(g_pci_pri->msix_name)
		kfree(g_pci_pri->msix_name);
}

int vlinky_netdevice_open(struct net_device *netdev)
{
	int rc;
	int idx=0;
	int nvector_base=0;
	int nvector_count=0;

	/*request interrupt whenever the device is open,also release irq whenever the device is closed */
	struct vlinky_adapter *adapter=netdev_priv(netdev);
	struct pci_private *pci_pri=adapter->pci_pri;
	for(idx=0;idx<adapter->link_index;idx++)
		nvector_base+=pci_pri->link_pri_arr[idx].link_channels;
	nvector_count=pci_pri->link_pri_arr[adapter->link_index].link_channels;
	printk("[x]netdev open:%s(msi-x base:%d count:%d)\n",netdev->name,nvector_base,nvector_count);
	for(idx=nvector_base;idx<nvector_base+nvector_count;idx++){
		sprintf(pci_pri->msix_name[idx],"vlinky-channel%d.%d",adapter->link_index,idx-nvector_base);
	
		rc=request_irq(pci_pri->msix_entries[idx].vector,vlinky_pci_interrupt_handler,0,pci_pri->msix_name[idx],&(pci_pri->link_pri_arr[adapter->link_index].channels_array[idx-nvector_base]));
		/*note that,the interrup handler argument is the channel_private*/
		pci_pri->link_pri_arr[adapter->link_index].channels_array[idx-nvector_base].interrupt_requested=!rc;
	}

	
	for(idx=0;idx<pci_pri->link_pri_arr[adapter->link_index].link_channels;idx++){
		napi_enable(&(pci_pri->link_pri_arr[adapter->link_index].channels_array[idx].napi));
	}
	
	return 0;
}
int vlinky_netdevice_close(struct net_device *netdev)
{
	int rc;
	int idx=0;
	int nvector_base=0;
	int nvector_count=0;
	struct vlinky_adapter *adapter=netdev_priv(netdev);
	struct pci_private *pci_pri=adapter->pci_pri;
	for(idx=0;idx<adapter->link_index;idx++)
		nvector_base+=pci_pri->link_pri_arr[idx].link_channels;
	nvector_count=pci_pri->link_pri_arr[adapter->link_index].link_channels;
	printk("[x]netdev closed:%s(msi-x base:%d count:%d)\n",netdev->name,nvector_base,nvector_count);
	for(idx=nvector_base;idx<nvector_base+nvector_count;idx++){
		if(pci_pri->link_pri_arr[adapter->link_index].channels_array[idx-nvector_base].interrupt_requested){
			free_irq(pci_pri->msix_entries[idx].vector,&(pci_pri->link_pri_arr[adapter->link_index].channels_array[idx-nvector_base]));
		}
	}
	for(idx=0;idx<pci_pri->link_pri_arr[adapter->link_index].link_channels;idx++){
		napi_disable(&(pci_pri->link_pri_arr[adapter->link_index].channels_array[idx].napi));
	}

	return 0;
}
netdev_tx_t vlinky_netdevice_xmit(struct sk_buff*skb,struct net_device *netdev)
{

	kfree_skb(skb);
	
	return NETDEV_TX_OK;
}
u16 vlinky_netdevice_select_queue(struct net_device* netdev,struct sk_buff * skb,void * accel_priv,select_queue_fallback_t fallback)
{
	return 0;
}
struct rtnl_link_stats64 *vlinky_netdevice_stat64(struct net_device *netdev,struct rtnl_link_stats64 *stats)
{
	memset(stats,0x0,sizeof(struct rtnl_link_stats64));
	return stats;
}
struct net_device_ops vlinky_netdev_ops={
	.ndo_open=vlinky_netdevice_open,
	.ndo_stop=vlinky_netdevice_close,
	.ndo_start_xmit=vlinky_netdevice_xmit,
	.ndo_select_queue=vlinky_netdevice_select_queue,
	.ndo_get_stats64=vlinky_netdevice_stat64,
	
};
int vlinky_channel_poll(struct napi_struct *napi,int budget)
{

	printk("cute,,napi :%d\n",budget);
	napi_complete(napi);
	
	return budget-1;
}
int vlinky_pci_probe(struct pci_dev *pdev,const struct pci_device_id *ent)
{
	
	int rc;
	int idx,idx_tmp;
	struct net_device *netdev_tmp;
	struct vlinky_adapter *adapter;
	struct pci_private*private=kmalloc(sizeof(struct pci_private),GFP_KERNEL);
	memset(private,0x0,sizeof(struct pci_private));
	
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

	/*3 request bar3 region resource*/
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
	/*2 request bar2 region resource*/
	private->bar2_reg_addr=pci_resource_start(pdev,2);
	private->bar2_reg_size=pci_resource_len(pdev,2);
	private->bar2_mapped_addr=pci_ioremap_bar(pdev,2);
	printk("[x]bar 2:reg addr:%llx\n",private->bar2_reg_addr);
	printk("[x]bar 2:reg len :%lld\n",private->bar2_reg_size);
	printk("[x]bar 2:map addr:%llx\n",private->bar2_mapped_addr);
	
	/*1.read register and get vectore count and request msi-x interrupt,here we only support MSI-X interrupt mode*/
	
	rc=request_msix_vectors(private,private->nvectors);
	if(rc)
		goto pci_release;
	pci_set_master(pdev);/*I do not know why this is essential ,but without this ,the IRQ message can not be routed correctly ,this issue confused me for several days*/

	
	pci_set_drvdata(pdev,private);

	/*next we will allocate ethernet device for multiple queues*/
	for(idx=0;idx<private->nr_links;idx++){
		
		netdev_tmp=alloc_etherdev_mq(sizeof(struct vlinky_adapter),private->link_pri_arr[idx].link_channels);
		if(!netdev_tmp)
			goto pci_resource_release;
		
		adapter=netdev_priv(netdev_tmp);
		adapter->link_index=private->link_pri_arr[idx].link_index;
		adapter->netdev=netdev_tmp;
		adapter->pci_pri=private;
		adapter->netdev->netdev_ops=&vlinky_netdev_ops;
		private->link_pri_arr[idx].adapter_ptr=adapter;/*link reference back ,this is critical*/
		strcpy(netdev_tmp->name,"vlinky%d");
		for(idx_tmp=0;idx_tmp<private->link_pri_arr[idx].link_channels;idx_tmp++){
			private->link_pri_arr[idx].channels_array[idx_tmp].channel_index=idx_tmp;
			private->link_pri_arr[idx].channels_array[idx_tmp].adapter_ptr=adapter;/*channel reference back ,this is critical*/
			private->link_pri_arr[idx].channels_array[idx_tmp].interrupt_requested=0;
			netif_napi_add(adapter->netdev,&(private->link_pri_arr[idx].channels_array[idx_tmp].napi),vlinky_channel_poll,64);
			//napi_enable(&(private->link_pri_arr[idx].channels_array[idx_tmp].napi));
			
		}
		netif_set_real_num_rx_queues(adapter->netdev,private->link_pri_arr[idx].link_channels);
		netif_set_real_num_tx_queues(adapter->netdev,private->link_pri_arr[idx].link_channels);
		rc=register_netdev(adapter->netdev);
		if(rc){
			
			goto pci_resource_release;
		}
	
		printk("[x] netdev success:%d\n",adapter->link_index);
	}

	
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
	pci_resource_release:
	if(private->link_pri_arr){
		for(idx=0;idx<private->nr_links;idx++)
			kfree(private->link_pri_arr[idx].channels_array);
		kfree(private->link_pri_arr);
		private->link_pri_arr=NULL;
	}
	if(private->bar0_mapped_addr)
		pci_iounmap(private->pdev,private->bar0_mapped_addr);
	if(private->bar3_mapped_addr)
		pci_iounmap(private->pdev,private->bar3_mapped_addr);
	pci_release:
		pci_release_regions(pdev);
	pci_disable:
		pci_disable_device(pdev);
		return rc;
	

	return 0;
}
void vlinky_pci_remove(struct pci_dev *pdev)
{
	int idx;
	struct pci_private * private=pci_get_drvdata(pdev);
	for(idx=0;idx<private->nr_links;idx++){
		unregister_netdev(private->link_pri_arr[idx].adapter_ptr->netdev);
		free_netdev(private->link_pri_arr[idx].adapter_ptr->netdev);
	}
	
	if(private->link_pri_arr){
		for(idx=0;idx<private->nr_links;idx++)
			kfree(private->link_pri_arr[idx].channels_array);
		kfree(private->link_pri_arr);
		private->link_pri_arr=NULL;
	}
	if(private->bar0_mapped_addr)
		pci_iounmap(private->pdev,private->bar0_mapped_addr);
	if(private->bar3_mapped_addr)
		pci_iounmap(private->pdev,private->bar3_mapped_addr);
	release_msix_vectors(private);
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

