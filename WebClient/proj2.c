/*
Name: Haolai Che
Case-ID: hxc859
file-Name: proj2.c
Date-Created: Sep/12/2022
*/
/*
Proj2 implements a simple http 1.0/1.1 web-client based on Linux-C.
Including -i, -c, -s, -o, -f, -C arguments, arguments could appear 
within arbitrary order. Could download data from target website, 
chunked-encoding supported. Re-direction Supported.
*/
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<strings.h>
#include<unistd.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<netdb.h>
#include<stdbool.h>
#include<ctype.h>
#include<errno.h>
#include "URLparse.h"
#include <fcntl.h>

#define ERROR 1
#define IPSTR url_result_t.svr_ip
#define BUFSIZE 4096
#define RNSIZE 4
#define REQSIZE 512
#define MIN(x, y) ((x) > (y) ? (y) : (x))
extern int optind, opterr, optopt;
extern char* optarg;
char* ustring;
char* ostring;
char* target = "-u";
int flag = 0; //keep track of -u argument 
int flag_i = 0; //keep track of -i argument
int flag_o = 0; //keep track of -o argument
int flag_c = 0; //keep track of -c argument
int flag_s = 0; //keep track of -s argument
int flag_f = 0; //keep track of -f argument
int flag_C = 0; //keep track of -C argument, chunked-encoding
FILE* SOCK_IN;
char buf[BUFSIZE] = {0};
const char s[] = "\n";
const char rn[] = "\r\n\r\n";
char* body_pointer; 
const char ok[] = "200 OK";
const char Moved[] = "301 Moved Permanently";
char* Redirect_LOC;
char* OK_code;
char* Moved_Code;
char* buf_Token;
char buf_cpy[BUFSIZE] ={0};
char request[REQSIZE] = {0};
int sockfd;
FILE* fp;
ssize_t header_len;
ssize_t body_len;
ssize_t iBuff = 1024;



typedef struct {
  int status_code;//HTTP/1.1 '200' OK
  char content_type[128];//Content-Type: application/gzip
  long content_length;
  char file_name[256];
  int chunked_flag;
  char location[MAX_URL_LEN];     //Redirect Location
}resp_header_def;

resp_header_def resp;

int errexit(char* format, char* arg){
	fprintf(stderr,format, arg);
	fprintf(stderr,"\n");
	exit(ERROR);

}
//parse the header
static int get_resp_header(const char *response, resp_header_def *resp)
{
  char *pos = strstr(response, "HTTP/");
  if (pos)
      sscanf(pos, "%*s %d", &resp->status_code);

  pos = strstr(response, "Content-Type:");
  if (pos)
      sscanf(pos, "%*s %s", resp->content_type);

  pos = strstr(response, "Content-Length:");
  if (pos)
      sscanf(pos, "%*s %ld", &resp->content_length);

  pos = strstr(response, "Location:");
  if (pos)
  		sscanf(pos, "%*s %s", resp->location);

  pos = strstr(response, "transfer-encoding:");
  if (pos){
  	if(strstr(response, "chunked")){
  		resp->chunked_flag = 1;
  	}
  	else{
  		errexit("Encoding not supported!\n",NULL);
  	}
  }
  

  return 0;
}


void INF_OP(){
	printf("INF: hostname = %s\n",url_result_t.domain);
	printf("INF: web_filename = %s\n",url_result_t.svr_dir);
	if(ostring != NULL)
		printf("INF: output_filename = %s\n", ostring);
}

void REQ_OP_0(){
	printf("REQ: GET %s HTTP/1.0\r\n",url_result_t.svr_dir);
	printf("REQ: Host: %s\r\n", url_result_t.domain);
	printf("REQ: User-Agent: CWRU CSDS 325 Client 1.0\r\n");
	//printf("\r\n");
}

