
#include "topology_vport.h"
extern struct hash_table_stub * vport_stub;
extern struct hash_table_stub * domain_stub;
extern struct hash_table_stub * device_stub;
extern struct topology_lan_domain domain_head;
extern struct topology_device     device_head;


int is_neighbour(struct topology_vport*vport1,struct topology_vport*vport2)
{
	if(!vport1->ld||!vport2->ld)
		return  FALSE;
	return vport1->ld==vport2->ld;
}
int is_in_the_same_chassis(struct topology_vport*vport1,struct topology_vport*vport2)
{
	if(!vport1->td||!vport2->td)
		return FALSE;
	return vport1->td==vport2->td;
}
struct topology_vport * find_all_neighbours(struct topology_vport * vport)
{

	struct topology_vport * ret_header=NULL;
	struct topology_vport *lptr;
	struct topology_vport *vport_ptr=index_hash_element(vport_stub,vport,STUB_TYPE_VPORT);
	if(!vport_ptr||!vport_ptr->ld)
		return NULL;
	lptr=vport_ptr->ld->first_vport_ptr;
	for(;lptr;lptr=lptr->domain_next_vport_ptr){
		if(lptr==vport_ptr)
			continue;
		lptr->neighbour_ship_next_vport_ptr=ret_header;
		ret_header=lptr;
	}
	return ret_header;
}
struct topology_vport * find_all_vports_in_the_same_chassis(struct topology_vport * vport)
{
	struct topology_vport * ret_header=NULL;
	struct topology_vport *lptr;
	struct topology_vport *vport_ptr=index_hash_element(vport_stub,vport,STUB_TYPE_VPORT);
	if(!vport_ptr||!vport_ptr->td)
		return NULL;
	lptr=vport_ptr->td->first_vport_ptr;
	for(;lptr;lptr=lptr->device_next_vport_ptr){
		if(lptr==vport_ptr)
			continue;
		lptr->neighbour_ship_next_vport_ptr=ret_header;
		ret_header=lptr;
	}
	return ret_header;

}
int main()
{
	topology_init();
	struct topology_vport vport1;
	struct topology_vport vport2;

	copy_mac_address(vport1.vport_id,"\x12\x12\x12\x12\x12\x12");
	copy_mac_address(vport2.vport_id,"\x12\x12\x12\x12\x12\x13");

	int rc=add_vport_node_pairs_into_topology(vport_stub,domain_stub,device_stub,&device_head,&domain_head,&vport1,0x5555,&vport2,0x5556,0x12);
	printf("rc %d\n",rc);
	copy_mac_address(vport1.vport_id,"\x12\x12\x12\x12\x12\x11");
	copy_mac_address(vport2.vport_id,"\x12\x12\x12\x12\x12\x14");
	rc=add_vport_node_pairs_into_topology(vport_stub,domain_stub,device_stub,&device_head,&domain_head,&vport1,0x5555,&vport2,0x5557,0x12);
	printf("rc %d\n",rc);

	struct topology_vport *vport1_ptr;
	struct topology_vport *vport2_ptr;
	struct topology_vport *vport3_ptr;
	struct topology_vport *vport4_ptr;
	
	copy_mac_address(vport1.vport_id,"\x12\x12\x12\x12\x12\x12");
	vport1_ptr=index_hash_element(vport_stub,&vport1,STUB_TYPE_VPORT);
	copy_mac_address(vport1.vport_id,"\x12\x12\x12\x12\x12\x13");
	vport2_ptr=index_hash_element(vport_stub,&vport1,STUB_TYPE_VPORT);
	copy_mac_address(vport1.vport_id,"\x12\x12\x12\x12\x12\x11");
	vport3_ptr=index_hash_element(vport_stub,&vport1,STUB_TYPE_VPORT);
	copy_mac_address(vport1.vport_id,"\x12\x12\x12\x12\x12\x14");
	vport4_ptr=index_hash_element(vport_stub,&vport1,STUB_TYPE_VPORT);

	struct topology_vport *hdr=find_all_vports_in_the_same_chassis(vport4_ptr);
	while(hdr){

		printf("0x%x\n",hdr);
		hdr=hdr->neighbour_ship_next_vport_ptr;
	}

	printf("%d\n",is_in_the_same_chassis(vport1_ptr,vport2_ptr));
	printf("%d\n",is_in_the_same_chassis(vport1_ptr,vport3_ptr));
	printf("%d\n",is_in_the_same_chassis(vport2_ptr,vport4_ptr));
	
	
	return 0;
}
