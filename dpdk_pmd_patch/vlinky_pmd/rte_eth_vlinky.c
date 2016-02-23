/*
*this is the host PMD driver based on DPDK
*,and as part of VLINKY demo projects
*author:Linky Zheng(jzheng.bjtu@hotmail.com)
*Jan.15.2016
*/
#include <time.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_kvargs.h>
#include <rte_dev.h>

#include "shm_util.h"
#include "queue.h"
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define MAX_RX_BURST_LEN 32 /*32 packets is the default rx length */
#define MAX_TX_BURST_LEN 32 /*the same burst size as RX_BURST size*/
#define ETH_VLINKY_ARG_NUM_QUEUES "queues"
#define ETH_VLINKY_ARG_SHM_FILE "shm"

long long virt2offset(void* vaddr);
void *offset2virt(long long offset);
struct pmd_vlinky_channel{
	struct rte_mempool * pool;
	int port_id;
	int channel_id;/*corresponding to queue_id of RX/TX callback function of DPDK API*/
	struct queue_stub * rx_queue_stub;/*for pmd,it's producer*/
	struct queue_stub * free_queue_stub;/*for pmd,it's consumer*/
	struct queue_stub * tx_queue_stub;/*for pmd,it's consumer*/
	struct queue_stub * alloc_queue_stub;/*for pmd,it;s produer*/
	
};
struct pmd_vlinky_private{
	int nr_queues;/*this is the channel numbers of a vlink,I SHOULD call them n_channels*/
	int nr_total_queues;/*total numbers of a vlink,including tx/free/rx/alloc tuple queues*/
	struct ether_addr mac_address;
	void * shm_base;
	int shm_size;
	struct queue_stub** queue_stub_array;
	struct pmd_vlinky_channel * channles;/*this is the really DPDK pmd queues*/

};
static const char* valid_arguments[]=
{
	ETH_VLINKY_ARG_NUM_QUEUES,
	ETH_VLINKY_ARG_SHM_FILE,
	NULL
};
static struct rte_eth_link vlink_pmd_link={
	.link_speed=ETH_LINK_SPEED_10G,/*OMG, I set it to be 10Gpbs link */
	.link_duplex=ETH_LINK_FULL_DUPLEX,
	.link_status=0
};
static const char* drivername="vlinky driver";
#define DEFAULT_QUEUE_LENGTH (10240+1)
static unsigned char mac_quantum=0x1;/*note in most cases ,there will be no more than 255 VMs in a host */


#if 0
int attach_to_dpdk_hugepage_memory(void **base,int *total_size);


/*get the dpdk memory base,once mmapping*/
void * get_dpdk_memory_layout(int *_size)
{
	static void * base=NULL;
	static int size=0;
	int rc=0;
	if(base)
		goto ret_flag;
	rc=attach_to_dpdk_hugepage_memory(&base,&size);
	if(rc){
		base=NULL;
		size=0;
	}
	ret_flag:
		*_size=size;
		return base;
}
#endif

static int argument_callback_queues(const char * key,const char * value,void *extra)
{
	int nr_queues=0;
	nr_queues=atoi(value);
	if(!nr_queues)
		return -1;
	*((int*)extra)=nr_queues;
	return 0;
}
static int argument_callback_shm(const char * key,const char * value,void * extra)
{
	sprintf(extra,"%s",value);
	return 0;
}
static int vlinky_pmd_dev_start(struct rte_eth_dev *dev)
{
	printf("[x]dev start:%s\n",dev->data->name);
	dev->data->dev_link.link_status=1;
	return 0;
}
static void vlinky_pmd_dev_stop(struct rte_eth_dev * dev)
{
	printf("[x]dev stop:%s\n",dev->data->name);
}
static void vlinky_pmd_dev_close(struct rte_eth_dev*dev)
{

}
static int vlinky_pmd_dev_configure(struct rte_eth_dev *dev)
{

	return 0;
}

