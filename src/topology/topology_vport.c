

#include"topology_vport.h"
#include "flex_hash_array.h"

#define VPORT_STUB_LENGTH 2
#define DOMAIN_STUB_LENGTH 16
#define DEVICE_STUB_LENGTH 1024


struct hash_table_stub * vport_stub;
struct hash_table_stub * domain_stub;
struct hash_table_stub * device_stub;
struct topology_lan_domain domain_head;
struct topology_device     device_head;

void topology_init()
{
	vport_stub=alloc_flex_stub_array(VPORT_STUB_LENGTH);
	domain_stub=alloc_flex_stub_array(DOMAIN_STUB_LENGTH);
	device_stub=alloc_flex_stub_array(DEVICE_STUB_LENGTH);
	memset(&domain_head,0x0,sizeof(struct topology_lan_domain));
	memset(&device_head,0x0,sizeof(struct topology_device));
}
int add_vport_nodes(struct hash_table_stub*hts_vport,
	struct hash_table_stub*hts_domain,
	struct hash_table_stub*hts_device,
	struct topology_vport*ele_tmp1,
	struct topology_vport*ele_tmp2){
	struct topology_vport * ele1;
	struct topology_vport * ele2;
	ele1=index_hash_element(hts_vport,ele_tmp1,STUB_TYPE_VPORT);
	ele2=index_hash_element(hts_vport,ele_tmp2,STUB_TYPE_VPORT);

	if(!ele1)
		ele1=insert_hash_element(hts_vport,ele_tmp1,STUB_TYPE_VPORT);
	if(!ele2)
		ele2=insert_hash_element(hts_vport,ele_tmp1,STUB_TYPE_VPORT);
	if(!ele1||!ele2){/*here we let it be there and we don't reclaim them if not successfull*/
		return -1;
	}
	/*here we must emerge them into potential domain and device list*/
	
	return 0;
}

