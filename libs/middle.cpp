#include "middle.h"

void initEvent(myevent *ev, int sockfd, int type, int index, int id){
	ev->sockfd = sockfd;
	ev->id = id;
	ev->filefd = 0;
	ev->events = 0;
	ev->type = type;
	ev->index = index;
	ev->status = 0;
}

void addEvent(int epollfd, int events, myevent *ev){
	struct epoll_event epv = {0, {0}};
	int op;
	epv.data.ptr = ev;
	epv.events = ev->events = events;
	if(ev->status == 1){
		op = EPOLL_CTL_MOD;
	}else{
		op = EPOLL_CTL_ADD;
		ev->status = 1;
	}
	if(epoll_ctl(epollfd, op, ev->sockfd, &epv) < 0){
		printf("Event Add Failed!\n");
	}
}


void delEvent(int epollfd, myevent *ev){
	struct epoll_event epv = {0, {0}};  
    if(ev->status != 1) 
    	return;  
    epv.data.ptr = ev;  
    ev->status = 0;  
    if(epoll_ctl(epollfd, EPOLL_CTL_DEL, ev->sockfd, &epv) < 0){
    	printf("Event Delete Failed!\n");
    }
}

unsigned short in_cksum(unsigned short *addr, int len){       
    register int sum = 0;
    u_short answer = 0;
    register u_short *w = addr;
    register int nleft = len;
    /*
     * Our algorithm is simple, using a 32 bit accumulator (sum), we add
     * sequential 16 bit words to it, and at the end, fold back all the
     * carry bits from the top 16 bits into the lower 16 bits.
     */
    while (nleft > 1)
    {
      sum += *w++;
      nleft -= 2;
    }
    /* mop up an odd byte, if necessary */
    if (nleft == 1)
    {
      *(u_char *) (&answer) = *(u_char *) w;
      sum += answer;
    }
    /* add back carry outs from top 16 bits to low 16 bits */
    sum = (sum >> 16) + (sum & 0xffff);       //add hi 16 to low 16 
    sum += (sum >> 16);                       //add carry 
    answer = ~sum;                            //truncate to 16 bits
    return (answer);
}

int check(char* buffer, int testID, int id){
	if(testID == 0){
		/*
		 *TODO
		 */
		return 3893;
	}
}

