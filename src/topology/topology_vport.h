
#include <linux/types.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

#ifndef _TOPOLOGY_VPORT
#define _TOPOLOGY_VPORT
struct topology_lan_domain;
struct topology_device;

struct topology_vport{
	uint8_t vport_id[6];
	struct topology_lan_domain *ld;
	struct topology_device * td;
	struct topology_vport *hash_tbl_next;
	struct topology_vport *domain_next_vport_ptr;
	struct topology_vport *device_next_vport_ptr;
};

struct topology_lan_domain{
	uint32_t domain_id;
	int vport_count;
	struct topology_vport *first_vport_ptr;
	struct topology_lan_domain *hash_tbl_next;
	struct topology_lan_domain *global_list_next;
	
};

struct topology_device{
	uint32_t chassis_id;
	int vport_count;
	struct topology_vport *first_vport_ptr;
	struct topology_device *hash_tbl_next;
	struct topology_device *global_list_next;
};

struct hash_table_stub{
	void* header_ptr;
};

enum STUB_TYPE{
	STUB_TYPE_VPORT=1,
	STUB_TYPE_DOMAIN=2,
	STUB_TYPE_DEVICE=3
};

void * alloc_stub_element(enum STUB_TYPE type);
void dealloc_stub_element(void* ele);

uint32_t calculate_hash_value(void * ele,enum STUB_TYPE type);
void * index_hash_element(struct hash_table_stub*hts,void*ele_tmp,enum STUB_TYPE type);
void * delete_hash_element(struct hash_table_stub*hts,void * ele,enum STUB_TYPE type);
void * insert_hash_element(struct hash_table_stub *hts,void *ele_tmp,enum STUB_TYPE type);
#define FOREACH_VPORT_IN_DOMAIN(dom,lptr) for((lptr)=((struct topology_lan_domain*)(dom))->first_vport_ptr;\
											(lptr);\
											(lptr)=(lptr)->domain_next_vport_ptr)
#define FOREACH_VPORT_IN_DEVICE(dev,lptr) for((lptr)=((struct topology_device*)(dev))->first_vport_ptr;\
											(lptr);\
											(lptr)=(lptr)->first_vport_ptr);


#endif