void REQ_OP_1(){
	printf("REQ: GET %s HTTP/1.1\r\n",url_result_t.svr_dir);
	printf("REQ: Host: %s\r\n", url_result_t.domain);
	printf("REQ: User-Agent: CWRU CSDS 325 Client 1.0\r\n");
}
//http 1.0
void Pack_Request_0(){
	memset(request, 0 , REQSIZE);
	strcat(request, "GET ");
	strcat(request,url_result_t.svr_dir);
	strcat(request, " HTTP/1.0\r\n");
	strcat(request, "Host: ");
	strcat(request,url_result_t.domain);
	strcat(request,"\r\n");
  strcat(request,"User-Agent: CWRU CSDS 325 Client 1.0\r\n");
  strcat(request, "\r\n");
}
//pack request when sending http 1.1 
void Pack_Request_1(){
	memset(request, 0 , REQSIZE);
	strcat(request, "GET ");
	strcat(request,url_result_t.svr_dir);
	strcat(request, " HTTP/1.1\r\n");
	strcat(request, "Host: ");
	strcat(request,url_result_t.domain);
	strcat(request,"\r\n");
  strcat(request,"User-Agent: CWRU CSDS 325 Client 1.0\r\n");
  strcat(request, "\r\n");
}

int Socket_Connect(){
	//create socket
  memset(buf,0x0, BUFSIZE);

	if((sockfd = socket(AF_INET,SOCK_STREAM, 0))<0){
		errexit("socket fail!\n",NULL);
	}
	if(flag_C == 1){
		Pack_Request_1();
	}
	else{
		Pack_Request_0();
	}

  //connect and send http request
  struct sockaddr_in servaddr;
  int writeRet;

  memset ((char *)&servaddr, 0x0, sizeof (servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons (url_result_t.port);

  if(inet_pton(AF_INET, IPSTR , &servaddr.sin_addr) <= 0){
  	errexit("inet_pton Error!\n",NULL);
  }

  if(connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr))<0){
  	errexit("Connect Error!\n",NULL);
  }
  writeRet = write(sockfd, request, strlen(request));
  if(writeRet < 0){
  	printf("http error:%d,%s\n",errno, strerror(errno));
  	errexit("ERROR\n",NULL);
  }

  
  SOCK_IN = fdopen(sockfd, "rb+");
  if(SOCK_IN == NULL){
  	errexit("fail to create Sock file\n",NULL);
  }
  return sockfd;
	//close(sockfd);
  //printf("BYE");
	//Socket part 
}

void get_Header(){
	int readLen = 0;
	Socket_Connect();
	readLen = fread(buf,1,BUFSIZE-1,SOCK_IN); // read only once, take a scoop
  if (readLen == 0)
  {
		close(sockfd);
    printf("The remote end is found to be closed,the thread is terminated！\n");
  }
  //printf("readLen:%d\n", readLen);
  //printf("%s", buf);
  get_resp_header(buf, &resp);
  //printf("content_length:%lu\n",resp.content_length);
  strcpy(buf_cpy, buf);
  buf_Token = strtok(buf_cpy,s);
  OK_code = strstr(buf_Token,ok);
  //moved code
  Moved_Code = strstr(buf_Token,Moved);
  body_pointer = strstr(buf, rn);
	body_pointer += RNSIZE;
	header_len = body_pointer - buf;
	body_len = BUFSIZE - 1 - header_len;
}

//using fgets to get header
void get_Chunked_Header(){
	char cFind[] = "\r\n";
	char cLine[iBuff];
	Socket_Connect();
	while(!feof(SOCK_IN)){
		fgets(cLine, iBuff, SOCK_IN);
		if(strstr(cLine, cFind))
		{
			//printf("%s", cLine);
			if(!strcmp(cLine, cFind))
			{
				break;
			}
		}
	}


}



