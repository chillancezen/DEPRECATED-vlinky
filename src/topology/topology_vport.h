
#include <linux/types.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

#ifndef _TOPOLOGY_VPORT
#define _TOPOLOGY_VPORT
struct lan_domain;
struct topology_device;

struct topology_vport{
	uint8_t vport_id[6];
	struct lan_domain *ld;
	struct topology_device * td;
	struct topology_vport *hash_tbl_next;
};

struct topology_lan_domain{
	uint32_t domain_id;
	struct topology_vport *first_vport_ptr;
	struct topology_lan_domain *hash_tbl_next;
	
};

struct topology_device{
	uint32_t chassis_id;
	struct topology_vport *first_vport_ptr;
	struct topology_device *hash_tbl_next;
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
uint32_t calculate_hash_value(void * ele,enum STUB_TYPE type);


#endif