void add_vport_to_domain(struct topology_lan_domain *domain,struct topology_vport*vport)
{
	/*sanity-check whether the vport already in the domain list*/
	struct topology_vport *lptr;
	FOREACH_VPORT_IN_DOMAIN(domain,lptr){
		if(lptr==vport)
			return;
	}
	vport->domain_next_vport_ptr=NULL;//reset the domain next pointers
	/*add it to the end of list*/
	if(!domain->first_vport_ptr)
		domain->first_vport_ptr=vport;
	else{
		lptr=domain->first_vport_ptr;
		while(lptr->domain_next_vport_ptr)
			lptr=lptr->domain_next_vport_ptr;
		lptr->domain_next_vport_ptr=vport;
	}
	domain->vport_count++;
	vport->ld=domain;
}
void remove_vport_from_domain(struct topology_lan_domain*domain,struct topology_vport*vport)
{
	struct topology_vport*lptr,*last=NULL;
	FOREACH_VPORT_IN_DOMAIN(domain,lptr){
		if(lptr==vport){//found it here,unlink it 
			if(!last)
				domain->first_vport_ptr=vport->domain_next_vport_ptr;
			else
				last->domain_next_vport_ptr=vport->domain_next_vport_ptr;
			domain->vport_count--;
			vport->ld=NULL;
			vport->domain_next_vport_ptr=NULL;
			break;
		}
		last=lptr;
	}
}
void add_domain_into_global_list(struct topology_lan_domain *head,struct topology_lan_domain* domain)
{
	struct topology_lan_domain* lptr=head;
	while(lptr->global_list_next){
		if(lptr->global_list_next==domain)//found it's already in the 
			return ;
		lptr=lptr->global_list_next;
	}
	lptr->global_list_next=domain;
}
void remove_domain_from_global_list(struct topology_lan_domain*head,struct topology_lan_domain* domain)
{
	struct topology_lan_domain *lptr=head;
	while(lptr->global_list_next){
		if(lptr->global_list_next==domain){
			lptr->global_list_next=domain->global_list_next;
			break;
			}
		lptr=lptr->global_list_next;
	}
}
void add_device_into_global_list(struct topology_device* head,struct topology_device*device)
{
	struct topology_device *lptr=head;
	while(lptr->global_list_next){
		if(lptr->global_list_next==device)
			return ;
		lptr=lptr->global_list_next;
	}
	lptr->global_list_next=device;
}
void remove_device_from_global_list(struct topology_device*head,struct topology_device*device)
{
	struct topology_device*lptr=head;
	while(lptr->global_list_next){
		if(lptr->global_list_next==device){
			lptr->global_list_next=device->global_list_next;
			break;
		}
		lptr=lptr->global_list_next;
	}
}
int main()
{
	topology_init();
	
	int idx;

	struct topology_lan_domain*tld_base=alloc_stub_element(STUB_TYPE_DOMAIN);
	struct topology_lan_domain *tld_index;
	tld_base->domain_id=uuid_alloc();
	insert_hash_element(domain_stub,tld_base,STUB_TYPE_DOMAIN);
	tld_index=index_hash_element(domain_stub,tld_base,STUB_TYPE_DOMAIN);
	if(tld_index)
	add_domain_into_global_list(&domain_head,tld_index);

	tld_base->domain_id=uuid_alloc();
	insert_hash_element(domain_stub,tld_base,STUB_TYPE_DOMAIN);
	tld_index=index_hash_element(domain_stub,tld_base,STUB_TYPE_DOMAIN);
	if(tld_index)
	add_domain_into_global_list(&domain_head,tld_index);

	tld_base->domain_id=uuid_alloc();
	insert_hash_element(domain_stub,tld_base,STUB_TYPE_DOMAIN);
	tld_index=index_hash_element(domain_stub,tld_base,STUB_TYPE_DOMAIN);
	if(tld_index)
	add_domain_into_global_list(&domain_head,tld_index);


	tld_base->domain_id=0x12345671;
	tld_index=index_hash_element(domain_stub,tld_base,STUB_TYPE_DOMAIN);
	if(tld_index){
		remove_domain_from_global_list(&domain_head,tld_index);
		delete_hash_element(domain_stub,tld_index,STUB_TYPE_DOMAIN);
	}
	
	for(idx=0;idx<DOMAIN_STUB_LENGTH;idx++){
		printf("%d:",idx);
		struct topology_lan_domain * lp_tv=domain_stub[idx].header_ptr;
		while(lp_tv){
			printf("%08x,",lp_tv);
			lp_tv=lp_tv->hash_tbl_next;
		}
		printf("\n");
	}
	
	struct topology_lan_domain*tld=domain_head.global_list_next;
	while(tld){
		printf("0x%08x\n",tld->domain_id);
		tld=tld->global_list_next;
	}
	
/*
	struct topology_vport* vport;
	vport_stub=alloc_flex_stub_array(VPORT_STUB_LENGTH);
	vport=alloc_stub_element(STUB_TYPE_VPORT);
	copy_mac_address(vport->vport_id,"\x01\x35\x00\x78\xcc\xcc");
	struct topology_vport* vport_new=insert_hash_element(vport_stub,vport,STUB_TYPE_VPORT);
	printf("indesrt1:%08x\n",vport_new);

	copy_mac_address(vport->vport_id,"\x02\x33\x00\x78\xcc\xcc");
	struct topology_vport* vport_new1=insert_hash_element(vport_stub,vport,STUB_TYPE_VPORT);
	printf("indesrt2:%08x\n",vport_new1);
	
	copy_mac_address(vport->vport_id,"\x02\x53\x0f\x78\xcc\xcc");
	struct topology_vport* vport_new2=insert_hash_element(vport_stub,vport,STUB_TYPE_VPORT);
	printf("indesrt3:%08x\n",vport_new2);

	copy_mac_address(vport->vport_id,"\x02\x53\x0f\x78\xcd\x2c");
	struct topology_vport* vport_new3=insert_hash_element(vport_stub,vport,STUB_TYPE_VPORT);
	printf("indesrt4:%08x\n",vport_new3);

	copy_mac_address(vport->vport_id,"\xf2\x53\x0f\x78\x9d\x23");
	struct topology_vport* vport_new4=insert_hash_element(vport_stub,vport,STUB_TYPE_VPORT);
	printf("indesrt5:%08x\n",vport_new4);
	
	int idx;
	for(idx=0;idx<VPORT_STUB_LENGTH;idx++){
		printf("%d:",idx);
		struct topology_vport * lp_tv=vport_stub[idx].header_ptr;
		while(lp_tv){
			printf("%08x,",lp_tv);
			lp_tv=lp_tv->hash_tbl_next;
		}
		printf("\n");
	}
	struct topology_vport * dele=delete_hash_element(vport_stub,vport_new3,STUB_TYPE_VPORT);
	dele=delete_hash_element(vport_stub,vport_new3,STUB_TYPE_VPORT);
	printf("delete:%08x\n",dele);
		for(idx=0;idx<VPORT_STUB_LENGTH;idx++){
		printf("%d:",idx);
		struct topology_vport * lp_tv=vport_stub[idx].header_ptr;
		while(lp_tv){
			printf("%08x,",lp_tv);
			lp_tv=lp_tv->hash_tbl_next;
		}
		printf("\n");
	}
	
	
	vport_stub=alloc_flex_stub_array(VPORT_STUB_LENGTH);
	domain_stub=alloc_flex_stub_array(DOMAIN_STUB_LENGTH);
	device_stub=alloc_flex_stub_array(DEVICE_STUB_LENGTH);
	
	void * lp=alloc_stub_element(STUB_TYPE_VPORT);
	((struct topology_vport*)lp)->vport_id[7]=0x10;
	printf("%08x\n",calculate_hash_value(lp,STUB_TYPE_VPORT));
	
	lp=alloc_stub_element(STUB_TYPE_DOMAIN);
	printf("%08x\n",calculate_hash_value(lp,STUB_TYPE_DOMAIN));
	*/
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
void dealloc_stub_element(void* ele)
{
	if(ele)
		free(ele);
}
/*here insert do not check whether item exists,caller must guarantee this*/
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
			if(!lp_tv_tmp)
				break;
			/*copy primary key*/
			copy_mac_address(lp_tv_tmp->vport_id,lp_tv->vport_id);
			index&=(VPORT_STUB_LENGTH-1);
			lp_tv_tmp->hash_tbl_next=(struct topology_vport *)hts[index].header_ptr;
			hts[index].header_ptr=lp_tv_tmp;
			ret=lp_tv_tmp;
			break;
		case STUB_TYPE_DOMAIN:
			
			lp_tid=(struct topology_lan_domain *)ele_tmp;
			lp_tid_tmp=(struct topology_lan_domain *)alloc_stub_element(STUB_TYPE_DOMAIN);
			
			if(!lp_tid_tmp)
				break;
			lp_tid_tmp->domain_id=lp_tid->domain_id;
			index&=(DOMAIN_STUB_LENGTH-1);
			lp_tid_tmp->hash_tbl_next=(struct topology_lan_domain *)hts[index].header_ptr;
			hts[index].header_ptr=lp_tid_tmp;
			ret=lp_tid_tmp;
			break;
		case STUB_TYPE_DEVICE:
			lp_td=(struct topology_device *)ele_tmp;
			lp_td_tmp=(struct topology_device *)alloc_stub_element(STUB_TYPE_DEVICE);
			
			if(!lp_td_tmp)
				break;
			lp_td_tmp->chassis_id=lp_td->chassis_id;
			index&=(DEVICE_STUB_LENGTH-1);
			lp_td_tmp->hash_tbl_next=(struct topology_device *)hts[index].header_ptr;
			hts[index].header_ptr=lp_td_tmp;
			ret=lp_td_tmp;
			break;
	}

	return ret;
}
//remove from hash list ,but we do not dealloc it,and return it to caller
void * delete_hash_element(struct hash_table_stub*hts,void * ele,enum STUB_TYPE type)
{
	void *ret=NULL;
	struct topology_vport *lp_tv;
	struct topology_lan_domain * lp_tid;
	struct topology_device *lp_td;
	int index=calculate_hash_value(ele,type);
	switch(type)
	{
		case STUB_TYPE_VPORT:
			index&=(VPORT_STUB_LENGTH-1);
			lp_tv=(struct topology_vport*)hts[index].header_ptr;
			
			if(lp_tv==ele){/*in case that it's the first nodes in the hash list*/
				hts[index].header_ptr=lp_tv?lp_tv->hash_tbl_next:NULL;
				ret=lp_tv;
			}else if(lp_tv){
				while(lp_tv->hash_tbl_next){
					
					if(lp_tv->hash_tbl_next==ele){//in case this is lp_tv is last node and ele is NULL
						ret=lp_tv->hash_tbl_next;
						lp_tv->hash_tbl_next=ret?lp_tv->hash_tbl_next->hash_tbl_next:NULL;
						break;
					}
					lp_tv=lp_tv->hash_tbl_next;
				}
			}
			break;
		case STUB_TYPE_DOMAIN:
			index&=(DOMAIN_STUB_LENGTH-1);
			lp_tid=(struct topology_lan_domain*) hts[index].header_ptr;
			if(lp_tid==ele){
				hts[index].header_ptr=lp_tid?lp_tid->hash_tbl_next:NULL;
				ret=lp_tid;
			}else if(lp_tid){
				while(lp_tid->hash_tbl_next){
					if(lp_tid->hash_tbl_next==ele){
						ret=lp_tid->hash_tbl_next;
						lp_tid->hash_tbl_next=ret?lp_tid->hash_tbl_next->hash_tbl_next:NULL;
						break;
					}
					lp_tid=lp_tid->hash_tbl_next;
				}
			}
			break;
		case STUB_TYPE_DEVICE:
			index&=(DEVICE_STUB_LENGTH-1);
			lp_td=(struct topology_device*)hts[index].header_ptr;
			if(lp_td==ele){
				hts[index].header_ptr=lp_td?lp_td->hash_tbl_next:NULL;
				ret=lp_td;
			}else if(lp_td){
				while(lp_td->hash_tbl_next){
					if(lp_td->hash_tbl_next==ele){
						ret=lp_td->hash_tbl_next;
						lp_td->hash_tbl_next=ret?lp_td->hash_tbl_next->hash_tbl_next:NULL;
						break;
					}
					lp_td=lp_td->hash_tbl_next;
				}
			}
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
			index&=(VPORT_STUB_LENGTH-1);
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
			index&=(DOMAIN_STUB_LENGTH-1);
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
			index&=(DEVICE_STUB_LENGTH-1);
			lp_td_head=(struct topology_device *)hts[index].header_ptr;
			while(lp_td_head){
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