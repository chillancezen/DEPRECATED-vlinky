#include <stdio.h>
#include "queue.h"
#include "shm_util.h"
#include <assert.h>
#include "vswitch_vlink.h"
#include "rte_config.h"
#include "rte_mbuf.h"

int main()
{
	int dpdk_size;
	void * dpdk_base;
	void * base=shm_alloc("cute",1311104);
	int idx;
	int rx;
	int rc;
	struct rte_mbuf *mbuf;
	struct queue_element qe_array[32];
	struct queue_element *qe_ptr_array[32];
	for(idx=0;idx<32;idx++){
		qe_ptr_array[idx]=&qe_array[idx];
	}
	assert(base);

	rc=attach_to_dpdk_hugepage_memory(&dpdk_base,&dpdk_size);
	assert(!rc);
	printf("dpdk:%llx (%d)\n",dpdk_base,dpdk_size);
	
	struct queue_stub * rx_stub;
	struct queue_stub * free_stub;
	struct queue_stub * tx_stub;
	struct queue_stub * alloc_stub;
	unsigned char* mac_header;
	rx_stub=(struct queue_stub*)(0+(char*)base);
	free_stub=(struct queue_stub*)(0+163888+(char*)base);
	tx_stub=(struct queue_stub*)(0+163888+163888+(char*)base);
	alloc_stub=(struct queue_stub*)(0+163888+163888+163888+(char*)base);

	printf("active:%d\n",!!(rx_stub->queue_stub_state&QUEUE_STUB_ACTIVE));
	#if 0
	printf("rx   %d\n",queue_quantum(rx_stub));	
	printf("free %d\n",queue_quantum(free_stub));	
	printf("tx   %d\n",queue_quantum(tx_stub));	
	printf("allo %d\n",queue_quantum(alloc_stub));
	#endif
	printf("size:%d\n",sizeof(struct rte_mbuf));
	while(1){
		rx=dequeue_bulk(rx_stub,qe_ptr_array,32);
		if(rx){
			//printf("rx:%d\n",rx);
		}

		#if 0
		for(idx=0;idx<rx;idx++){
			mbuf=(struct rte_mbuf*)(qe_ptr_array[idx]->rte_pkt_offset+(char*)dpdk_base);
			mbuf->buf_addr=(void*)(qe_ptr_array[idx]->rte_data_offset+(char*)dpdk_base);
			mac_header=mbuf->data_off+(unsigned char*)mbuf->buf_addr;
			printf("pkt: %02x:%02x:%02x:%02x:%02x:%02x\n",mac_header[0+12]
				,mac_header[1+12]
				,mac_header[2+6]
				,mac_header[3+6]
				,mac_header[4+6]
				,mac_header[5+6]);
		}
		#endif 
		/*relase them immediately */
		assert(queue_quantum(free_stub)>=rx);
		enqueue_bulk(tx_stub,qe_ptr_array,rx);
	}
	
	return 0;
}

