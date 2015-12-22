
#include "topology_vport.h"
extern struct hash_table_stub * vport_stub;
extern struct hash_table_stub * domain_stub;
extern struct hash_table_stub * device_stub;
extern struct topology_lan_domain domain_head;
extern struct topology_device     device_head;


#define EDGE_STR_NUM (1024*4)
struct edge_str edge_array[EDGE_STR_NUM+1];/*the 1st node would be the first*/


static void init_edge_array()
{
	int idx=0;
	for(idx=1;idx<(EDGE_STR_NUM+1);idx++){
		edge_array[idx].directed_vport=NULL;
		edge_array[idx].next=NULL;
		edge_array[idx-1].next=&edge_array[idx];
	}
}
int debug_cnt=EDGE_STR_NUM;
static struct edge_str* alloc_edge_element()
{	
	struct edge_str * ret=NULL;
	if(edge_array[0].next){
		ret=edge_array[0].next;
		edge_array[0].next=ret->next;
	}else{
		ret=(struct edge_str*)malloc(sizeof(struct edge_str));
	}
	if(ret)
		memset(ret,0x0,sizeof(struct edge_str));
	debug_cnt--;
	return ret;
}
static void dealloc_edge_element(struct edge_str *ele)
{
	debug_cnt++;
	ele->next=edge_array[0].next;
	edge_array[0].next=ele;
}
void topology_ops_init()
{	
	init_edge_array();
}
int is_the_same_vport(struct topology_vport*_vport1,struct topology_vport*_vport2)
{
	struct topology_vport* vport1=index_hash_element(vport_stub,_vport1,STUB_TYPE_VPORT);
	struct topology_vport* vport2=index_hash_element(vport_stub,_vport2,STUB_TYPE_VPORT);
	if(!vport1||!vport2)
			return FALSE;
	return vport1==vport2;

}
int is_neighbour(struct topology_vport*_vport1,struct topology_vport*_vport2)
{	
	struct topology_vport* vport1=index_hash_element(vport_stub,_vport1,STUB_TYPE_VPORT);
	struct topology_vport* vport2=index_hash_element(vport_stub,_vport2,STUB_TYPE_VPORT);

	if(!vport1||!vport2)
		return FALSE;
	
	if(!vport1->ld||!vport2->ld)
		return  FALSE;
	return vport1->ld==vport2->ld;
}
int is_in_the_same_chassis(struct topology_vport*_vport1,struct topology_vport*_vport2)
{
	struct topology_vport* vport1=index_hash_element(vport_stub,_vport1,STUB_TYPE_VPORT);
	struct topology_vport* vport2=index_hash_element(vport_stub,_vport2,STUB_TYPE_VPORT);
	
	if(!vport1||!vport2)
		return FALSE;

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
/*
find all the adjacent vport nodes which may be neighbours ,or in the same chassis or both.
*/
struct topology_vport *find_all_adjacent_vports(struct topology_vport*vport)
{
	struct topology_vport * ret_header=NULL;
	struct topology_vport *vport_ptr=index_hash_element(vport_stub,vport,STUB_TYPE_VPORT);
	if(!vport_ptr||!vport_ptr->ld)
		return NULL;
	struct topology_vport *lptr;
	struct topology_vport *lptr_tmp;
	lptr=vport_ptr->ld->first_vport_ptr;
	for(;lptr;lptr=lptr->domain_next_vport_ptr){
		if(lptr==vport_ptr)
			continue;
		lptr->neighbour_ship_next_vport_ptr=ret_header;
		ret_header=lptr;
	}
	lptr=vport_ptr->td->first_vport_ptr;
	for(;lptr;lptr=lptr->device_next_vport_ptr){
		if(lptr==vport_ptr)
			continue;
		/*lookup current list to check whether the vport exist in the list*/
		lptr_tmp=ret_header;
		for(;lptr_tmp;lptr_tmp=lptr_tmp->neighbour_ship_next_vport_ptr)
			if(lptr_tmp==lptr)
				break;
		if(lptr_tmp)
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
struct  topology_vport * find_all_vports()
{
	struct topology_vport *ret_header=NULL;
	struct topology_vport *vport_ptr=NULL;
	
	struct topology_device * device_ptr=device_head.global_list_next;
	while(device_ptr){
		vport_ptr=device_ptr->first_vport_ptr;
		while(vport_ptr){

			vport_ptr->neighbour_ship_next_vport_ptr=ret_header;
			ret_header=vport_ptr;
			vport_ptr=vport_ptr->device_next_vport_ptr;
		}
		device_ptr=device_ptr->global_list_next;
	}
	return ret_header;
}
void dijstra_reset_vports(struct topology_vport * vport_src)
{
	struct topology_vport *vport=find_all_vports();
	while(vport){
		vport->is_permanent=0;
		vport->dijstra_backward_ptr=NULL;
		//vport->dijstra_first_backward_ptr=NULL;
		vport->dijstra_next_ecmp_ptr=NULL;
		vport->dijstra_permanent_ptr=NULL;
		vport->dijstra_backward_edge_ptr=NULL;
		vport->upward_vport_count=0;
		vport->cost_to_src=cost_between_vports(vport_src,vport);
		vport=vport->neighbour_ship_next_vport_ptr;
		
	}
}

int dijstra_find_short_ecmp_path(struct topology_vport*_vport_src,struct topology_vport *_vport_dst)
{
	struct topology_vport * vport_src=index_hash_element(vport_stub,_vport_src,STUB_TYPE_VPORT);
	struct topology_vport * vport_dst=index_hash_element(vport_stub,_vport_dst,STUB_TYPE_VPORT);
	struct topology_vport * vport_permanent_ptr;
	struct topology_vport permanent_vport_header={
		.dijstra_permanent_ptr=NULL,
	};
	struct edge_str *edge_ptr;
	struct edge_str *edge_tmp1;
	struct topology_vport least_cost_vport_header;
	struct topology_vport * vport_adjacent_ptr;
	struct topology_vport *vport_tmp1;
	struct topology_vport *vport_tmp2;
	struct topology_vport *ecmp_vport_ptr;
	int current_least_cost;
	struct edge_str*edge_tmp2;
	struct topology_vport *vport_backward_ptr;
	struct topology_vport *vport_tmp3;
	int update_cost;
	/*int least_upward_count;*/
	
	if(!vport_src ||!vport_dst)
			return -1;/*node not found*/
	/*1.reset the whole topology data struccture*/
	dijstra_reset_vports(vport_src);
	vport_src->is_permanent=TRUE;
	permanent_vport_header.dijstra_permanent_ptr=vport_src;
	
	/*2.iterate,until the last reachable vports found*/
	while(TRUE){
		least_cost_vport_header.dijstra_next_ecmp_ptr=NULL;
		current_least_cost=-1;
		/*2.1 find all the nodes which have the equal cost to src,keep them in the least_cost_vport_header link list*/
		vport_permanent_ptr=permanent_vport_header.dijstra_permanent_ptr;
		puts("");
		while(vport_permanent_ptr){
			
			vport_adjacent_ptr=find_all_adjacent_vports(vport_permanent_ptr);
			
			for(;vport_adjacent_ptr;vport_adjacent_ptr=vport_adjacent_ptr->neighbour_ship_next_vport_ptr){
				
				if(vport_adjacent_ptr->is_permanent)/*already in the permanent set*/
					continue;
				if(vport_adjacent_ptr->cost_to_src==INFINITE_COST)/*unreachablle*/
					continue;
				
				#if 0
				if(current_least_cost==-1){/*first found legal node, link it into the least_cost_vport_header*/
					current_least_cost==vport_adjacent_ptr->cost_to_src;
					vport_adjacent_ptr->dijstra_next_ecmp_ptr=NULL;
					least_cost_vport_header.dijstra_next_ecmp_ptr=vport_adjacent_ptr;
					/*allocate edge to link them together*/
					edge_ptr=alloc_edge_element();
					if(!edge_ptr){/*this may seldomly occur,but here we must handle this memory exception*/
						exit(-1);
					}
					/*the edge-node must be the first*/
					edge_ptr->directed_vport=vport_permanent_ptr;
					vport_adjacent_ptr->dijstra_backward_edge_ptr=edge_ptr;
					
				}else
				#endif
				if(current_least_cost==-1||vport_adjacent_ptr->cost_to_src<current_least_cost){
					/*first we must release the least_cost_vport_header link resource */
					vport_tmp1=least_cost_vport_header.dijstra_next_ecmp_ptr;
					while(vport_tmp1){
						while(vport_tmp1->dijstra_backward_edge_ptr){
							edge_tmp1=vport_tmp1->dijstra_backward_edge_ptr;
							vport_tmp1->dijstra_backward_edge_ptr=vport_tmp1->dijstra_backward_edge_ptr->next;
							dealloc_edge_element(edge_tmp1);
						}
						vport_tmp1=vport_tmp1->dijstra_next_ecmp_ptr;
					}
					/*then we will mount this node*/
					
					current_least_cost=vport_adjacent_ptr->cost_to_src;
					vport_adjacent_ptr->dijstra_next_ecmp_ptr=NULL;
					least_cost_vport_header.dijstra_next_ecmp_ptr=vport_adjacent_ptr;
					edge_ptr=alloc_edge_element();
					if(!edge_ptr){
						exit(-1);
					}
					edge_ptr->directed_vport=vport_permanent_ptr;
					vport_adjacent_ptr->dijstra_backward_edge_ptr=edge_ptr;
					
				} else if(vport_adjacent_ptr->cost_to_src==current_least_cost){
					/*to check whether the upward already in the ECMP list*/
					vport_tmp2=least_cost_vport_header.dijstra_next_ecmp_ptr;
					for(;vport_tmp2;vport_tmp2=vport_tmp2->dijstra_next_ecmp_ptr)
						if(vport_tmp2==vport_adjacent_ptr)
							break;
					if(vport_tmp2){
					}else{/**/
						vport_adjacent_ptr->dijstra_next_ecmp_ptr=least_cost_vport_header.dijstra_next_ecmp_ptr;
						least_cost_vport_header.dijstra_next_ecmp_ptr=vport_adjacent_ptr;
						vport_adjacent_ptr->dijstra_backward_edge_ptr=NULL;
					}
					edge_ptr=alloc_edge_element();
					if(!edge_ptr){
						exit(-1);
					}

					edge_ptr->directed_vport=vport_permanent_ptr;
					edge_ptr->next=vport_adjacent_ptr->dijstra_backward_edge_ptr;
					vport_adjacent_ptr->dijstra_backward_edge_ptr=edge_ptr;
				}
				
			}
			vport_permanent_ptr=vport_permanent_ptr->dijstra_permanent_ptr;
		}
		if(!least_cost_vport_header.dijstra_next_ecmp_ptr)
			break;/*no further steps more*/

		/*2.2 chose the appropriate backward pathes and link them,then release allocated edge struct,then make them permanent*/
		for(ecmp_vport_ptr=least_cost_vport_header.dijstra_next_ecmp_ptr;ecmp_vport_ptr;ecmp_vport_ptr=ecmp_vport_ptr->dijstra_next_ecmp_ptr){
			vport_backward_ptr=NULL;
			for(edge_tmp2=ecmp_vport_ptr->dijstra_backward_edge_ptr;edge_tmp2;edge_tmp2=edge_tmp2->next){
				if(!vport_backward_ptr){
					vport_backward_ptr=edge_tmp2->directed_vport;
				}else if(vport_backward_ptr->upward_vport_count>edge_tmp2->directed_vport->upward_vport_count){
					vport_backward_ptr=edge_tmp2->directed_vport;
				}
			}
			/*found a backward vport with least reference count,then link them*/
			ecmp_vport_ptr->dijstra_backward_ptr=vport_backward_ptr;
			ecmp_vport_ptr->upward_vport_count++;
			ecmp_vport_ptr->is_permanent=TRUE;
			ecmp_vport_ptr->dijstra_permanent_ptr=permanent_vport_header.dijstra_permanent_ptr;
			permanent_vport_header.dijstra_permanent_ptr=ecmp_vport_ptr;
			printf("permanent:...%02x(%d)--->...%02x\n",ecmp_vport_ptr->vport_id[5],ecmp_vport_ptr->cost_to_src,ecmp_vport_ptr->dijstra_backward_ptr->vport_id[5]);
			/*and then release thses edge structs*/
			while(ecmp_vport_ptr->dijstra_backward_edge_ptr){
				edge_tmp2=ecmp_vport_ptr->dijstra_backward_edge_ptr;
				ecmp_vport_ptr->dijstra_backward_edge_ptr=edge_tmp2->next;
				dealloc_edge_element(edge_tmp2);
			}
			
			
		}

		/*2.3 recalculate all the temp nodes' cost to src which are neighbours of newly joint into permanent set */
		for(ecmp_vport_ptr=least_cost_vport_header.dijstra_next_ecmp_ptr;ecmp_vport_ptr;ecmp_vport_ptr=ecmp_vport_ptr->dijstra_next_ecmp_ptr){
			vport_tmp3=find_all_adjacent_vports(ecmp_vport_ptr);
			
			for(;vport_tmp3;vport_tmp3=vport_tmp3->neighbour_ship_next_vport_ptr){
				if(vport_tmp3->is_permanent)
					continue;
				if((update_cost=cost_between_vports(vport_tmp3,ecmp_vport_ptr))>=INFINITE_COST)/*sanity check*/
					continue;
				update_cost+=ecmp_vport_ptr->cost_to_src;
				if(update_cost<vport_tmp3->cost_to_src)
					vport_tmp3->cost_to_src=update_cost;
				
				
			}
		}
		
	}
	

	#if 0
	struct topology_vport * vport_src=index_hash_element(vport_stub,_vport_src,STUB_TYPE_VPORT);
	struct topology_vport * vport_dst=index_hash_element(vport_stub,_vport_dst,STUB_TYPE_VPORT);
	if(!vport_src ||!vport_dst)
		return -1;/*node not found*/
	struct topology_vport *vport_tmp=NULL,*vport_permanent_ptr;
	
	/*1.reset the topology*/
	dijstra_reset_vports(vport_src);
	vport_src->is_permanent=1;
	/*2.iterate :find the neighbour which be shortest ones to the source nodes*/
	struct topology_vport permanent_vport_header={
		.dijstra_permanent_ptr=NULL,
	};
	struct topology_vport least_cost_vport_header;
	int current_least_cost;
	permanent_vport_header.dijstra_permanent_ptr=vport_src;
	
	while(TRUE){
		least_cost_vport_header.dijstra_next_ecmp_ptr=NULL;
		current_least_cost=-1;
		
		vport_permanent_ptr=permanent_vport_header.dijstra_permanent_ptr;
		while(vport_permanent_ptr){
			vport_tmp=find_all_adjacent_vports(vport_permanent_ptr);
			for(;vport_tmp;vport_tmp=vport_tmp->neighbour_ship_next_vport_ptr){
				if(vport_tmp->is_permanent)
					continue;
				if(current_least_cost==-1 ||vport_tmp->cost_to_src<current_least_cost){
					least_cost_vport_header.dijstra_next_ecmp_ptr=vport_tmp;
					current_least_cost=vport_tmp->cost_to_src;
					vport_permanent_ptr->dijstra_next_ecmp_ptr=NULL;
					vport_tmp->dijstra_first_backward_ptr=vport_permanent_ptr;
					
				}else if(vport_tmp->cost_to_src==current_least_cost){
					/*for sanity check that whether the vport  already in ecmp list*/
					struct topology_vport *vport_local_ptr=least_cost_vport_header.dijstra_next_ecmp_ptr;
					for(;vport_local_ptr;vport_local_ptr=vport_local_ptr->dijstra_next_ecmp_ptr)
						if(vport_local_ptr==vport_tmp)
							break;
					if(vport_local_ptr){/*already in the chain*/

					}else{


					}
					
				}
				
			}
			//end of traverse of permanent nodes
			vport_permanent_ptr=vport_permanent_ptr->dijstra_permanent_ptr;
		}
	}
	#endif
	return 0;
}
int cost_between_vports(struct topology_vport *vport1,struct topology_vport *vport2)
{/*current we calculate cost by hop count,
later we may refer to current load and link capability or something else*/
	int cost=INFINITE_COST;
	if(is_the_same_vport(vport1,vport2))
		cost=0;
	else if(is_neighbour(vport1,vport2))
		cost=1;
	else if(is_in_the_same_chassis(vport1,vport2))
		cost=1;
	return cost;
}

 
int main()
{
	topology_init();
	topology_ops_init();
	struct topology_vport vport1;
	struct topology_vport vport2;

	copy_mac_address(vport1.vport_id,"\x12\x12\x12\x12\x12\x12");
	copy_mac_address(vport2.vport_id,"\x12\x12\x12\x12\x12\x13");
	int rc=add_vport_node_pairs_into_topology(vport_stub,domain_stub,device_stub,&device_head,&domain_head,&vport1,0x5555,&vport2,0x5556,0x12);

	copy_mac_address(vport1.vport_id,"\x12\x12\x12\x12\x12\x11");
	copy_mac_address(vport2.vport_id,"\x12\x12\x12\x12\x12\x14");
	rc=add_vport_node_pairs_into_topology(vport_stub,domain_stub,device_stub,&device_head,&domain_head,&vport1,0x5555,&vport2,0x5557,0x13);

	copy_mac_address(vport1.vport_id,"\x12\x12\x12\x12\x12\x16");
	copy_mac_address(vport2.vport_id,"\x12\x12\x12\x12\x12\x13");
	rc=add_vport_node_pairs_into_topology(vport_stub,domain_stub,device_stub,&device_head,&domain_head,&vport1,0x5558,&vport2,0x5556,0x12);

	copy_mac_address(vport1.vport_id,"\x12\x12\x12\x12\x12\x15");
	copy_mac_address(vport2.vport_id,"\x12\x12\x12\x12\x12\x14");
	rc=add_vport_node_pairs_into_topology(vport_stub,domain_stub,device_stub,&device_head,&domain_head,&vport1,0x5558,&vport2,0x5557,0x13);


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
	
	dijstra_find_short_ecmp_path(vport2_ptr,vport4_ptr);
	printf("%d\n",debug_cnt);
	return 0;
}