static void vlinky_pmd_dev_info_get(struct rte_eth_dev*dev,struct rte_eth_dev_info *dev_info)
{
	struct pmd_vlinky_private *pmd_pri=dev->data->dev_private;
	dev_info->driver_name=drivername;
	dev_info->if_index=dev->data->port_id;
	dev_info->max_mac_addrs=1;
	dev_info->max_rx_pktlen=(uint32_t) -1;
	dev_info->max_rx_queues=pmd_pri->nr_queues;
	dev_info->max_tx_queues=pmd_pri->nr_queues;
	dev_info->pci_dev=NULL;
	
}
static int vlinky_pmd_dev_rx_queue_setup(
	struct rte_eth_dev * dev,
	uint16_t rx_queue_id,
	uint16_t nb_rx_desc,
	unsigned int socket_id,
	const struct rte_eth_rxconf*rx_conf,
	struct rte_mempool*mb_pool)
{

	struct pmd_vlinky_private * pmd_pri=dev->data->dev_private;
	dev->data->rx_queues[rx_queue_id]=&pmd_pri->channles[rx_queue_id];/*this channel ,not the real queue. will be delivered to rx PMD callback*/
	pmd_pri->channles[rx_queue_id].port_id=dev->data->port_id;
	pmd_pri->channles[rx_queue_id].pool=mb_pool;
	pmd_pri->channles[rx_queue_id].rx_queue_stub->queue_stub_state|=QUEUE_STUB_ACTIVE;
	pmd_pri->channles[rx_queue_id].free_queue_stub->queue_stub_state|=QUEUE_STUB_ACTIVE;
	pmd_pri->channles[rx_queue_id].alloc_queue_stub->queue_stub_state|=QUEUE_STUB_ACTIVE;
	printf("[x] rx queue setup:%d\n",rx_queue_id);
	return 0;
}
static int vlinky_pmd_dev_tx_queue_setup(
	struct rte_eth_dev *dev,
	uint16_t tx_queue_id,
	uint16_t nb_tx_desc,
	unsigned int socket_id,
	const struct rte_eth_txconf *tx_conf)
{
	struct pmd_vlinky_private *pmd_pri=dev->data->dev_private;
	dev->data->tx_queues[tx_queue_id]=&pmd_pri->channles[tx_queue_id];
	pmd_pri->channles[tx_queue_id].tx_queue_stub->queue_stub_state|=QUEUE_STUB_ACTIVE;
	printf("[x] tx queue setup:%d\n",tx_queue_id);
	return 0;
}
static int vlinky_pmd_dev_link_update(struct rte_eth_dev *dev __rte_unused,
	int wait_to_complete __rte_unused)
{	
	return 0;
}
static const struct eth_dev_ops pmd_dev_ops={
	.dev_start=vlinky_pmd_dev_start,
	.dev_stop=vlinky_pmd_dev_stop,
	.dev_close=vlinky_pmd_dev_close,
	.dev_configure=vlinky_pmd_dev_configure,
	.dev_infos_get=vlinky_pmd_dev_info_get,
	.rx_queue_setup=vlinky_pmd_dev_rx_queue_setup,
	.tx_queue_setup=vlinky_pmd_dev_tx_queue_setup,
	.link_update=vlinky_pmd_dev_link_update
	
};
static int last=0;
static uint16_t vlinky_dev_rx(void *queue,struct rte_mbuf**bufs,uint16_t nr_pkts)
{
	struct rte_mbuf *mbuf_tmp;
	int rx_packets=0,free_packets;
	int idx;
	int data_len;
	int allocate_quantum=0;
	int alloc_rc=0;
	struct pmd_vlinky_channel *channel=queue;
	struct queue_element rx_packets_buff[MAX_RX_BURST_LEN];
	struct queue_element * rx_packets_buff_ptr[MAX_RX_BURST_LEN];
	
	
	//printf("rx:%d,%d\n",channel->port_id,channel->channel_id);
	/*1.dequeue as any packets from tx_queues of channel,and maybe alter some field ,and deliever them to DPDK  */
	for(idx=0;idx<MAX_RX_BURST_LEN;idx++)
		rx_packets_buff_ptr[idx]=&rx_packets_buff[idx];
	
	rx_packets=dequeue_bulk(channel->tx_queue_stub,rx_packets_buff_ptr,MIN(nr_pkts,MAX_RX_BURST_LEN));
	for(idx=0;idx<rx_packets;idx++){
		bufs[idx]=offset2virt(rx_packets_buff_ptr[idx]->rte_pkt_offset);
		bufs[idx]->buf_addr=offset2virt(rx_packets_buff_ptr[idx]->rte_data_offset);
		bufs[idx]->port=channel->port_id;
	}
	
	/*2. try to free packets from free_queue,here we don't really free them by calling rte_pkt_free,instead ,we  put them into alloc_queue*/
	/*for now ,rx_packets_buff_ptr will be used no more by previous RX action,so here I use them once more*/
	free_packets=dequeue_bulk(channel->free_queue_stub,rx_packets_buff_ptr,MAX_RX_BURST_LEN);
	allocate_quantum=queue_quantum(channel->alloc_queue_stub);
	allocate_quantum=MIN(allocate_quantum,free_packets);
	alloc_rc=0;
	if(allocate_quantum){
		alloc_rc=enqueue_bulk(channel->alloc_queue_stub,rx_packets_buff_ptr,allocate_quantum);
		/*in fact ,alloc_rc must be equal to allocate_quantum,because we check it before calling */
	}
	if(free_packets-alloc_rc){
		//printf("[x]free packets:%d\n",free_packets-alloc_rc);
	}
	for(idx=alloc_rc;idx<free_packets;idx++){
		rte_pktmbuf_free(offset2virt(rx_packets_buff_ptr[idx]->rte_pkt_offset));
	}
	
	/*3.allocate as many packets as possible,and put them into the alloc queue*/
	allocate_quantum=queue_quantum(channel->alloc_queue_stub);
	allocate_quantum=MIN(allocate_quantum,MAX_RX_BURST_LEN);
	for(idx=0;idx<allocate_quantum;idx++){
		mbuf_tmp=rte_pktmbuf_alloc(channel->pool);
		if(!mbuf_tmp)
			break;
		rx_packets_buff_ptr[idx]->rte_pkt_offset=virt2offset(mbuf_tmp);
		rx_packets_buff_ptr[idx]->rte_data_offset=virt2offset(mbuf_tmp->buf_addr);
	}
	allocate_quantum=idx;/*update allocate_quantum in case of allocation failure*/
	alloc_rc=0;
	if(allocate_quantum){
		alloc_rc=enqueue_bulk(channel->alloc_queue_stub,rx_packets_buff_ptr,allocate_quantum);
	}
	for(idx=alloc_rc;idx<allocate_quantum;idx++){
		rte_pktmbuf_free(offset2virt(rx_packets_buff_ptr[idx]->rte_pkt_offset));
	}
	
	if(last!=queue_quantum(channel->alloc_queue_stub)){
		printf("[x]:%lld\n",queue_quantum(channel->alloc_queue_stub));
		last=queue_quantum(channel->alloc_queue_stub);
	}
	return rx_packets;
}
static uint16_t vlinky_dev_tx(void *queue,struct rte_mbuf**bufs,uint16_t nr_pkts)
{
	#if 0
	struct queue_element qe;
	struct queue_element* qe_arr[1];
	struct pmd_vlinky_channel *channel=queue;
	int idx=0;
	int rc;
	qe_arr[0]=&qe;
	
	for(idx=0;idx<nr_pkts;idx++){
		rc=enqueue_bulk(channel->rx_queue_stub,qe_arr,1);
		//printf("offset:%llx\n",virt2offset(bufs[idx]));
		rte_pktmbuf_free(bufs[idx]);
	}
	//printf("tx mbuf:%d\n",nr_pkts);
	#endif
	int idx;
	int quantum;
	int tx_quantum;
	struct pmd_vlinky_channel *channel=queue;
	struct queue_element tx_packets_buff[MAX_TX_BURST_LEN];
	struct queue_element * tx_packets_buff_ptr[MAX_TX_BURST_LEN];
	/*1.initialize local buffer */
	for(idx=0;idx<MAX_TX_BURST_LEN;idx++){
		tx_packets_buff_ptr[idx]=&tx_packets_buff[idx];
	}
	quantum=MIN(MAX_TX_BURST_LEN,nr_pkts);
	for(idx=0;idx<quantum;idx++){
		tx_packets_buff_ptr[idx]->rte_pkt_offset=virt2offset(bufs[idx]);
		tx_packets_buff_ptr[idx]->rte_data_offset=virt2offset(bufs[idx]->buf_addr);
		//printf("[x]%d tx :%llx %llx\n",idx,bufs[idx],bufs[idx]->buf_addr);
	}
	tx_quantum=enqueue_bulk(channel->rx_queue_stub,tx_packets_buff_ptr,quantum);
	
	return tx_quantum;
}
static int calculate_shm_size(int nr_queues,int queue_length)
{
	int size_ret=0;
	size_ret=nr_queues*(sizeof(struct queue_stub)+queue_length*sizeof(struct queue_element));
	return size_ret;
}
static int rte_pmd_vlinky_devinit(const char*name,const char *params)
{
	struct rte_kvargs *kvlist;
	struct rte_eth_dev_data *data;
	struct rte_eth_dev *pmd_dev;
	struct pmd_vlinky_private * pmd_pri;
	
	int idx;
	int ret=0;
	int  rc;
	int tmp_length;
	kvlist=rte_kvargs_parse(params,valid_arguments);
	if(kvlist==NULL)
		return -1;
	int nr_qeueues;
	char shm_file[256];
	rc=rte_kvargs_process(kvlist,ETH_VLINKY_ARG_NUM_QUEUES,argument_callback_queues,&nr_qeueues);
	if(rc<0)
		goto list_free;
	rc=rte_kvargs_process(kvlist,ETH_VLINKY_ARG_SHM_FILE,argument_callback_shm,shm_file);
	if(rc<0)
		goto list_free;
	/*1.steup local pmd eth device*/

	pmd_dev=rte_eth_dev_allocate(name,RTE_ETH_DEV_VIRTUAL);
	if(!pmd_dev){
		ret=-1;
		goto list_free;
	}
	data=rte_zmalloc(name,sizeof(struct rte_eth_dev_data),0);
	if(!data)
		goto dev_free;
	pmd_pri=rte_zmalloc(NULL,sizeof(struct pmd_vlinky_private),0);
	if(!pmd_pri)
		goto data_free;

	pmd_pri->nr_queues=nr_qeueues;
	
	pmd_pri->mac_address.addr_bytes[0]=0x0;
	pmd_pri->mac_address.addr_bytes[1]=0x0;
	pmd_pri->mac_address.addr_bytes[2]=0x0;
	pmd_pri->mac_address.addr_bytes[3]=0x1;
	pmd_pri->mac_address.addr_bytes[4]=0x2;
	pmd_pri->mac_address.addr_bytes[5]=mac_quantum++;
	
	//pmd_dev->data=data;
	/*since current data is allocated temporary*/
	data->dev_private=pmd_pri;
	data->port_id=pmd_dev->data->port_id;
	sprintf(data->name,"%s",pmd_dev->data->name);
	data->nb_rx_queues=nr_qeueues;
	data->nb_tx_queues=nr_qeueues;
	data->dev_link=vlink_pmd_link;
	data->mac_addrs=&pmd_pri->mac_address;

	pmd_dev->data=data;
	pmd_dev->dev_ops=&pmd_dev_ops;
	pmd_dev->data->dev_flags=RTE_ETH_DEV_DETACHABLE;
	pmd_dev->driver=NULL;
	pmd_dev->data->kdrv=RTE_KDRV_NONE;
	pmd_dev->data->drv_name=drivername;
	
	printf("[x]name init:%s\n",pmd_dev->data->name);
	printf("[x]portid:%d\n",pmd_dev->data->port_id);
	printf("[x]shm:%s\n",shm_file);
	

	pmd_dev->rx_pkt_burst=vlinky_dev_rx;
	pmd_dev->tx_pkt_burst=vlinky_dev_tx;
	/*2.setup control channels shared memory */
	
	pmd_pri->nr_total_queues=pmd_pri->nr_queues*4;/*rx/alloc/free/tx tuple*/
	pmd_pri->shm_size=calculate_shm_size(pmd_pri->nr_total_queues,DEFAULT_QUEUE_LENGTH);
	pmd_pri->shm_base=shm_alloc(shm_file,pmd_pri->shm_size);
	
	if(!pmd_pri->shm_base)
		goto priv_free;

	pmd_pri->queue_stub_array=rte_zmalloc(NULL,sizeof(struct queue_stub *)*pmd_pri->nr_total_queues,0);
	if(!pmd_pri->queue_stub_array)
		goto priv_free;
	
	/*initialize these native queues data structure*/
	for(idx=0;idx<pmd_pri->nr_total_queues;idx++){
		pmd_pri->queue_stub_array[idx]=(struct queue_stub *)(idx*(sizeof(struct queue_stub)+DEFAULT_QUEUE_LENGTH*(sizeof(struct queue_element)))+(unsigned char *)pmd_pri->shm_base);
		tmp_length=(sizeof(struct queue_stub)+DEFAULT_QUEUE_LENGTH*(sizeof(struct queue_element)));
		rc=initialize_queue(pmd_pri->queue_stub_array[idx],tmp_length,(DEFAULT_QUEUE_LENGTH-1));/*actual queue length equals DEFAULT_QUEUE_LENGTH minus one */
		if(rc)
			goto priv_queue_free;
	}
	/*initialize pmd channel*/
	pmd_pri->channles=rte_malloc(NULL,sizeof(struct pmd_vlinky_channel)*pmd_pri->nr_queues,0);
	if(!pmd_pri->channles)
		goto priv_queue_free;
	for(idx=0;idx<pmd_pri->nr_queues;idx++){
		pmd_pri->channles[idx].channel_id=idx;
		/*the exact following order*/
		pmd_pri->channles[idx].rx_queue_stub=pmd_pri->queue_stub_array[idx*4+0];
		pmd_pri->channles[idx].free_queue_stub=pmd_pri->queue_stub_array[idx*4+1];
		pmd_pri->channles[idx].tx_queue_stub=pmd_pri->queue_stub_array[idx*4+2];
		pmd_pri->channles[idx].alloc_queue_stub=pmd_pri->queue_stub_array[idx*4+3];
	}
	for(idx=0;idx<pmd_pri->nr_queues;idx++)
		printf("[vlinky]:%d\n",pmd_pri->channles[idx].channel_id);
	
	
	list_free:
	rte_kvargs_free(kvlist);
	printf("rte:%d\n",ret);
	return ret;
	/*exception return flag*/

	priv_queue_free:
		rte_free(pmd_pri->queue_stub_array);
	priv_free:
		rte_free(pmd_pri);
	data_free:
		rte_free(data);
	dev_free:
		ret=-1;
		
		goto list_free;
}
static int rte_pmd_vlinky_devuninit(const char*name)
{
	printf("[x]name uninit:%s\n",name);
	return 0;
}
static struct rte_driver pmd_vlinky_driver={
	.name="eth_vlinky",
	.type=PMD_VDEV,
	.init=rte_pmd_vlinky_devinit,
	.uninit=rte_pmd_vlinky_devuninit
};


PMD_REGISTER_DRIVER(pmd_vlinky_driver);