void RSP_OP(){
	while(buf_Token != NULL)
		{
     	printf( "RSP: %s\n", buf_Token);
  		//strcat(buf_Token, "RSP: ");
  		buf_Token = strtok(NULL, s);
  		if(!strcmp(buf_Token, "\r"))
  			break;
  	}	
}
//Download Data with content length
void download_Data(){
	int total_Len = resp.content_length ;
	//int total_Write = len;
	int total_Write = resp.content_length - body_len;
	int read_Len = 0;
	int rtn_Len = 0;
	int tmp = body_len;
	if(BUFSIZE - 1 >= resp.content_length)
	{
		fp = fopen(ostring, "wb");
		fwrite(body_pointer,1,total_Len,fp);
		fclose(fp);
		//printf("Download success!\n");
	}
	else
	{
		fp = fopen(ostring, "wb");
		fwrite(body_pointer,1,body_len,fp);
		fclose(fp);
		fp = fopen(ostring, "ab");
		if(fp == NULL)
		{
			printf("fail to open file!\n");
		}
		while(total_Write)
		{
			read_Len = MIN(total_Write, BUFSIZE-1);
			rtn_Len = fread(buf,1,read_Len,SOCK_IN);
			if(rtn_Len < read_Len)
			{
				printf("rtn_Len:%d\n",rtn_Len);
		 		printf("read_Len:%d\n",read_Len);
		 		printf("total_Len:%d\n",total_Len);
		 		total_Len -= rtn_Len;
		 		break;
			}
			int x = fwrite(buf,1,rtn_Len,fp );
			tmp += x;
			total_Write -= rtn_Len;
		}
		if(tmp  == total_Len)
		{
			//printf("Download success!\n");
			printf("\r");
		}
		if(total_Write != 0)
		{
			printf("we need to read %ld bytes,instead read %ld bytes.\n",resp.content_length,resp.content_length - total_Write);
		}
		fclose(fp);

	}
	close(sockfd);
}

void no_length_download()
{
	//you have no idea the content length
	get_Chunked_Header();
	//skip the header part
	int read_Len = 0;
	int rtn_Len;
	fp = fopen(ostring, "ab");
	if(fp == NULL){
		printf("fail to open file!\n");
	}
	do{
		read_Len = BUFSIZE - 1;
		rtn_Len = fread(buf, 1, read_Len, SOCK_IN);
		fwrite(buf,1,rtn_Len,fp );

	}while(rtn_Len != 0);
	fclose(fp);
	close(sockfd);

}

//Download chunked data 
void RCV_CHUNK(int len){
	int total_Write = len;
	int read_Len = 0;
	int rtn_Len = 0;
	int tmp = 0;

	if(BUFSIZE - 1 >= len)
	{
		fp = fopen(ostring, "ab");
		fwrite(buf,1,len,fp);
		fclose(fp);
		//printf("Download success!\n");
	}
	else
	{
		fp = fopen(ostring, "ab");
		if(fp == NULL)
		{
			printf("fail to open file!\n");
		}
		while(total_Write)
		{
			read_Len = MIN(total_Write, BUFSIZE-1);
			rtn_Len = fread(buf,1,read_Len,SOCK_IN);
			if(rtn_Len < read_Len)
			{
				printf("rtn_Len:%d\n",rtn_Len);
		 		printf("read_Len:%d\n",read_Len);
		 		printf("total_Len:%d\n",total_Write);
		 		total_Write -= rtn_Len;
		 		break;
			}
			int x = fwrite(buf,1,rtn_Len,fp );
			tmp += x;
			//printf("currently write:%d\n",tmp);
			total_Write -= rtn_Len;
		}
		if(tmp  == total_Write)
		{
			//printf("Download success!\n");
			printf("\r");
		}
		if(total_Write != 0)
		{
			printf("we need to read %d bytes,instead read %d bytes.\n",len,len - total_Write);
		}
		fclose(fp);

	}

}


//iteratively invoke RCV_CHUNK()
void download_Chunked_Data(){
	long part_len;
	
	do{
		fgets(buf, BUFSIZE - 1, SOCK_IN);
		part_len = strtol(buf, NULL, 16);
		//part_len = atoi(buf);
		//printf("part len: %ld\n", part_len);
		RCV_CHUNK(part_len);

		if(2 != fread(buf,1 , 2, SOCK_IN))
		{
			errexit("fread \\r\\n error\n",NULL);
		}
	}while(part_len);
	close(sockfd);

}
	
