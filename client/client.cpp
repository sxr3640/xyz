#include "../libs/middle.h"

int epollfd;

/*
 * sendsd : raw socket of send raw packet
 * recvsd : raw socket of receive raw packet 
 */
int sendsd;
int recvsd;

/*
 * sendNum : the sum of send raw packet
 * recvNum : the sum of receive raw packet
 */
int sendNum = 0;
int recvNum = 0;

/*
 * testID : the type of test
 * id     : the unique id of client
 * result : the result of check the receiving raw packet
 * over   : if client compelete its work
 */
int ftos = -1;
int testID;
int id;
int type;
int i_type, i_code;
int tcp_data_len = 0, ip_opt_len = 0;
char result[5*128];
bool over = false;

/*
 * Epoll Event
 */
struct myevent myevents[3];
struct epoll_event events[MAXEPOLLSIZE];

/***************************************************
 * Handle EpollIN Event
 ***************************************************/
void recvMessage(struct myevent * ev){
	int sockfd = ev->sockfd;
	ssize_t n = 0;
	char buf[MAXLINE+1];
	memset(buf, 0, sizeof(buf));

	/*Client receive the raw packet*/
	if(ev->type == RECVNEXT && recvNum != TESTNUM){
		char buffer[128];
		struct iphdr* ip;
		struct ip6hdr* ip6;
		char *iphead, *p;
		if((n = recvfrom(sockfd, buffer, 128, 0, NULL, NULL)) < 10){
			printf("Too short\n");
		}else{
			if(ftos == 1){
				iphead = buffer + 14;
				p = iphead + 8;
			}else{
				iphead = buffer;
				p = iphead + 12;
			}
			if(ftos == 1 && p[5] == (char)166 && p[8] == (char)233){
				strncpy(result+recvNum*114, iphead, 114);
				recvNum++;
				printf("IPv6:%d.%d.%d.%d\n",p[5]&0xff, p[6]&0xff, p[7]&0xff, p[8]&0xff);
			}else if(ftos == 0 && p[0] == (char)58 && p[1] == (char)200){
				strncpy(result+recvNum*114, iphead, 114);
				recvNum++;
				ip = (struct iphdr*)iphead;
				printf("IP Header:\n");
				printf("ihl:%d, ver:%d, tos:%d, tot_len:%d\n",ip->ihl, ip->version, ip->tos, ntohs(ip->tot_len));
				printf("id:%d\n", ip->id);
				printf("ttl:%d, pro:%d\n", ip->ttl, ip->protocol); 
				printf("sIPv4:%d.%d.%d.%d\n",p[0]&0xff, p[1]&0xff, p[2]&0xff, p[3]&0xff);
				printf("dIPv4:%d.%d.%d.%d\n",p[4]&0xff, p[5]&0xff, p[6]&0xff, p[7]&0xff);
				if(type == TYPEUDP){
					printf("UDP Header:\n");
					struct udphdr* udp = (struct udphdr*)(iphead + 20);
					printf("sport:%d, dport:%d\n", ntohs(udp->source), ntohs(udp->dest));
					printf("len:%d\n", ntohs(udp->len));
				}else if(type == TYPEHTTP){
					printf("HTTP Header:\n");
					struct tcphdr* tcp = (struct tcphdr*)(iphead + 20);
					printf("sport:%d, dport:%d\n", ntohs(tcp->source), ntohs(tcp->dest));
					printf("Data:%s\n", iphead+52);
				}else if(type == TYPEICMP){
					printf("ICMP Header:\n");
					struct icmphdr* icmp = (struct icmphdr*)(iphead + 20);
					printf("type:%d, code:%d\n", icmp->type, icmp->code);
				}else if(type == TYPETCP){
					printf("TCP Header\n");
					struct tcphdr* tcp = (struct tcphdr*)(iphead + 20);
					printf("sport:%d, dprot:%d\n", ntohs(tcp->source), ntohs(tcp->dest));
				}
			}
				//printf("IPv6:%d.%d.%d.%d\n",p[1]&0xff, p[2]&0xff, p[3]&0xff, p[4]&0xff);
		}
		addEvent(epollfd, EPOLLIN|EPOLLET, ev);
	/*client receive command from server*/
	}else {
		n = read(sockfd, buf, MAXLINE);
		struct transUnit * comm = (struct transUnit *)buf;
		struct transUnit sendUnit;
		if(comm->fromServer){
			switch (comm->request){
				case READY:
					ev->type = CONTROL;
					addEvent(epollfd, EPOLLOUT|EPOLLET, ev);
					printf("Receive server's ready command!\n");
					break;
				case START:
					printf("Receive server's start command!\n");
					ev->type = DATA;
					addEvent(epollfd, EPOLLOUT|EPOLLET, ev);
					initEvent(&myevents[0], sendsd, SENDNEXT, 0, -1);
					addEvent(epollfd, EPOLLOUT|EPOLLET, &myevents[0]);
					break;
			}
		}
    }
}

