    Virtual Networking
              ----meeeow@bjtu.edu.cn
              ----jzheng.bjtu@hotmail.com
      This present document will address the virtual network, originating mainly from NFV infrastructure Overview 
    and network Domain specifications, i.e. there would be some conclusion and my private opinions.
    
     # What is virtual networks?
     So what is the virtual networks? As far as I know ,Even no body or organization give some definition about the terminology ,however  it do exist in our network industry appliance, I could give a overview description of VN according to description instead of its essence, that’s VN is using the common existing networking technology such as Ethernet L2 switching and L3 ip routing to constitute some new types of networks thus overlaying some special network traffic and network business. In common cases, virtual network multiplexes the common infrastructure resource ,on the other hand ,virtual network must isolate network traffic that belongs to different subscribers. So the two obvious features of Virtual networking would be resource multiplexing and resource isolation.
	Virtual networks are broadly deployed in DataCenter, Could networks etc. and the cases all meet requirement of resource pooling, the main characteristics are described as follow:
	1.On-demand self-service
	2.Resource pooling
	3.Rapid elasticity(automated provisioning and rapid scaling)
	And you know ,these features are all the requirement of cloud infrastructure , so I will introduce virtual networking based on cloud computing ,especially OpenStack Neutron and NFV specifications.
	# How Virtual Networks are realized?
	According to NFV Infrastructure Network Domain chapter 6 describes, virtual networks can be divided into two categories ,Infrastructure-based virtual network and layered virtual networks.
	Infrastructure-based virtual network using some logic address space partitioning which can restrict how virtual network traffic will be directed ,typical implement would be L3 access control List like iptables and ebtables,note that this kind of virtual network would not isolation address space ,instead partitioning them . 
	Layered virtual network can be implemented in two ways: virtual partitioning and overlay network.
	Virtual partitioning will divide the infrastructure into several logic link or area to support such network requirements ,which means that the whole infrastructure must participate in the resource pooling and allocation.as an example ,vlan should be this case.one more complex scenario would be flow based forwarding ,the specification would be openflow .etc.
	Virtual networks using overlay technologies may be the most popular way when we need to extending some service from local service domain into another area which utilities some network technology else to build their network. In this we ,we use existing underlay network infrastructure, we add some extra information to the header of a packet when it’s sent to this virtual network ,so the underlay network can be some standard and easily-deployed networking technology, maybe this is the most attractive point when we use virtual networking since the underlay network still can use some industrial standard like STP,SPB and TRILL to build their robust infrastructure.
	And these virtual networking technology can be combined together .
	#Requirement aspects of Virtual Networks?
	Still, these requirements below originate from NFV infrastructure Network Domain specification，Maybe we could refer to these key  point when I do design a virtual network.
	Topology and device Detection.
	As we know ,the virtual network can be quantified in some ways to provide the virtual networking service, and knowing the topology would be the basis before we getting everything started. traditional networks mostly use some distributed networking topology building technology,like BGP,OSPF in L3 area and STP，SFP in L2 area, also there should be a centralized way offering even faster converged and more efficient topology detection and reachability dispatch. I’d prefer the latter method when designing my networks even we need a control &management  plane agents to control and make everything run as expected.
	Address space 
	Virtual networks partitioning and isolation
	We partition some network into different namespaces ,this is needed when we pooling some infrastructure resources ,to do so,we must make sure the underlay network resources can isolate the traffic from different upper layer entities.
	Here we must make it clear the relationship between address space partitioning and address space isolation. Partitioning will not use overlapped addresses, i.e. all addresses should be in the single namespace. Address space isolation cases are we have several namespaces and we could use same address segmentation.
	Traffic engineering and Quality of Service
	Within NFV domain ,traffic engineering and QoS could be integrated into infrastructures and VNF, when we implement these features at infrastructure layer ,it could be as portable as when we implementing these into VNF，what’s more attractive ,these PNF bare-metal method will probably improve performance we could virtualize and pool these PNF.
	Failure detection and redundancy measure
	Here we just consider virtualization layer failure scenarios, at first we must know when we build underlay infrastructure we could utilize some failure and recovery technology like link aggregation and  backup link through STP ,SFP and even ECMP like TRILL switching.
	So when we design our own virtual network, we just consider the layer failure cases ,we just ignore the underlay network, as with virtualization layer failure detection  ,we could design some recovery measures to offer redundant connectivity.


