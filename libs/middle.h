/************************************************
 *Header File
 ************************************************/

/************************************************/
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <unistd.h>
/************************************************/
#include <string.h>
#include <stdlib.h>
#include <stdlib.h>
#include <stdio.h>
/************************************************/
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <linux/if_ether.h>
/************************************************/


/************************************************
 * Config of Server and Client Socket
 ***********************************************/
 
#define MAXSOCK             10                 //socket max number
#define MAXEPOLLSIZE        100                //epoll events max number
#define MAXLINE             1024               //length of receive message from socket          
#define LISTENQ             5                  //listen client max number
#define SERVERPORT          "3893"             //listen socket port of server
#define CLIENTPORT 			3893
#define CLIENTNUM           2                  //client numbers
#define SERVERADDR 			"2001:da8:ff3a:c881:db00::"   //server address 
#define CLIENTADDR 			"2001:da8:ff45:a322:7300::"    //client address 
#define HTTPPORT 			80
#define TESTNUM 			1 				   //Test Counts
#define ID 					3893 			   //IPv4 ID

/************************************************
 * Type of Epoll Event
 ***********************************************/
 
#define LISTENFD            2000               //event of sever socket listening
#define SENDNEXT            2001               //event of client send raw packet to next client
#define RECVNEXT            2002               //event of client receive raw packet from last client
#define INIT                2003               //event of server or client initializing
#define CONTROL             2004               //event of server notify client be ready
#define DATA                2005               //event of client upload data to server
#define WAITREADY           2006               //event of server wait client ack ready information
#define WAITRESULT          2007               //event of server wait client to upload data

/***********************************************
 * Event of Epoll
 **********************************************/

struct myevent{
	int sockfd;                                //socket
	int filefd;                      	       //file
	int events;                                //EPOLLOUT|EPOLLET|EPOLLIN
	int index;                                 //Mapped event in the events[]
	int status;                                //EPOLL_CTL_MOD|EPOLL_CTL_ADD
	char* unit;                                //information between different status of epoll event
	int type;                                  //type of epoll event
	int id;                                    //id of 
};

/***********************************************
 * Type of Trans Unit Request
 ***********************************************/
 
#define READY               2008               //server => client           : ready
#define READYOK             2009               //client => server           : be ready Ok
#define START               2010               //server => the first client : start testing

/**********************************************
 * Type of Test
 **********************************************/

#define TYPEICMP	    1
#define TYPETCP		    2
#define TYPEUDP		    3
#define TYPEHTTP	    4
#define IPOPTION 		5
#define TIMESTAMP 		0
#define STRICTROUTE 	1

/***********************************************
 * Translation Unit between server and client
 **********************************************/

struct transUnit{
	bool fromServer;                           //Judge if from the server
	int request;                               //Type of Trans Unit Request
	int type;                                  //Test type
	int testID;                                //The unique test id
	char res[5*128];                          //Result of check the raw packet
};

/***********************************************
 * TCP Ps Header
 **********************************************/

struct tsdhdr{
	unsigned long saddr;
	unsigned long daddr;
	char mbz;
	char protocol;
	unsigned short len;
};

/**********************************************
 * ipv6 header
 **********************************************/

struct ip6hdr{
	__u8 	priority:4,
		version:4;
	__u8	flow_lbl[3];
	__be16	payload_len;
	__u8	nexthdr;
	__u8	hop_limit;

	struct in6_addr saddr;
	struct in6_addr daddr;
};


/**********************************************
 * Time Stamp
 * *******************************************/

struct my_timestamp{
	u_int8_t type;
	u_int8_t len;
	u_int8_t ptr;
	unsigned int flags:4;
	unsigned int overflow:4;
	u_int32_t data[9];
};

/**********************************************
 * Strict Route
 * ********************************************/

struct strict_route{
	u_int8_t type; 
	u_int8_t len;
	u_int8_t ptr;
	unsigned int flags:4;
	unsigned int overflow:4;
	u_int32_t data[9];
};


/***********************************************
 * Generate checksum of ipheader of icmpheader 
 **********************************************/
unsigned short in_cksum(unsigned short *buf, int nwords);

/***********************************************
 * Init epoll event @ev by socket @sockfd, client id @id
 **********************************************/

void initEvent(myevent *ev, int sockfd, int type, int index, int id);

/***********************************************
 * Add or Modfy event in epoll
 **********************************************/
void addEvent(int epollfd, int events, myevent *ev);

/***********************************************
 * Delete epoll event from epoll
 **********************************************/
void delEvent(int epollfd, myevent *ev);

/***********************************************
 * Check the raw packet @buffer by the unique test 
 * id @testID, and client id @id
 **********************************************/
int check(char* buffer, int testID, int id);


