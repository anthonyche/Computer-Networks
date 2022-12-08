/*
Name: Haolai Che
Case-ID: hxc859
file-Name: URLparse.h
Date-Created: Sep/12/2022
*/
/*
URLparse defines all the functions that could be used to 
parse the url we pass in the proj2.c, supporting HTTP protocol
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>			
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#define _URL_DEBUG 0
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define HEAD_HTTP_P "http://"
#define PORT_HTTP 80
#define URL_ERROR -1
#define URL_OK 0
#define MAX_COMM_NAME_LEN 128
#define MAX_URL_LEN 1024
#define INET_ADDRSTRLEN 16
#define INET_DOMAINSTRLEN 128
#define MAX_PORT_LEN 6
#define MAX_PATH_FILE_LEN 256
#define MAX_IP_STR_LEN 32
#define MAX_USER_LEN 32
#define MAX_PASS_LEN 32

struct pro_port{
	char pro_s[32];
	unsigned short port;
};

struct pro_port g_pro_port[]={	
	{HEAD_HTTP_P,PORT_HTTP},	
};

typedef struct
{
	char user[MAX_USER_LEN];
	char pass[MAX_PASS_LEN];
	char domain[INET_DOMAINSTRLEN];//domain
	char svr_dir[MAX_PATH_FILE_LEN]; //directory
	char svr_ip[MAX_IP_STR_LEN];
	int port;
}URL_RESULT_T;

URL_RESULT_T url_result_t;

//Parse usr/password, host, saving directory, port
int parse_domain_dir(char* url, URL_RESULT_T* result)
{
	char* p; //pointer
	char* token = NULL;
	char buf[MAX_URL_LEN] = {0};
	char dir_str[MAX_PATH_FILE_LEN * 2] = {0};
	char svr_dir[MAX_COMM_NAME_LEN] = {0};
	char ip_port[MAX_COMM_NAME_LEN * 2] = {0};
	char domain[INET_DOMAINSTRLEN] = {0};
	char port_s[MAX_PORT_LEN] = {0};
	char user_pass[MAX_USER_LEN + MAX_PASS_LEN] = {0};
	char user[MAX_USER_LEN] = {0};
	char pass[MAX_PASS_LEN] = {0};

	p = buf;
	strncpy(buf,url,sizeof(buf));
	token = strtok(buf, "@");
	if(token != NULL) {
		if(strlen(token) != strlen(url)) 
		{
			if(strlen(user_pass) == 0) {
				strncpy(user_pass, token,sizeof(user_pass));
				p += strlen(user_pass)+1; //point to the string after @
			} 
		}
	}
	//Divide user_name and password if any
	if(strlen(user_pass) > 0)
	{
		token = strtok(user_pass, ":");
		 while(token != NULL) {
			if(strlen(user) == 0) {
				strncpy(user, token,sizeof(user));
				if((strlen(user)==0)||(strlen(user)>MAX_USER_LEN))
				{
					printf("%s() %d\n",__func__,__LINE__);
					return URL_ERROR;
				}
			} else{
				strcat(pass,token);
			}
			token = strtok(NULL, "/");
		}
	}

	token = strtok(p, "/");
	while(token != NULL) {
		if(strlen(ip_port) == 0) {
			strncpy(ip_port, token,sizeof(ip_port));
			if((strlen(ip_port)==0)||(strlen(ip_port)>INET_DOMAINSTRLEN + MAX_PORT_LEN + 1))
			{
				printf("%s() %d\n",__func__,__LINE__);
				return URL_ERROR;
			}
		} else{
			strcat(dir_str,"/");
			strcat(dir_str,token);
		}

		token = strtok(NULL, "/");
	}

	token = strtok(ip_port, ":");
	
	while(token != NULL) {
		if(strlen(domain) == 0) {
			strncpy(domain, token,sizeof(domain));
			if((strlen(domain)==0)||(strlen(domain)>INET_DOMAINSTRLEN))
			{
				printf("%s() %d invalid domain length\n",__func__,__LINE__);
				return URL_ERROR;
			}
		}else if(strlen(port_s) == 0){
			strncpy(port_s, token,sizeof(port_s));
			if((strlen(port_s)==0)||(strlen(port_s)>MAX_PORT_LEN))
			{
				printf("%s() %d invalid port length\n",__func__,__LINE__);
				return URL_ERROR;
			}
		}
		token = strtok(NULL, ":");
	} 
	if(strlen(domain)==0)
	{
		printf("%s() %d there is no domain\n",__func__,__LINE__);
		return URL_ERROR;
	}
	if(strlen(port_s) > 0)
	{
		result->port = atoi(port_s);		
	}

	if(strlen(dir_str)==0){
		//default is "/"
		strcat(dir_str,"/");
	}
	
	else if(strlen(dir_str)!= 0){
		//We want to know if the last character of url is '/'
		int url_len = strlen(url);
		char* url_last = &url[url_len - 1];
		//printf("%c\n", *url_last);
		if(*url_last == '/'){
		strcat(dir_str,"/");}
	}

	strncpy(result->user,user,sizeof(result->user));
	strncpy(result->pass,pass,sizeof(result->pass));
	strncpy(result->domain,domain,sizeof(result->domain));
	strncpy(result->svr_dir,dir_str,sizeof(svr_dir));

	return URL_OK;


}

/*
return code
-1 : invalid param  0:domain   1 :ipv4 
*/
 int check_is_ipv4(char *domain)
 {
	struct in_addr s;  
	char IPdotdec[20] = {0}; 

	if((strlen(domain) == 0) || (strlen(domain)>INET_DOMAINSTRLEN))
	{
		printf("invalid domain length!\n");	
		return -1;
	}
	if(inet_pton(AF_INET, domain, (void *)&s)==1)
	{
		inet_ntop(AF_INET, (void *)&s, IPdotdec, 16);
		//printf("fota_check_is_ipv4 %s\n", IPdotdec);
		return 1;
	}else{
		//printf("may be domain\n");	
		return -1;
	}
 }