/***************************************************
 * Handle EpollOUT Event
 ***************************************************/
void sendMessage(struct myevent * ev){
	char buf[MAXLINE+1];
	memset(buf, 0, sizeof(buf));
	int nn = 0;
	int sockfd = ev->sockfd;
	int filefd = ev->filefd;
	struct transUnit sendUnit;
	switch(ev->type){
		/*Send registration information to server*/
		case INIT :
			sendUnit.fromServer = false;
			sendUnit.testID = ev->id;
			ev->unit = (char*)(&sendUnit);
			nn = write(sockfd, ev->unit, sizeof(struct transUnit));
			if(nn < 0){
				printf("Send registration failed\n");
				return ;
			}else
				printf("Send registration successfully\n");
			addEvent(epollfd, EPOLLIN|EPOLLET, ev);
			break;
		/*Upload Data when client complete testing*/
		case DATA :
			if((ev->id == 0 && sendNum == TESTNUM) || (ev->id != 0 && recvNum == TESTNUM)){
				sendUnit.fromServer = false;
				sendUnit.request = WAITRESULT;
				if(ev->id == 0){
					sprintf(sendUnit.res, "0\0");
				}else{
					strncpy(sendUnit.res, result, 5*114);	
					//sprintf(sendUnit.res,"%s,%s,%s,%s,%s\0", result[0], result[1], result[2], result[3], result[4]);
				}
				printf("%s\n",sendUnit.res);
				ev->unit = (char*)(&sendUnit);
				nn = write(sockfd, ev->unit, sizeof(struct transUnit));
				if(nn < 0){
					printf("Can not send\n");
					return ;
				}else if(nn != MAXLINE){
					delEvent(epollfd, ev);
					printf("Send %d Data Successfully\n", nn);
					over = true;
					return;
				}
			}else{
				addEvent(epollfd, EPOLLOUT|EPOLLET, ev);
			}
			break;
		/*Clieng prepare to receive raw packet except the first client*/
		case CONTROL :
			sendUnit.fromServer = false;
			sendUnit.request = READYOK;
			ev->unit = (char*)(&sendUnit);
			nn = write(sockfd, ev->unit, sizeof(struct transUnit));
			if(nn < 0){
				printf("Send Control Message Failed\n");
				return ;
			}
			if(ev->id != 0){
				printf("Begin receive raw socket\n");
				ev->type = DATA;
				addEvent(epollfd, EPOLLOUT|EPOLLET, ev);
				initEvent(&myevents[1], recvsd, RECVNEXT, 0, -1);
				addEvent(epollfd, EPOLLIN|EPOLLET, &myevents[1]);
				return;
			}
			addEvent(epollfd, EPOLLIN|EPOLLET, ev);
			break;
		/*The first Client send raw packet*/
		case SENDNEXT :
			/*IP Heder of All Raw Socket*/
			struct iphdr* ip;
			struct ip6hdr* ip6;

			/*The sockaddr only for the raw socket, except for TYPEHTTP*/
			struct sockaddr_in connection;
			struct sockaddr_in6 connection6;

			char packet_icmp[sizeof(struct iphdr) + sizeof(struct icmphdr) + 100];
		 	char packet_tcp[sizeof(struct iphdr) + sizeof(struct tcphdr) + 1460];
			char packet_udp[sizeof(struct iphdr) + sizeof(struct udphdr) + 1460];
			char packet_tsd[sizeof(struct tsdhdr) + sizeof(struct tcphdr) + 1460];			

			memset(packet_icmp, 0, sizeof(struct iphdr)+sizeof(struct icmphdr) + 100);
			memset(packet_tcp, 0, sizeof(struct iphdr)+sizeof(struct tcphdr) + 1460);
			memset(packet_udp, 0, sizeof(struct iphdr)+sizeof(struct udphdr) + 1460);
			memset(packet_tsd, 0, sizeof(struct tsdhdr)+sizeof(struct tcphdr) + 1460);


			if(type == TYPEICMP || type == IPOPTION){
				ip = (struct iphdr*) packet_icmp;
				ip6 = (struct ip6hdr*) packet_icmp;
			}else if(type == TYPETCP || type == TYPEHTTP){
				ip = (struct iphdr*) packet_tcp;
				ip6 = (struct ip6hdr*) packet_tcp;
			}else if(type == TYPEUDP){
				ip = (struct iphdr*) packet_udp;
				ip6 = (struct ip6hdr*) packet_udp;
			}

			struct icmphdr* icmp;
			int iphdr_len;
			if(ftos == 1){
				iphdr_len = sizeof(struct iphdr);
			}else{
				iphdr_len = sizeof(struct ip6hdr);
			}

			icmp = (struct icmphdr*) (packet_icmp + iphdr_len + ip_opt_len);
		 	struct tcphdr* tcp;
			tcp  = (struct tcphdr*) (packet_tcp + iphdr_len);
			struct udphdr* udp;
			udp  = (struct udphdr*) (packet_udp + iphdr_len);
			struct tsdhdr* tsd;
			tsd  = (struct tsdhdr*) (packet_tsd);
			
			/* Fill The IP Header */
			if(ftos == 1){
				ip->ihl          = 5;
				ip->version      = 4;
				ip->tos          = 0;
				if(type == TYPEICMP || type == IPOPTION){
					ip->protocol = IPPROTO_ICMP;	
				}else if(type == TYPETCP || type == TYPEHTTP){
					ip->tot_len	 = sizeof(struct iphdr) + sizeof(struct tcphdr) + tcp_data_len;
					ip->protocol = IPPROTO_TCP;
				}else if(type == TYPEUDP){
					ip->tot_len  = sizeof(struct iphdr) + sizeof(struct udphdr);
					ip->protocol = IPPROTO_UDP;
				}
				ip->id           = htons(ID);   //will be discard by translation!
				ip->ttl          = 64;
				ip->saddr        = inet_addr(CLIENTADDR);
				ip->daddr        = inet_addr(SERVERADDR);
			}else{
				ip6->version = 6;
				ip6->payload_len = 8;
				if(type == TYPEUDP)
					ip6->nexthdr = 17;
				else if(type == TYPETCP)
					ip6->nexthdr = 6;
				else if(type == TYPEICMP)
					ip6->nexthdr = 1;
				ip6->hop_limit = 64;
				inet_pton(AF_INET6, CLIENTADDR, &(ip6->daddr));
				inet_pton(AF_INET6, SERVERADDR, &(ip6->saddr));	
			}
			/* Fill The ICMP Header*/
			if(type == TYPEICMP){
				icmp->type   = i_type;
				icmp->code   = i_code;
				
				/* Unreachable , stof not complement*/
				if(i_type == 3){
					icmp->un.gateway    	= 0;
					memcpy(packet_icmp + iphdr_len + sizeof(struct icmphdr), packet_icmp, iphdr_len);
					struct iphdr* tmp_iphdr = (struct iphdr *) (packet_icmp + iphdr_len + sizeof(struct icmphdr));
					tmp_iphdr->tot_len 		= htons(sizeof(struct iphdr) + sizeof(struct udphdr));
					tmp_iphdr->protocol 	= IPPROTO_UDP;
					tmp_iphdr->saddr   		= inet_addr(SERVERADDR);
					tmp_iphdr->daddr   		= inet_addr(CLIENTADDR);
					tmp_iphdr->check   		= in_cksum((unsigned short *)tmp_iphdr, sizeof(struct iphdr));

					tsd->saddr				= inet_addr(SERVERADDR);
					tsd->daddr 				= inet_addr(CLIENTADDR);
					tsd->mbz 				= 0;
					tsd->protocol 			= IPPROTO_UDP;
					tsd->len 				= htons(sizeof(struct udphdr));

					struct udphdr* tmp_udphdr = (struct udphdr *)(packet_icmp + 2 * sizeof(struct iphdr) + sizeof(struct icmphdr));
					tmp_udphdr->source 		= htons(atoi(SERVERPORT));
					tmp_udphdr->dest 		= htons(CLIENTPORT);
					tmp_udphdr->len 		= htons(sizeof(struct udphdr));
					
					memcpy(packet_tsd + sizeof(struct tsdhdr), packet_icmp + 2 * sizeof(struct iphdr) + sizeof(struct icmphdr), sizeof(struct udphdr));

					tmp_udphdr->check 		= in_cksum((unsigned short*)packet_tsd, sizeof(struct tsdhdr)+sizeof(struct udphdr));
					ip->tot_len 			= 2 * sizeof(struct iphdr) + sizeof(struct icmphdr) + sizeof(struct udphdr);
					icmp->checksum          = in_cksum((unsigned short *)icmp, sizeof(struct icmphdr) + sizeof(struct iphdr) + sizeof(struct udphdr));
				/* Ping Request */
				}else if(i_type == 8 || i_type == 128){
					ip->tot_len 			= iphdr_len + sizeof(struct icmphdr);
					icmp->checksum  		= in_cksum((unsigned short *)icmp, sizeof(struct icmphdr));
				}

			/* Fill The TCP Header*/
			}else if(type == TYPETCP){
				tcp->source		= htons(CLIENTPORT);
				tcp->dest 		= htons(atoi(SERVERPORT));
				tcp->syn 		= 1;
				tcp->seq 		= htonl(0);
				tcp->doff		= 5;
				tcp->window		= htons(512);
				tcp->urg_ptr	= 0;
				tsd->saddr		= inet_addr(CLIENTADDR);
				tsd->daddr		= inet_addr(SERVERADDR);
				tsd->mbz		= 0;
				tsd->protocol	= IPPROTO_TCP;
				tsd->len		= htons(sizeof(struct tcphdr));
				memcpy(packet_tsd+sizeof(struct tsdhdr), tcp, sizeof(struct tcphdr));
				tcp->check		= in_cksum((unsigned short*)packet_tsd, sizeof(struct tsdhdr)+sizeof(struct tcphdr));
			
			/* Fill The UDP Header */
			}else if(type == TYPEUDP){
				udp->source		= htons(CLIENTPORT);
				udp->dest		= htons(atoi(SERVERPORT));
				udp->len		= htons(sizeof(struct udphdr));
				udp->check		= 0;

			/* Fill IPOption TimeStamp */
			}else if(type == IPOPTION && i_type == TIMESTAMP){
				ip->tot_len     = sizeof(struct iphdr) + sizeof(struct icmphdr) + ip_opt_len;
				struct my_timestamp* ip_time =  (struct my_timestamp *)(packet_icmp + sizeof(struct iphdr));
				ip->ihl 		= 15;
				ip_time->type 	= 68;
				ip_time->len 	= 40;
				ip_time->ptr 	= 5;
				ip_time->flags 	= 0;
				ip_time->overflow = 0;
				icmp->type 		= 8;
				icmp->code 		= 0;
				icmp->checksum 	= in_cksum((unsigned short*)icmp, sizeof(struct icmphdr));
			
			/* Fill IPOption Strict Route*/
			}else if(type == IPOPTION && i_type == STRICTROUTE){
				ip->tot_len 	= sizeof(struct iphdr) + sizeof(struct icmphdr) + ip_opt_len;
				struct strict_route * ip_strict;
				ip_strict 		= (struct strict_route * )(packet_icmp + sizeof(struct iphdr));
				ip->ihl 		= 15;
				ip_strict->type = 137;
				ip_strict->len 	= 40;
				ip_strict->ptr 	= 4;
				icmp->type 		= 8;
				icmp->code 		= 0;
				icmp->checksum 	= in_cksum((unsigned short*)icmp, sizeof(struct icmphdr));
			}

			/* Fill IP Check Sum*/
			//ip->check                = in_cksum((unsigned short *)ip, sizeof(struct iphdr) + ip_opt_len);
		 
			connection.sin_family = AF_INET;
			connection.sin_addr.s_addr = inet_addr(SERVERADDR);
			connection.sin_port = htons(atoi(SERVERPORT));

			connection6.sin6_family = PF_INET6;
			inet_pton(PF_INET6, CLIENTADDR, &(connection6.sin6_addr));
			connection6.sin6_port = htons(CLIENTPORT);

			struct sockaddr * conn;
			if(ftos == 1){
				conn = (struct sockaddr *)&connection;
			}else{
				conn = (struct sockaddr *)&connection6;
			}

			if(sendNum != TESTNUM){
				if(type == TYPEICMP){
					nn = sendto(sockfd, packet_icmp, 28, 0, conn, sizeof(struct sockaddr_in6));
				}else if(type == TYPETCP){
					printf("Do Send\n");
					nn = sendto(sockfd, packet_tcp, 70, 0, (struct sockaddr*)&connection6, sizeof(struct sockaddr_in6));
					printf("Send Complete\n");
				}else if(type == TYPEUDP){
					nn = sendto(sockfd, packet_udp, 0, 0, conn, sizeof(struct sockaddr_in6));
				}else if(type == TYPEHTTP){
				    char request[] = "GET /readme.html HTTP/1.1\r\nHost: 58.200.129.102\r\nUser-Agent: Mozilla/5.0 (X11; Ubuntu; Linux i686; rv:14.0) Gecko/20100101 Firefox/14.0.1\r\nAccept: text/html, application/xhtml+xml,application/xml;1=0.9,*/*;q=0.8\r\nAccept-Lanuage: en-us,en;q=0.5\r\nAccept-Encoding: gzip,deflate\r\nConnection: keep-alive\r\nIf-Modified-Since: Sun, 24 Jun 2012 14:53:51 GMT\r\nIf-None-Match: \"633a8-23d7-4c339089cb86f\"\r\nCache-Control: max-age=0\r\n\r\n";
					if(tcp_data_len == 0)
						nn = write(sockfd, request, strlen(request));
					else{
						char t[1500];
						nn = write(sockfd, t, tcp_data_len);
					}
	
				}else if(type == IPOPTION){
					nn = sendto(sockfd, packet_icmp, ip->tot_len, 0, conn, sizeof(struct sockaddr));
				}
				if(nn < 0){
					perror("sendto() error");
					exit(-1);
				}else{
					printf("%d: raw socket sendto() is OK, length is %d.\n",sendNum, nn);
					sendNum++;
				}
				addEvent(epollfd, EPOLLOUT|EPOLLET, ev);
				return;
			}
			break;
		default:
			break;
	}
}

