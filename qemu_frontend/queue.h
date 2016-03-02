/*here is a shared queue that will be used between guest and  hypervisor,
for simplicity,here it's a single producer and single consumer queue
here we begin
*/
#define __QEMU_CONTEXT

#if 0
#include <rte_atomic.h>
typedef long long offset_t  ;/*exact 8-bytesss,theses should be platform dependent*/
typedef int  my_int_32;
#define WRITE_MEM_WB() rte_wmb()
#else 
#include<qemu/atomic.h>
#include <assert.h>
typedef long long offset_t ;
typedef int my_int32;
typedef int  my_int_32;
#define WRITE_MEM_WB() smp_wmb()
#endif


#pragma pack(1)
/*as for queue element,we use 2 levels offset which would be enough to transmit a element structure one time*/
struct queue_element{
	union{
		offset_t  rte_pkt_offset;/*here we get 8-byte,when porting,this may be changed*/
		offset_t  level1_offset;
	};
	union {
		offset_t  rte_data_offset;
		offset_t  level2_offset;
	};
};

#define QUEUE_CHANNEL_ACTIVE 0x01

struct queue_stub{
	
	my_int_32 ele_num;
	my_int_32 ele_size;
	my_int_32 front_ptr;/*update by consumer,read by both*/
	my_int_32 rear_ptr;/*updtae by produccer,read by both*/

	union {
		unsigned char dummy[16];//align to 16 bytes
		struct {
			my_int32 queue_channel_state;
		};
	};
	struct queue_element  records[0];/*this must be last field*/
};

int initialize_queue(void *buffer,int size,int queue_ele_nr);
int queue_quantum(struct queue_stub *stub);
int enqueue_single(struct queue_stub* stub,struct queue_element *ele);
int dequeue_single(struct queue_stub *stub,struct queue_element*ele);
int enqueue_bulk(struct queue_stub *stub,struct queue_element **ele_arr,int length);
int dequeue_bulk(struct queue_stub * stub,struct queue_element **ele_arr,int max_length);