//invoke parse_domain_dir()
 int parse_url(char* raw_url, URL_RESULT_T* result)
 {
 	int i;
 	int ret;
 	char* p = NULL;
 	char out_buf[MAX_URL_LEN]; // url
 	char body[MAX_URL_LEN];
 	strcpy(out_buf,"");
 	strcpy(body,"");
 	strncpy(out_buf, raw_url, sizeof(out_buf));
 	p = out_buf;

 	for(i=0;i<ARRAY_SIZE(g_pro_port);i++)
	{	
		if(strncasecmp(g_pro_port[i].pro_s,p,strlen(g_pro_port[i].pro_s))==0)
		{	
			p += strlen(g_pro_port[i].pro_s);
			strncpy(body, p,sizeof(body));
			result->port = g_pro_port[i].port;
			break;
		}
	}
	if(i == ARRAY_SIZE(g_pro_port))
	{
		printf("%s: Error: invalid protocol %s\n", __FUNCTION__,out_buf);
		return URL_ERROR;	
	}

	ret = parse_domain_dir(body,result);
	if(ret == URL_ERROR)
	{
		ret = printf("parse_domain_dir() err\n");
		return URL_ERROR;
	}	
	return ret;

 }

//resolve DNS
int dns_resoulve(char *svr_ip,const char *domain)
{
    char   **pptr;
    struct hostent *hptr;
    char   str[MAX_IP_STR_LEN];

    if((hptr = gethostbyname(domain)) == NULL)
    {
        printf(" gethostbyname error for host:%s\n", domain);
        return URL_ERROR;
    }
#if _URL_DEBUG

    printf("official hostname:%s\n",hptr->h_name);
    for(pptr = hptr->h_aliases; *pptr != NULL; pptr++)
        printf(" alias:%s\n",*pptr);
#endif
    switch(hptr->h_addrtype)
    {
        case AF_INET:
        case AF_INET6:
            pptr=hptr->h_addr_list;
		#if _URL_DEBUG
            for(; *pptr!=NULL; pptr++)
            {
                printf(" address:%s\n",inet_ntop(hptr->h_addrtype, *pptr, str, sizeof(str)));
            }
            printf(" first address: %s\n",inet_ntop(hptr->h_addrtype, hptr->h_addr, str, sizeof(str)));
		#else	
			inet_ntop(hptr->h_addrtype, *pptr, str, sizeof(str));
		#endif	
			strncpy(svr_ip,str,MAX_IP_STR_LEN);
		
        break;
        default:
            printf("unknown address type\n");
        break;
    }

    return URL_OK;
}




