/*
Name: Haolai Che
Case-ID: hxc859
file-Name: proj4.c
Date-Created: Oct/25/2022
*/
/*
Description:
proj4.c implements a packet tracer, which analyzes trace file
via Summary, Length, Packet Printing, Traffic Matrix Mode. i.e.
-s, -l, -p, -m Modes.
-t is a mandatory option, and argument to -t is the name of the 
trace file.
*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include "hashMap.h"

#define ERROR 1
#define BUFSIZE 4096
#define MAX_PKT_SIZE 1600
#define CAPLENMIN 34
#define MAX_TOTAL_LEN 65535
#define tcpseq 6
#define udpseq 17
#define udpheaderlen 8
#define prototimes 4



extern int optind, opterr, optopt;
extern char* optarg;
// int ip_len;
int iphl;
int ipprotocol;
int transportlen;
int payload_len;
unsigned int flag_t = 0; //keep track of argument t
unsigned int flag_s = 0; //keep track of argument s, summary mode
unsigned int flag_l = 0; //keep track of argument l, length analysis mode
unsigned int flag_p = 0; //keep track of argument p, packet printing mode
unsigned int flag_m = 0; //keep track of argument m, traffic matrix mode
unsigned int flag_tcpinclude = 0; //keep track of if there is transport header
unsigned int flag_udpinclude = 0;
unsigned int flag_IPinclude = 0;
unsigned int flag_IPnotinclude = 0;
unsigned int flag_transnotinclude = 0;
char* pckName; //the packet trace file name

/* meta information, using same layout as trace file */
struct METAINFO
{
    unsigned short caplen; // 2 bytes
    unsigned short ignored;// 2 bytes
    unsigned int secs; // 4 bytes
    unsigned int usecs; // 4 bytes
};

/* record of information about the current packet */
struct PKT_INFO
{
    unsigned short caplen;      /* from meta info */
    double now;                 /* from meta info */
    unsigned char pkt [MAX_PKT_SIZE];
    struct ether_header* ethh;  /* ptr to ethernet header, if fully present,
                                   otherwise NULL */
    struct ip* iph;          /* ptr to IP header, if fully present, 
                                   otherwise NULL */
    struct tcphdr* tcph;        /* ptr to TCP header, if fully present,
                                   otherwise NULL */
    struct udphdr* udph;        /* ptr to UDP header, if fully present,
                                   otherwise NULL */
};


int errexit(char* format, char* arg)
{
	fprintf(stderr,format, arg);
	fprintf(stderr,"\n");
	exit(ERROR);
}

unsigned int hash(char* str)
{
	register unsigned int h;
	register unsigned char* p;

	for(h=0,p = (unsigned char*)str; *p;p++)
	{
		h = 31*h + *p;
	}
	return h;
}

HashMap* CreateHashMap(int n)
{
	HashMap* hashMap = (HashMap*)calloc(1, sizeof(HashMap));
	hashMap->hashArr = (HashNode**)calloc(n, sizeof(HashNode*));
	if (hashMap==NULL || hashMap->hashArr==NULL) {
        return NULL;
        errexit("Calloc Fail!\r\n",NULL);
    }
    hashMap->size = n;
    return hashMap;
}

int InsertHashMap(HashMap* hashMap, char* key, long int value)
{
	//create a node
	int index;
	HashNode* node = (HashNode*)calloc(1, sizeof(HashNode));
	if(node == NULL)
	{
		return 0;
		errexit("Calloc Fail\r\n",NULL);
	}
	node->key = strdup(key);
	node->value = value;
	//node->p = &value;
	node->next = NULL;
	index = hash(key) % hashMap -> size;

	if(hashMap->hashArr[index] == NULL)
	{
		hashMap->hashArr[index] = node;
	}
	else
	{
		HashNode* temp = hashMap->hashArr[index];
		HashNode* prev = temp;
		while(temp != NULL)
		{
			if(strcmp(temp->key, node->key) == 0)
			{
				temp->value += node->value;
				return 1;
			}
			prev = temp;
			temp = temp->next;
		}
		prev->next = node;
	}
	return 1;

}

int GetHashMap(HashMap* hashMap, char* key)
{
	int index = hash(key) % hashMap -> size;
	HashNode* temp = hashMap->hashArr[index];
	while(temp != NULL)
	{
		if(strcmp(temp->key, key) == 0)
		{
			return temp->value;
		}
		
	}
	return 0;

}

