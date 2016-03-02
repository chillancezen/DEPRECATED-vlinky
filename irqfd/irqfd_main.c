#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <signal.h>
#include <sys/epoll.h>
#include <errno.h>
#include <assert.h>
#define MSI_X_SERVER_PATH "/tmp/msi_x_server.sock"
#define MSI_X_MAX_LINK 256
#define MSI_X_MAX_QUEUE 64

/*messgae transaction format
link-num  | channel-num |dpdk&qemu-context(0|1)|reserved
*/
struct fd_desc{
	int dpdk_fd[MSI_X_MAX_QUEUE];
	int qemu_fd[MSI_X_MAX_QUEUE];
};


int main()
{
	int rc;int idx,idx_tmp;
	int fd_client;
	
	int fd=socket(AF_UNIX,SOCK_STREAM,0);
	struct sockaddr_un un;
	struct sockaddr_un un_client;
	struct fd_desc fd_desc_arr[MSI_X_MAX_LINK];
	int link_num;
	int channel_num;
	int context;
	
	int un_len;
	char buffer[4];
	for(idx=0;idx<MSI_X_MAX_LINK;idx++){
		for(idx_tmp=0;idx_tmp<MSI_X_MAX_QUEUE;idx_tmp++){
			fd_desc_arr[idx].dpdk_fd[idx_tmp]=-1;
			fd_desc_arr[idx].qemu_fd[idx_tmp]=-1;
		}
	}
	unlink(MSI_X_SERVER_PATH);
	memset(&un,0x0,sizeof(un));
	un.sun_family=AF_UNIX;
	strcpy(un.sun_path,MSI_X_SERVER_PATH);
	assert(fd>=0);
	rc=bind(fd,(struct sockaddr*)&un,sizeof(struct sockaddr_un));
	assert(fd>=0);
	rc=listen(fd,5);
	assert(fd>=0);
	struct epoll_event ev,events[256];
	int epfd=epoll_create(256);
	int nfds;
	ev.data.fd=fd;
	ev.events=EPOLLIN;
	epoll_ctl(epfd,EPOLL_CTL_ADD,fd,&ev);
	while(1){
		nfds=epoll_wait(epfd,events,256,100);
		for(idx=0;idx<nfds;idx++){
			if(events[idx].data.fd==fd){
				memset(&un_client,0x0,sizeof(un_client));
				un_len=sizeof(struct sockaddr_un);
				fd_client=accept(fd,(struct sockaddr*)&un_client,&un_len);
				ev.data.fd=fd_client;
				ev.events=EPOLLIN|EPOLLET;
				epoll_ctl(epfd,EPOLL_CTL_ADD,fd_client,&ev);
			}else if (events[idx].events&EPOLLIN){
				if(events[idx].data.fd==-1)
					continue;
				rc=read(events[idx].data.fd,buffer,4);
				if(rc<=0){
					puts("meeeow");
					close(events[idx].data.fd);
					events[idx].data.fd=-1;
					continue;
					
				}
				link_num=buffer[0];
				channel_num=buffer[1];
				context=buffer[2];
				
				switch(context)
				{
					case 0:/*dpdk*/
						fd_desc_arr[link_num].dpdk_fd[channel_num]=events[idx].data.fd;
					//	printf("dpdk %d %d %d \n",link_num,channel_num,fd_desc_arr[link_num].dpdk_fd[channel_num]);
						/*yet send msi-x request to the qemu  irq peer*/
						
						if(fd_desc_arr[link_num].qemu_fd[channel_num]!=-1){
							//puts("flag");
							write(fd_desc_arr[link_num].qemu_fd[channel_num],buffer,4);
							}
						
						break;
					case 1:/*qemu*/
						fd_desc_arr[link_num].qemu_fd[channel_num]=events[idx].data.fd;
					//	printf("qemu %d %d \n",link_num,channel_num);
						break;
				}
			}
		}
	}
	
	return 0;
}

