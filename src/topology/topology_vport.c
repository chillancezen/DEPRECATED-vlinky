

#include"topology_vport.h"
#include "flex_hash_array.h"

#define VPORT_STUB_LENGTH 1024
#define DOMAIN_STUB_LENGTH 1024
#define DEVICE_STUB_LENGTH 1024


struct hash_table_stub * vport_stub;
struct hash_table_stub * domain_stub;
struct hash_table_stub * device_stub;


int main()
{
	/*
	vport_stub=alloc_flex_stub_array(VPORT_STUB_LENGTH);
	domain_stub=alloc_flex_stub_array(DOMAIN_STUB_LENGTH);
	device_stub=alloc_flex_stub_array(DEVICE_STUB_LENGTH);*/
	
	void * lp=alloc_stub_element(STUB_TYPE_VPORT);
	((struct topology_vport*)lp)->vport_id[7]=0x10;
	printf("%08x\n",calculate_hash_value(lp,STUB_TYPE_VPORT));
	
	lp=alloc_stub_element(STUB_TYPE_DOMAIN);
	printf("%08x\n",calculate_hash_value(lp,STUB_TYPE_DOMAIN));
	
	return 0;
}


uint32_t calculate_hash_value(void * ele,enum STUB_TYPE type)
{
	uint32_t sum=0;
	struct topology_vport *lp_tv;
	struct topology_lan_domain * lp_tid;
	struct topology_device *lp_td;
	switch(type)
	{
		case STUB_TYPE_VPORT:
			lp_tv=(struct topology_vport *)ele;
			sum=jhash(lp_tv->vport_id,6,0x0);
			break;
		case STUB_TYPE_DOMAIN:
			lp_tid=(struct topology_lan_domain *)ele;
			sum=jhash(&lp_tid->domain_id,4,0x0);
			break;
		case STUB_TYPE_DEVICE:
			lp_td=(struct topology_device *)ele;
			sum=jhash(&lp_td->chassis_id,4,0x0);
			break;
		default:
			
			break;
	}
	return sum;
}
void * alloc_stub_element(enum STUB_TYPE type)
{
	void * ret=NULL;
	switch(type)
	{
		case STUB_TYPE_VPORT:
			ret=malloc(sizeof(struct topology_vport));
			if(ret)
				memset(ret,0x0,sizeof(struct topology_vport));
			break;
		case STUB_TYPE_DOMAIN:
			ret=malloc(sizeof(struct topology_lan_domain));
			if(ret)
				memset(ret,0x0,sizeof(struct topology_lan_domain));
			break;
		case STUB_TYPE_DEVICE:
			ret=malloc(sizeof(struct topology_device));
			if(ret)
				memset(ret,0x0,sizeof(struct topology_device));
			break;
		default:
			break;
	}
	
	return ret;
}
void * insert_hash_element(struct hash_table_stub *hts,void *ele_tmp,enum STUB_TYPE type)
{
//allocate a node and insert it into the hash head
	struct topology_vport *lp_tv,*lp_tv_tmp;
	struct topology_lan_domain * lp_tid,*lp_tid_tmp;
	struct topology_device *lp_td,*lp_td_tmp;
	void * ret=NULL;
	int index=calculate_hash_value(ele_tmp,type);
	switch(type)
	{
		case STUB_TYPE_VPORT:
			lp_tv=(struct topology_vport *)ele_tmp;
			lp_tv_tmp=(struct topology_vport *)alloc_stub_element(STUB_TYPE_VPORT);
			index%=VPORT_STUB_LENGTH;
			lp_tv_tmp->hash_tbl_next=(struct topology_vport *)hts[index].header_ptr;
			hts[index].header_ptr=lp_tv_tmp;
			ret=lp_tv_tmp;
			break;
		case STUB_TYPE_DOMAIN:
			lp_tid=(struct topology_lan_domain *)ele_tmp;
			lp_tid_tmp=(struct topology_lan_domain *)alloc_stub_element(STUB_TYPE_DOMAIN);
			index%=DOMAIN_STUB_LENGTH;
			lp_tid_tmp->hash_tbl_next=(struct topology_lan_domain *)hts[index].header_ptr;
			hts[index].header_ptr=lp_tid_tmp;
			ret=lp_tid_tmp;
			break;
		case STUB_TYPE_DEVICE:
			lp_td=(struct topology_device *)ele_tmp;
			lp_td_tmp=(struct topology_device *)alloc_stub_element(STUB_TYPE_DEVICE);
			index%=DEVICE_STUB_LENGTH;
			lp_td_tmp->hash_tbl_next=(struct topology_device *)hts[index].header_ptr;
			hts[index].header_ptr=lp_td_tmp;
			ret=lp_td_tmp;
			break;
	}

	return ret;
}
void * index_hash_element(struct hash_table_stub*hts,void*ele_tmp,enum STUB_TYPE type)
{
	void * ret=NULL;
	struct topology_vport *lp_tv,*lp_tv_head;
	struct topology_lan_domain * lp_tid,*lp_tid_head;
	struct topology_device *lp_td,*lp_td_head;
	int index=calculate_hash_value(ele_tmp,type);
	
	switch(type)
	{
		case STUB_TYPE_VPORT:
			lp_tv=(struct topology_vport *)ele_tmp;
			index%=VPORT_STUB_LENGTH;
			lp_tv_head=(struct topology_vport *)hts[index].header_ptr;
			while(lp_tv_head){
				if(mac_address_equal(lp_tv->vport_id,lp_tv_head->vport_id)){
					ret=lp_tv_head;
					break;
				}
				lp_tv_head=lp_tv_head->hash_tbl_next;
			}
			break;
		case STUB_TYPE_DOMAIN:
			lp_tid=(struct topology_lan_domain *)ele_tmp;
			index%=DOMAIN_STUB_LENGTH;
			lp_tid_head=(struct topology_lan_domain*)hts[index].header_ptr;
			while(lp_tid_head){
				if(lp_tid->domain_id==lp_tid_head->domain_id){
					ret=lp_tid_head;
					break;
				}
				lp_tid_head=lp_tid_head->hash_tbl_next;
			}
			break;
		case STUB_TYPE_DEVICE:
			lp_td=(struct topology_device *)ele_tmp;
			index%=DEVICE_STUB_LENGTH;
			lp_td_head=(struct topology_device *)ele_tmp;
			while(lp_tid_head){
				if(lp_td->chassis_id==lp_td_head->chassis_id){
					ret=lp_td_head;
					break;
				}
				lp_tid_head=lp_tid_head->hash_tbl_next;
			}
			break;
	}
	return ret;
}