void DeleteHashMap(HashMap* hashMap)
{
	for(int i = 0; i < hashMap->size; i++)
	{
		HashNode* temp = hashMap->hashArr[i];
		HashNode* prev = temp;
		while(temp != NULL)
		{
			prev = temp;
			temp = temp->next;
			free(prev->key);
			//free(prev->p);
			free(prev);
		}
	}
	free(hashMap->hashArr);
	free(hashMap);
	hashMap->hashArr = NULL;
	hashMap = NULL;
}


void PrintHashMap(HashMap* hashMap)
{
    //printf("===========PrintHashMap==========\r\n");
    HashNode* node = NULL;
    const char x[] = "X";
    char* token;
    char* token_x;
    for (int i = 0; i < hashMap->size; i++)
    {
        node = hashMap->hashArr[i];
        if (node != NULL) 
        {
            HashNode* temp = node;
            
            while (temp != NULL) 
            {
            	token = strtok(temp->key,x);
            	token_x = strtok(NULL, x);
                printf("%s %s %ld\n", token, token_x, temp->value);
                temp = temp->next;
            }

        }

    }
}

/*
	fd: an open file to read packets from 
	while(next_packet)
	pinfo: allocated memory to put packet info into for one packet
	returns: 0 || 1
	1: a packet was read and pinfo is set up for processing the packet
	0: we have reach the end of the file and no packet is available
*/

unsigned short next_packet(int fd, struct PKT_INFO* pinfo)
{
	//int timesize = 100;
	struct METAINFO meta;
	int bytes_read;
	//char time_char[timesize];
	long seconds;
	long microseconds;
	long total_microseconds;
	double total_seconds;
	int timesby = 1000000;

	memset(pinfo, 0x0, sizeof(struct PKT_INFO));
	memset(&meta, 0x0, sizeof(struct METAINFO));

	/*read meta info*/
	bytes_read = read(fd, &meta, sizeof(meta));
	if(bytes_read == 0)
	{
		return (0);
	}
	if(bytes_read < sizeof(meta))
	{
		errexit("Cannot Read meta Information!\r\n",NULL);
	}
	pinfo->caplen = ntohs(meta.caplen);
	if(pinfo->caplen == 0)
	{
		return (1);
	}
	if(pinfo->caplen > MAX_PKT_SIZE)
	{
		errexit("Packet too big!\r\n",NULL);
	}

	//pinfo->now based on meta.secs and meta.usecs
	// sprintf(time_char, "%d.%d", ntohl(meta.secs), ntohl(meta.usecs));
	// sscanf(time_char, "%lf", &pinfo->now);
	seconds = ntohl(meta.secs);
	microseconds = ntohl(meta.usecs);
	total_microseconds = seconds * timesby + microseconds;
	total_seconds = (double)total_microseconds/timesby;
	pinfo->now = total_seconds;



	//read the packet information
	bytes_read = read(fd, pinfo->pkt, pinfo->caplen);
	if(bytes_read < 0)
	{
		errexit("Error Reading Packets!\r\n",NULL);
	}
	if(bytes_read < pinfo->caplen)
	{
		errexit("unexpected end of file encountered!\r\n",NULL);
	}
	if(bytes_read < sizeof(struct ether_header))
	{
		return (1);
		//Etherheader not complete
	}
	pinfo->ethh = (struct ether_header*)pinfo->pkt;
	//pinfo->pkt is the pointer points to the head of the packet part
	pinfo->ethh->ether_type = ntohs(pinfo->ethh->ether_type);
	if(pinfo->ethh->ether_type != ETHERTYPE_IP)
	{
		//ignore a packet if it's not IP packet
		return (1);
	}

	if(pinfo->caplen == sizeof(struct ether_header))
	{
		flag_IPnotinclude = 1;
		return (1);
	}

	//go after the ether header if any
	if(pinfo->caplen >= CAPLENMIN && pinfo->ethh->ether_type == ETHERTYPE_IP)
	{
		//CAPLENMIN is 34
		flag_IPinclude = 1;
		//if the caplen is at least 34 bytes long
		//and if the ether_type is IPV4
		/*means we have more than just ethernet header to process*/
		pinfo->iph = (struct ip*)(pinfo->pkt + sizeof(struct ether_header));
		/*Deal with IP headers if any
		  after IP headers if TCP
		  	set pinfo->tcph to the start of the TCP header

		  if UDP
		  	set pinfo->udph to the start of the UDP header
		*/
		pinfo->iph->ip_len = ntohs(pinfo->iph->ip_len);
		if(pinfo->iph->ip_len > MAX_TOTAL_LEN)
		{
			//unlikely to happen but just in case
			errexit("MAL IP PACKET!\r\n",NULL);
		}

		iphl = pinfo->iph->ip_hl * prototimes;
		ipprotocol = pinfo->iph->ip_p;

		
		// tcp : 6
		// udp :17
		if(pinfo->caplen == sizeof(struct ether_header) + iphl)
		{
			flag_transnotinclude = 1;
			return (1);
		}

		if(pinfo->caplen > sizeof(struct ether_header) + iphl)
		{
			//if TCP:
			if(ipprotocol == tcpseq)
			{
				flag_tcpinclude = 1;
				pinfo->tcph = (struct tcphdr*)(pinfo->pkt + sizeof(struct ether_header) + iphl);
				//the start of tcp header
				transportlen = pinfo->tcph->th_off;
				
				//linux :th_off -> doff

				transportlen *= 4;

			}
			//if UDP:
			if(ipprotocol == udpseq)
			{
				flag_udpinclude = 1;
				pinfo->udph = (struct udphdr*)(pinfo->pkt + sizeof(struct ether_header) + iphl);
				transportlen = udpheaderlen;
			}

			payload_len = pinfo->iph->ip_len - transportlen - iphl;

		}

		

	}

	return (1);

	

}