int main(int argc, char ** argv)
{
	/* Help of Program */
	if(argc < 2 || strcmp("--help", argv[1]) == 0){
		printf("./client <4to6 or 6t04> <IPv6 of Server> <Client ID> <Test Type> [Sub Type -- Option] [Sub Code -- Option]\n");
		printf("Test Type : \n");
		printf("ICMP = 1, TCP = 2, UDP = 3, HTTP = 4, IPOption = 5\n");
		printf("Sub Type for IP Option : \n");
		printf("TimeStamp = 0, Stricted Route = 1\n");
		printf("Sub Type for HTTP\n");
		printf("sub type = tcp data length\n");
		return 0;
	}

	/* Test command is allowable */
	if(argc != 5 && argc != 6 && argc != 7){
		printf("Input illeagl! Please try --Help!");
		return 0;
	}
	
	if(strcmp("4to6", argv[1]) == 0){
		ftos = 1;
	}else if(strcmp("6to4", argv[1]) == 0){
		ftos = 0;
	}else
		return 0;

	/* Parse The Input */
	id   = atoi(argv[3]);	
	type = atoi(argv[4]);
	if(type == TYPEICMP){
 		i_type 		 = atoi(argv[5]);
		i_code 		 = atoi(argv[6]);
	}else if(type == TYPEHTTP){
		tcp_data_len = atoi(argv[5]);
	}else if(type == IPOPTION){
		i_type 		 = atoi(argv[5]);
		ip_opt_len 	 = 40;
	}

	/*Init epollfd and server host information*/ 
	epollfd = epoll_create(MAXEPOLLSIZE);
	if(epollfd < 0){
		printf("Create Epoll Failed!\n");
		return 0;
	}
	
	/*Init the raw socket*/
	int pf_sock_family = 0, af_sock_family = 0;
	if(ftos == 1){
		pf_sock_family = PF_INET;
		af_sock_family = AF_INET;
	}else if(ftos == 0){
		printf("Right\n");
		pf_sock_family = PF_INET6;
		af_sock_family = AF_INET6;
	}
	if(id == 0 && (type == TYPEICMP || type == IPOPTION)){
		if(ftos == 1)
			sendsd = socket(af_sock_family, SOCK_RAW, IPPROTO_ICMP);
		else
			sendsd = socket(af_sock_family, SOCK_RAW, IPPROTO_ICMPV6);
	}else if(id == 0 && (type == TYPETCP)){
		if(ftos == 1)	
			sendsd = socket(pf_sock_family, SOCK_RAW, IPPROTO_TCP);
		else
			sendsd = socket(pf_sock_family, SOCK_STREAM, 0);
	}else if(id == 0 && (type == TYPEUDP)){
		if(ftos == 1)
			sendsd = socket(pf_sock_family, SOCK_RAW, IPPROTO_UDP);
		else
			sendsd = socket(pf_sock_family, SOCK_DGRAM, 0);
	}else if(id == 0 && (type == TYPEHTTP)){
		sendsd = socket(af_sock_family, SOCK_STREAM, 0);
		struct sockaddr_in servaddr;
		memset(&servaddr, 0, sizeof(servaddr));
		servaddr.sin_family 	 = AF_INET;
		servaddr.sin_port 		 = htons(HTTPPORT);
		servaddr.sin_addr.s_addr = inet_addr(SERVERADDR);

		struct sockaddr_in6 servaddr6;
		memset(&servaddr6, 0, sizeof(struct sockaddr_in6));
		servaddr6.sin6_family = AF_INET6;
		servaddr6.sin6_port    = htons(HTTPPORT);
		inet_pton(AF_INET6, CLIENTADDR, &(servaddr6.sin6_addr));

		if(ftos == 1 && connect(sendsd, (sockaddr *)&servaddr, sizeof(servaddr)) < 0){
			printf("Connect server failed\n");
			return 0;
		}else if(ftos == 0 && connect(sendsd, (sockaddr *)&servaddr6, sizeof(servaddr6)) < 0){
			printf("Connect server failed\n");
			return 0;
		}else
			printf("Test Http Connect server successfully\n");
		
		int opt = 1;
		setsockopt(sendsd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
		if(fcntl(sendsd, F_SETFL, fcntl(sendsd, F_GETFD, 0)|O_NONBLOCK) == -1){
			printf("Set socket option failed\n");
			return 0;
		}
	}

	if(sendsd < 0){
		perror("raw socket() error");
		return 0;
	}else
		printf("create raw socket successfully\n");
	int one = 1;
	const int *val = &one;
	if(id == 0 && type != TYPEHTTP && ftos == 1 && setsockopt(sendsd, IPPROTO_IP, IP_HDRINCL, val, sizeof(one)) < 0){
		perror("setsockopt() error");
		exit(-1);
	}else if(id == 0 && type != TYPEHTTP && ftos == 0 && setsockopt(sendsd, IPPROTO_IPV6, IP_HDRINCL, val, sizeof(one)) < 0){
		perror("setsockopt() error");
		exit(-1);
	}else{
		perror("setsockopt() error");
		printf("setsockopt() is OK.\n");
	}
	if(ftos == 1 && (recvsd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IPV6))) < 0){
		perror("raw socket() error");
		return 0;
	}else if(ftos == 0 && (recvsd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP))) < 0){
		perror("raw socket() error");
		return 0;
	}else
		printf("create raw socket successfully\n");
		
	/*Connect to Server*/
	/*
	struct addrinfo Hints, *AddrInfo, *AI;
	memset(&Hints, 0, sizeof(Hints));
	Hints.ai_family = AF_UNSPEC;
	Hints.ai_socktype = SOCK_STREAM;
	if(getaddrinfo(argv[1], SERVERPORT,&Hints, &AddrInfo))
		printf("GetAddrinfo Wrong\n");
	int sockfd;
	for(AI=AddrInfo; AI!=NULL; AI=AI->ai_next){
		if((sockfd = socket(AI->ai_family, AI->ai_socktype, AI->ai_protocol)) < 0)
			continue;
		if(connect(sockfd, AI->ai_addr, AI->ai_addrlen >= 0)){
			printf("Connect Server Failed\n");
			break;
		}
	}
	freeaddrinfo(AddrInfo);
	*/
	int sockfd = socket(af_sock_family, SOCK_STREAM, 0);
	
		struct sockaddr_in6 local_addr6; 
		local_addr6.sin6_family = AF_INET6;
		local_addr6.sin6_port = htons(3893);
		inet_pton(AF_INET6, argv[2], &local_addr6.sin6_addr);
		if(connect(sockfd, (sockaddr *)&local_addr6, sizeof(local_addr6)) != 0){
			printf("Connect Failed\n");
			return 0;
		}
	/*else{
		struct sockaddr_in servaddr;
		memset(&servaddr, 0, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_port = htons(atoi(SERVERPORT));
		inet_pton(AF_INET, argv[1],  &servaddr.sin_addr);
		if(connect(sockfd, (sockaddr *)&servaddr, sizeof(servaddr)) < 0){
			printf("Connect server failed\n");
			return 0;
		}else
			printf("Connect server successfully\n");
	}
	*/	
	int opt = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if(fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0)|O_NONBLOCK) == -1){
		printf("Set socket option failed\n");
		return 0;
	}
	initEvent(&myevents[2], sockfd, INIT, 0, id);
	addEvent(epollfd, EPOLLOUT|EPOLLET, &myevents[2]);
	

	/*Do with socket handle*/
	int fds = 0;
	struct sockaddr_in cliaddr;
	socklen_t clilen;
	while(!over){
		fds = epoll_wait(epollfd, events, MAXEPOLLSIZE, -1);
        if (fds == -1)
        {
            printf("Warning : epoll wait fd number is -1");
            continue;
        }

        for (int i=0; i<fds; i++)
        {
        	myevent *ev = (struct myevent *)events[i].data.ptr;
            if(ev->events&EPOLLIN){
            	recvMessage(ev);
            }else if(ev->events&EPOLLOUT){
            	sendMessage(ev);
            }
        }
	}

	return 0;
}
