#include "topology_vport.h"

#ifndef FLEX_HASH_ARRAY
#define FLEX_HASH_ARRAY
void *alloc_flex_hash_array(int ele_size,int ele_length,void (*ele_init)(void*));
struct hash_table_stub * alloc_flex_stub_array(int ele_length);

#endif 