int main(int argc, char* argv[])
{

	int f_d;

	int c = 0;
	while(EOF != (c = getopt(argc, argv, "t:slpm")))
	{
		//printf("start to process %d para\n", optind);
		switch(c)
		{
			case 't':
				//must present
				//the name of the trace file
				break;
			case 's':
				//summary mode
				break;
			case 'l':
				//length mode
				break;
			case 'p':
				//IPv4 / TCP packet printing
				break;
			case 'm':	
				//traffic matric mode
				break;
			case '?':
				errexit("Unknown Argument Specified!\r\n",NULL);
				break;
			default:
				break;
		
		}
	
	}


	if(argc == 1)
	{
		printf("Programname is :%s\r\n",argv[0]);
		printf("No extra commandline arguments Passed other than Programname\r\n");
	}
	
	if(argc >= 2)
	{
		for(int i = 0; i < argc; i++)
		{
			if(!strcmp(argv[i], "-t"))
			{
				flag_t = 1;
				pckName = argv[i+1];

			}


			if(!strcmp(argv[i], "-s"))
			{
				flag_s = 1;
				
			}


			if(!strcmp(argv[i], "-l"))
			{
				flag_l = 1;
				
			}


			if(!strcmp(argv[i], "-p"))
			{
				flag_p = 1;

			}

			if(!strcmp(argv[i], "-m"))
			{
				flag_m = 1;
				
			}

		}

	}

	if(flag_t == 0)
	{
		errexit("The -t option is mandatory!\r\n", NULL);
	}

	else if(flag_t == 1)
	{
		f_d = open(pckName, O_RDWR);
		if(f_d == -1)
		{
			errexit("Fail to open File!\r\n",NULL);
		}

		if(flag_s == 0 && flag_l == 0 && flag_p == 0 && flag_m == 0)
		{
			errexit("No Mode Specified!\r\n",NULL);
		}

		if(flag_s == 1)
		{
			//enter Summary Mode
			int i = 0;
			int ip_pckts = 0;
			double time_slot[BUFSIZE] = {0};
			double zero;
			if(flag_l == 1 || flag_p == 1 || flag_m == 1)
			{
				errexit("Multiple Modes Selected!\r\n",NULL);
			}

			struct PKT_INFO p_info;
			while(next_packet(f_d,&p_info))
			{
				if(i == 0){
					time_slot[i] = p_info.now;
					zero = time_slot[i];
				}
				//printf("time:%lf",p_info.now);
				time_slot[i%BUFSIZE] = p_info.now;
				i++;
				if(p_info.ethh->ether_type == ETHERTYPE_IP)
				{
					ip_pckts++;
				}
			}
			printf("FIRST PKT: %lf\n",zero);
			printf("LAST PKT: %lf\n",time_slot[(i-1)%BUFSIZE]);
			printf("TOTAL PACKETS: %d\n", i);
			printf("IP PACKETS: %d\n", ip_pckts);
			close(f_d);
		}

		if(flag_l == 1)
		{
			if(flag_s == 1 || flag_p == 1 || flag_m == 1)
			{
				errexit("Multiple Modes Selected!\r\n",NULL);
			}
			//enter Length Analysis Mode
			struct PKT_INFO p_info;
			int i = 0;
			double time_slot[BUFSIZE] = {0};
			int caplen_slot[BUFSIZE] = {0};
			int ip_len_slot[BUFSIZE] = {0};
			char ip_len_x[BUFSIZE] = {0};
			int iphl_slot[BUFSIZE] = {0};
			char iphl_x[BUFSIZE] = {0};
			char protocol_x[BUFSIZE] = {0};
			int transheaderlen[BUFSIZE] = {0};
			char transportheader_x[BUFSIZE] = {0};
			int payloadlen[BUFSIZE] = {0};
			char payload_x[BUFSIZE] = {0};
			while(next_packet(f_d,&p_info))
			{
				if(p_info.ethh->ether_type == ETHERTYPE_IP)
				{
					
					if(flag_IPinclude == 1)
					{
						time_slot[i] = p_info.now;
						caplen_slot[i] = p_info.caplen;
						ip_len_slot[i] = p_info.iph->ip_len;
						iphl_slot[i] = iphl;
						if(ipprotocol == tcpseq){
							protocol_x[i] = 'T';
							if(flag_tcpinclude == 1 && flag_transnotinclude == 0)
							{
								transheaderlen[i] = transportlen;
								payloadlen[i] = payload_len;
								printf("%lf %d %d %d %c %d %d\n",time_slot[i],
								caplen_slot[i],ip_len_slot[i],iphl_slot[i],
								protocol_x[i], transheaderlen[i],payloadlen[i]);

							}
							if(flag_transnotinclude == 1)
							{
								transportheader_x[i] = '-';
								payload_x[i] = '-';
								printf("%lf %d %d %d %c %c %c\n",time_slot[i],
								caplen_slot[i],ip_len_slot[i],iphl_slot[i],
								protocol_x[i], transportheader_x[i],payload_x[i]);
							}

							
						}
						if(ipprotocol == udpseq){
							protocol_x[i] = 'U';
							if(flag_udpinclude == 1 && flag_transnotinclude == 0)
							{
								transheaderlen[i] = transportlen;
								payloadlen[i] = payload_len;
								printf("%lf %d %d %d %c %d %d\n",time_slot[i],
								caplen_slot[i],ip_len_slot[i],iphl_slot[i],
								protocol_x[i], transheaderlen[i],payloadlen[i]);

							}
							if(flag_transnotinclude == 1)
							{
								transportheader_x[i] = '-';
								payload_x[i] = '-';
								printf("%lf %d %d %d %c %c %c\n",time_slot[i],
								caplen_slot[i],ip_len_slot[i],iphl_slot[i],
								protocol_x[i], transportheader_x[i],payload_x[i]);
							}
						}

						if(ipprotocol != tcpseq && ipprotocol != udpseq)
						{
							protocol_x[i] = '?';
							transportheader_x[i] = '?';
							payload_x[i] = '?';
							printf("%lf %d %d %d %c %c %c\n",time_slot[i],
								caplen_slot[i],ip_len_slot[i],iphl_slot[i],
								protocol_x[i], transportheader_x[i],payload_x[i]);
						}

					}

					else if(flag_IPnotinclude == 1 )
					{
						time_slot[i] = p_info.now;
						caplen_slot[i] = p_info.caplen;
						ip_len_x[i] = '-';
						iphl_x[i] = '-';
						protocol_x[i] = '-';
						transportheader_x[i] = '-'; 
						payload_x[i] = '-';
						printf("%lf %d %c %c %c %c %c\n",time_slot[i],
							caplen_slot[i],ip_len_x[i],iphl_x[i],
							protocol_x[i],transportheader_x[i],payload_x[i]);
						
					}
					//At the end of loop put all flags back to 0
					//Prep for the next loop
					//**Critical step**
					flag_IPinclude = 0;
					flag_IPnotinclude = 0;
					flag_transnotinclude = 0;
					flag_tcpinclude = 0;
					flag_udpinclude = 0;

					
				}

				

			}
			close(f_d);


		}

		if(flag_p == 1)
		{
			if(flag_l == 1 || flag_s == 1 || flag_m == 1)
			{
				errexit("Multiple Modes Selected!\r\n",NULL);
			}
			//enter Packet Printing Mode
			struct PKT_INFO p_info;
			int i = 0;
			while(next_packet(f_d,&p_info))
			{
				double time_slot[BUFSIZE] = {0};
				char* IPsrc_slot[BUFSIZE] = {0};
				char* IPdest_slot[BUFSIZE] = {0};
				char src[BUFSIZE] = {0};
				char dst[BUFSIZE] = {0};
				int ipttl_slot[BUFSIZE] = {0};
				int tcpsrcport_slot[BUFSIZE] = {0};
				int tcpdstport_slot[BUFSIZE] = {0};
				int tcpwindow_slot[BUFSIZE] = {0};
				long tcpseqno_slot[BUFSIZE] = {0};
				long tcpackno_slot[BUFSIZE] = {0};
				char tcpack_X[BUFSIZE] = {0};

				if(p_info.ethh->ether_type == ETHERTYPE_IP &&
				flag_IPinclude == 1 && 
				ipprotocol == tcpseq && flag_tcpinclude == 1)
				{
					time_slot[i] = p_info.now;
					strcpy(src, inet_ntoa(p_info.iph->ip_src));
					strcpy(dst, inet_ntoa(p_info.iph->ip_dst));
					IPsrc_slot[i] = src;
					IPdest_slot[i] = dst;
					ipttl_slot[i] = p_info.iph->ip_ttl;
					tcpsrcport_slot[i] = ntohs(p_info.tcph->th_sport);
					tcpdstport_slot[i] = ntohs(p_info.tcph->th_dport);
					tcpwindow_slot[i] = ntohs(p_info.tcph->th_win);
					tcpseqno_slot[i] = ntohl(p_info.tcph->th_seq);
					if(p_info.tcph->th_flags & TH_ACK)
					{
						tcpackno_slot[i] = ntohl(p_info.tcph->th_ack);
						printf("%lf %s %s %d %d %d %d %ld %ld\n",time_slot[i], IPsrc_slot[i], 
							IPdest_slot[i],ipttl_slot[i],tcpsrcport_slot[i], 
							tcpdstport_slot[i],tcpwindow_slot[i],tcpseqno_slot[i],tcpackno_slot[i]);
					}
					else
					{
						tcpack_X[i] = '-';
						printf("%lf %s %s %d %d %d %d %ld %c\n",time_slot[i], IPsrc_slot[i], 
							IPdest_slot[i],ipttl_slot[i],tcpsrcport_slot[i], 
							tcpdstport_slot[i],tcpwindow_slot[i],tcpseqno_slot[i],tcpack_X[i]);
					}



				}
				flag_IPinclude = 0;
				flag_IPnotinclude = 0;
				flag_transnotinclude = 0;
				flag_tcpinclude = 0;
				flag_udpinclude = 0;
				

			}
			close(f_d);

		}



		if(flag_m == 1)
		{
			if(flag_l == 1 || flag_p == 1 || flag_s == 1)
			{
				errexit("Multiple Modes Selected!\r\n",NULL);
			}
			//enter Traffix Matrix Mode
			struct PKT_INFO p_info;
			int i = 0;
			HashMap* hashMap = CreateHashMap(BUFSIZE);
			if(!hashMap)
			{
				errexit("Create UnSuccess!\r\n",NULL);
			}
			while(next_packet(f_d,&p_info))
			{
				char* IPsrc_slot[BUFSIZE] = {0};
				char* IPdest_slot[BUFSIZE] = {0};
				char src[BUFSIZE] = {0};
				char dst[BUFSIZE] = {0};
				char IPPair[BUFSIZE] = {0};
				int payloadlen[BUFSIZE] = {0};
			
				if(p_info.ethh->ether_type == ETHERTYPE_IP &&
				flag_IPinclude == 1 && 
				ipprotocol == tcpseq && flag_tcpinclude == 1)
				{
					strcpy(src, inet_ntoa(p_info.iph->ip_src));
					strcpy(dst, inet_ntoa(p_info.iph->ip_dst));
					IPsrc_slot[i] = src;
					IPdest_slot[i] = dst;
					payloadlen[i] = payload_len;
					strcpy(IPPair, IPsrc_slot[i]);
					strcat(IPPair,"X");
					strcat(IPPair,IPdest_slot[i]);
					//printf("%s\r\n",IPPair);
					InsertHashMap(hashMap,IPPair,payloadlen[i]);



				}
				flag_IPinclude = 0;
				flag_IPnotinclude = 0;
				flag_transnotinclude = 0;
				flag_tcpinclude = 0;
				flag_udpinclude = 0;
				
			}
			PrintHashMap(hashMap);
			DeleteHashMap(hashMap);
			close(f_d);

		}


	}


	

}