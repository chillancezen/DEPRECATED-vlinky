#include "flex_hash_array.h"


#include <stdio.h>
#include <stdlib.h>

void *alloc_flex_hash_array(int ele_size,int ele_length,void (*ele_init)(void*))
{
	void* *base=NULL;
	unsigned char *lptr;
	int idx=0;
	
	base=(void*)malloc(ele_size*ele_length);
	if(!base)
		goto ret_flag;
	lptr=(unsigned char*)base;
	for(;idx<ele_length;idx++){
		ele_init(lptr+idx*ele_size);
	}
	ret_flag:
	return base;
}
void stub_ele_init(void* ele)
{
	struct hash_table_stub * hts=(struct hash_table_stub*)ele;
	hts->header_ptr=NULL;
}
	
struct hash_table_stub * alloc_flex_stub_array(int ele_length)
{
	struct hash_table_stub * base=alloc_flex_hash_array(
		sizeof(struct hash_table_stub),
		ele_length,
		stub_ele_init);
	return base;
}