int main(int argc, char* argv[]){
	if(argc == 1){
		printf("Programname is :%s\n",argv[0]);
		printf("\nNo extra commandline arguments Passed other than Programname\n");}
	if(argc >= 2){
		// printf("number of Arguments Passed:%d\n",argc);
		// printf("These are the Arguments Passed:\n");
		// for(int counter=0; counter<argc;counter++){
		//     printf("argv[%d]: %s\n", counter, argv[counter]);
		// }
		
		for(int i = 0; i< argc; i++){
			if(!strcmp(argv[i],"-u")){
			//	printf("found -u in %d\n", i);
				flag = 1;
				ustring = argv[i+1];
			}
			
		
			if(!strcmp(argv[i],"-i")){
			//	printf("found -i in %d\n", i);
				flag_i = 1;
			}


			
			if(!strcmp(argv[i],"-o")){
			//	printf("found -o in %d\n",i);
				flag_o = 1;
				ostring = argv[i+1];
			
			}
			
			if(!strcmp(argv[i],"-c")){
			//	printf("found -c in %d\n",i);
				flag_c = 1;
			}

			if(!strcmp(argv[i],"-s")){
			//	printf("found -s in %d\n",i);
				flag_s = 1;
			}
			if(!strcmp(argv[i],"-f")){
			//	printf("found -s in %d\n",i);
				flag_f = 1;
			}

			if(!strcmp(argv[i],"-C")){
			//	printf("found -s in %d\n",i);
				flag_C = 1;
			}


		}
		
	}

	if(flag == 0)
		errexit("-u option is mandatory!\n", NULL);
	
	int ret = 0;
	if(flag_i == 1 || flag_c == 1 || flag_s == 1 || flag_o ==1){
		parse_url(ustring, &url_result_t);
		ret = check_is_ipv4(url_result_t.domain);
		if(ret != 1){
			dns_resoulve(url_result_t.svr_ip, url_result_t.domain);
		}
	}
		

	if(flag_i == 1){
		INF_OP();
	}

	if(flag_c == 1){
		if(flag_C == 1){
			REQ_OP_1();
		}
		else{
			REQ_OP_0();
		}
	}

	if(flag_C == 1 && flag_c == 0){
		REQ_OP_1();
	}

	
	Socket_Connect();
	get_Header();
	if(flag_s == 1){
		RSP_OP();
	}

	if(ostring != NULL && OK_code != NULL){
		if(resp.chunked_flag == 1)
		{
			fp = fopen(ostring, "w");
			get_Chunked_Header();
			download_Chunked_Data();
		}
		else{
			if(resp.content_length == 0){
				fp = fopen(ostring, "w");
				no_length_download();

			}
			else{
				download_Data(); // sockfd is closed here
			}
			
		}
		
	}
	else if(OK_code == NULL && Moved_Code == NULL){
		errexit("ERROR: non-200 or 301 response code",NULL);
		close(sockfd);
	}

	while(flag_f == 1 && Moved_Code != NULL)
	{
		Redirect_LOC = resp.location;
		//printf("Redirect_LOC:%s\n", Redirect_LOC);
		parse_url(Redirect_LOC, &url_result_t);
		ret = check_is_ipv4(url_result_t.domain);
		if(ret != 1){
			dns_resoulve(url_result_t.svr_ip, url_result_t.domain);
		}
		Socket_Connect();
		get_Header();
		if(flag_i == 1){
			INF_OP();
		}
		if(flag_c == 1){
			if(flag_C == 1){
				REQ_OP_1();
			}
			else{
				REQ_OP_0();
			}
		}
		if(flag_s == 1){
			RSP_OP();
		}
		if(OK_code != NULL){
			if(flag_o == 1){
				if(resp.chunked_flag == 1)
				{
					fp = fopen(ostring, "w");
					get_Chunked_Header();
					download_Chunked_Data();
				}
				else
				{
					if(resp.content_length == 0){
						fp = fopen(ostring, "w");
						no_length_download();
					}
					else{
						download_Data();
					}
					
				}

			}
			close(sockfd);
			break;
		}


	}

	// if(resp.chunked_flag == 1){
	// 	get_Chunked_Header();
	// 	download_Chunked_Data();
	// }



	
	close(sockfd);


	int c = 0;
	while(EOF != (c = getopt(argc, argv, "u:icso:fC"))){
		//printf("start to process %d para\n", optind);
		switch(c)
		{
			case 'u':
				//printf("-u is used to supply the web server and page the client will access\n");
				ustring = optarg; //obtain u's optarg, the IP address
				//printf("u's argument is %s\n",ustring);
				break;
			case 'i':
				break;
			case 'c':
				break;
			case 's':
				break;
			case 'o':	
				//printf("option -o is under development\n");
				ostring = optarg;
				//printf("option -o's argument is:%s\n",ostring);
				break;
			case 'f':
				break;
			case 'C':
				break;
			case '?':
				printf("currently unknown:%c\n",optopt);
				break;
			default:
				break;
		
		
		
		}
	
	}
}





