/*
Name: Haolai Che
Case ID: hxc859
File Name: proj3.c
Date Created: 10/04/2022
<==============================================================>
This code implements a simple web server                      ||
-p: Assign a port where the server runs on                    ||
-r: The directory from which the server serves files          ||
-t: Specifies authentication token to terminate the service   ||
This server accepts TCP connections on the port given via -p  ||
This server accepts standard, well-formed HTTP requests       ||
GET && TERMINATE method supported.                            ||
<==============================================================>
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
#include <fcntl.h>

#define BUFSIZE 4096
#define ERROR 1
#define PROTOCOL "tcp"
#define QLEN 5
//#define HTTPPROLEN 8


int count = 0;
int flag_p = 0; // keep track of argument p
int flag_r = 0; // keep track of argument r
int flag_t = 0; // keep track of argument t
int flag_malform = 0; //if is 400 malformed Request
int flag_PNI = 0; // if is 501 Protocol Not Implemented
int flag_USM = 0; // if is Unsupported Method
int flag_TERMINATE = 0;
int flag_OKSHUT = 0;//if is 200 ok shut down
int flag_MALSHUT = 0;//if is 403 forbidden shut
int flag_MALFILE = 0;//if is 406 Invalid File name
int flag_OKGET = 0;//if is 200 ok get
int flag_NOFIND = 0;//if is 404 not found
int sockfd; //socket file descriptor
int client_sfd;
int res;
int rc_fd; // receive file descriptor
char* portstring; // the argument p's param 
char* file_Directory; // the argument r's param
char* terminate_Code; // the argument t's param
char buf[BUFSIZE] = {0}; // declare buffer 
char filestr[BUFSIZE] = {0};
FILE* RCV_IN;
FILE* fp;
const char MALFORM[] =  "HTTP/1.1 400 Malformed Request\r\n\r\n";
const char MALMETHOD[] = "HTTP/1.1 405 Unsupported Method\r\n\r\n";
const char MALPROTOCOL[] = "HTTP/1.1 501 Protocol Not Implemented\r\n\r\n";
const char OKSHUTDOWN[] = "HTTP/1.1 200 Server Shutting Down\r\n\r\n";
const char MALSHUTDOWN[] = "HTTP/1.1 403 Operation Forbidden\r\n\r\n";
const char MALFILE[] = "HTTP/1.1 406 Invalid Filename\r\n\r\n";
const char OKGET[] = "HTTP/1.1 200 OK\r\n\r\n";
const char NOFIND[] = "HTTP/1.1 404 File Not Found\r\n\r\n";

typedef struct {
	char* method;
	char* argument;
	char* P_version; // protocol
}HTTP_REQUEST;
HTTP_REQUEST request;

int errexit(char* format, char* arg)
{
	fprintf(stderr,format, arg);
	fprintf(stderr,"\r\n");
	exit(ERROR);

}

/*Parse the Request
Request is in RCV_IN
fgets read the first line
ignore the others
1.Check if each line of the request ends with \r\n and 
the whole request ends with \r\n\r\n
1.If not valid 400
2.If valid: Get the first line
3.Check if it is HTTP/Version, if not 501
3.Check if the METHOD is GET or TERMINATE
4.IF neither, 405
4.IF GET: 

5.IF TERMINATE:
*/
int is_begin_with(const char * str1,char *str2)
{
	int len1  = strlen(str1);
	int len2  = strlen(str2);
	char* p  = str2;
	int i = 0;
	if(str1 == NULL || str2 == NULL)
	{
		return -1;
	}
	if((len1 < len2) || (len1 == 0 || len2 == 0))
	{
		return -1;
	}
	while(*p != '\0')
	{
		if(*p != str1[i])
		{
			return 0;
		}
		p++;
		i++;
	}
	return 1;

}

/*
void cd(char* path)
{
	chdir(path);
	if(chdir(path) != 0)
	{
		errexit("chdir()to %s fail!\r\n",path);
	}
}
*/

void write_file(char* filename)
{
	int read_Len = 0;
	int rtn_Len;
	FILE* fp;
	fp = fopen(filename, "rb");
	if(fp == NULL)
	{
		errexit("CRUSH! Fail to Open File!\r\n",NULL);
	}
	do{
		read_Len = BUFSIZE - 1;
		rtn_Len = fread(buf, 1, read_Len, fp);
		write(client_sfd,buf,rtn_Len);
	}while(rtn_Len != 0);
	fclose(fp);
	close(client_sfd);
	close(sockfd);
}

int if_Exists(char* filename)
{
	FILE* file;
	file = fopen(filename, "rb");
	if(file != NULL)
	{
		fclose(file);
		return 1;
	}
	else
	{
		//printf("File Not Exists!!\r\n");
		return 0;
	}
}

void terminate_OP()
{
	//request argument matches -t argument
	if(!strcmp(request.argument, terminate_Code))
	{
		flag_OKSHUT = 1;
	}
	else if(strcmp(request.argument, terminate_Code) != 0)
	{
		flag_MALSHUT = 1;
	}
}

void Get_OP(){
	if(!is_begin_with(request.argument, "/"))
	{
		flag_MALFILE = 1;
	}
	if(flag_MALFILE == 0)
	{
		if(!strcmp(request.argument,"/"))
		{
			request.argument = "/homepage.html";
		}
		strcpy(filestr, file_Directory);
		strcat(filestr, request.argument);
		if(!if_Exists(filestr))
		{
			flag_NOFIND = 1;//file not found
		}
		else if(if_Exists(filestr))
		{
			flag_OKGET = 1;//found the file
		}
	}


}



void parse_HTTPREQ(HTTP_REQUEST* request, char* http_data)
{
	const char slash_rn[] = "\r\n";
	//const char slash_n[] = "\n";
	const char slash[] = "/";
	const char slash_rnrn[] = "\r\n\r\n";
	const char htc[] = "HTTP";
	const char getc[] = "GET";
	const char termi[] = "TERMINATE";
	//char dest[3] = {0};
	char data_cpy_n[BUFSIZE] = {0};
	char* token;
	char* last_four; // last four characters
	//char* endofFirstLine;
	char* start;
	char* method;
	char* url = 0;
	char* version = 0;
	int only_r = 0;
	int only_n = 0;

	/*
	if(strstr(http_data, htc) != 0)
	{
		endofFirstLine = strstr(http_data, htc);
		endofFirstLine += HTTPPROLEN;
		strncpy(dest, endofFirstLine, 2);
		dest[2] = '\0';
		if(strcmp(dest, slash_rn) != 0)
		{
			//detect 400 malform error
			flag_malform = 1;
		}

	}
	*/

	last_four = &http_data[strlen(http_data) - 4];
	if(strcmp(last_four, slash_rnrn) != 0)
	{
		//there is no blank line at the end
		//ending malform
		flag_malform = 1;
	}

	for(int i = 0; i < strlen(http_data); i++)
	{
		if(http_data[i] == '\r' && http_data[i+1] != '\n')
		{
			only_r = 1;
			//there is \r but no \n
		}
		else if(http_data[i] == '\n' && http_data[i-1] != '\r')
		{
			only_n = 1;
			//there is \n but no \r
		}
	}
	if(only_n == 1 || only_r == 1)
	{
		flag_malform = 1;
	}

	//take the first line
	strcpy(data_cpy_n, http_data);
	token = strtok(data_cpy_n, slash_rn);
	//printf("First Line:%s\r\n\r\n",token);

	start = token;
	method = start;

	for(;*start && *start != '\r'; start++)
	{	
		if(*start == ' ')
		{
			if(url == 0)
			{
				url = start + 1;
			}
			else
			{
				version = start + 1;
			}
			*start = '\0';
		}
	}
	*start = '\0';
	request->method = method;
	request->argument = url;
	request->P_version = version;

	/*
	//Test Success.
	printf("METHOD IS:%s\r\n",request->method);
	printf("ARGUMENT IS:%s\r\n",request->argument);
	printf("VERSION IS:%s\r\n",request->P_version);
	*/
	token = strtok(request->P_version, slash);

	if(strcmp(token, htc) != 0)
	{
		flag_PNI = 1;
	}

	if(strcmp(request->method, getc) != 0 && strcmp(request->method, termi) != 0)
	{
		flag_USM = 1;
	}

	if(!strcmp(request->method, termi))
	{
		flag_TERMINATE = 1;
		terminate_OP();
	}

	if(!strcmp(request->method, getc))
	{
		Get_OP();
	}


}





//initiate Socket Connection
int socket_init()
{	
	struct sockaddr_in servaddr;

	memset ((char *)&servaddr, 0x0, sizeof (servaddr));
  	servaddr.sin_family = AF_INET;
  	servaddr.sin_addr.s_addr = INADDR_ANY; // '0.0.0.0' ,unsure
  	//assign the port from input
  	servaddr.sin_port = htons((u_short) atoi (portstring));
  	sockfd = socket(AF_INET, SOCK_STREAM, 0);
  	if(sockfd < 0)
  	{
  		errexit("Fail to Create Socket!\r\n",NULL);
  	}
  	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
  	{
  		errexit("setsockopt(SO_REUSEADDR) failed\r\n",NULL);
  	}
    
	/*Bind the Socket*/
	res = bind(sockfd,(struct sockaddr *)&servaddr, sizeof(servaddr));
	if(res < 0)
	{
		errexit("Bind Error!\r\n",NULL);//port reuse issue
	}
	res = listen(sockfd, QLEN);
	if(res < 0)
	{
		errexit("Cannot listen on port %s\r\n", portstring);
	}

	return sockfd;
}

//Accept a connection
void socket_accept()
{
	memset(buf, 0x0, BUFSIZE);
	struct sockaddr_in client_sockaddr;
	socklen_t len = sizeof(client_sockaddr);

	memset(&client_sockaddr, 0x0, sizeof(client_sockaddr));
	//get client_sfd
	client_sfd = accept(sockfd, (struct sockaddr *)& client_sockaddr, &len);
	if(client_sfd< 0)
	{
		errexit("Fail to Accept Connection!\r\n",NULL);
	}

	/*
	1.receive request
	2.parse request and all
	3.make the return content as requested
	4.write it to client socket
	*/
	
	read(client_sfd, buf, BUFSIZE);
	
	//printf("%s",buf);

	parse_HTTPREQ(&request, buf);

	if(flag_malform == 1){
		int ret_wr = 0;
		ret_wr = write(client_sfd,MALFORM,strlen(MALFORM));
		if(ret_wr < 0)
		{
			errexit("Error Write!\r\n",NULL);
		}
		close(client_sfd);
		close(sockfd);
		//exit(0);

	}

	else if(flag_malform == 0)
	{
		if(flag_PNI == 1)
		{
			int ret_wr = 0;
			ret_wr = write(client_sfd, MALPROTOCOL, strlen(MALPROTOCOL));
			if(ret_wr < 0)
			{
				errexit("Error Write!\r\n",NULL);
			}
			close(client_sfd);
			close(sockfd);
			//exit(0);
		
		}
		else if(flag_PNI == 0)
		{
			if(flag_USM == 1)
			{
				int ret_wr = 0;
				ret_wr = write(client_sfd, MALMETHOD, strlen(MALMETHOD));
				if(ret_wr < 0)
				{
					errexit("Error Write!\r\n",NULL);
				}
				close(client_sfd);
				close(sockfd);
				//exit(0);
				
			}

			if(flag_OKSHUT == 1)
			{
				int ret_wr = 0;
				ret_wr = write(client_sfd, OKSHUTDOWN, strlen(OKSHUTDOWN));
				if(ret_wr < 0)
				{
					errexit("Error Write!\r\n",NULL);
				}
				close(client_sfd);
				close(sockfd);
				exit(0);
			}


			if(flag_MALSHUT == 1)
			{
				int ret_wr = 0;
				ret_wr = write(client_sfd, MALSHUTDOWN, strlen(MALSHUTDOWN));
				if(ret_wr < 0)
				{
					errexit("Error Write!\r\n",NULL);
				}
				close(client_sfd);
				close(sockfd);
				// exit(0);
			}

			if(flag_MALFILE == 1)
			{
				int ret_wr = 0;
				ret_wr = write(client_sfd, MALFILE, strlen(MALFILE));
				if(ret_wr < 0)
				{
					errexit("Error Write!\r\n",NULL);
				}
				close(client_sfd);
				close(sockfd);

			}

			if(flag_NOFIND == 1)
			{
				int ret_wr = 0;
				ret_wr = write(client_sfd, NOFIND, strlen(NOFIND));
				if(ret_wr < 0)
				{
					errexit("Error Write!\r\n",NULL);
				}
				close(client_sfd);
				close(sockfd);
			}

			if(flag_OKGET == 1)
			{
				int ret_wr = 0;
				ret_wr = write(client_sfd, OKGET, strlen(OKGET));
				if(ret_wr < 0)
				{
					errexit("Error Write!\r\n",NULL);
				}
				write_file(filestr);

			}



		}


	}


	close(client_sfd);
 	close(sockfd);
	
	

}







int main(int argc, char* argv[])
{
	if(argc == 1)
	{
		printf("Programme name is:%s\r\n", argv[0]);
		printf("No extra argument passed other than the name\r\n");
	}

	if(argc >= 2)
	{
		for(int i = 0; i < argc; i++)
		{
			if(!strcmp(argv[i], "-p"))
			{
				flag_p = 1;
				portstring = argv[i+1];
			}

			if(!strcmp(argv[i], "-r"))
			{
				flag_r = 1;
				file_Directory = argv[i+1];
			}

			if(!strcmp(argv[i], "-t"))
			{
				flag_t = 1;
				terminate_Code = argv[i+1];
			}
		}

	}

	//Judging if all required args are there
	if(flag_t == 0 || flag_r == 0 || flag_t == 0)
	{
		errexit("Missing Arguments!\r\n",NULL);
	}

	//judging if input port num is legal
	if(atoi(portstring) < 1025)
	{
		errexit("PortNumber Reserved!:%s\r\n",portstring);
	}
	if(atoi(portstring) >65535)
	{
		errexit("Invalid PortNumber!:%s\r\n",portstring);
	}



	// socket_init();
	
	// socket_accept();
	
	do{
		socket_init();
		socket_accept();
		flag_malform = 0; 
		flag_PNI = 0; 
		flag_USM = 0; 
		flag_TERMINATE = 0;
		flag_OKSHUT = 0;
		flag_MALSHUT = 0;
		flag_MALFILE = 0;
		flag_OKGET = 0;
		flag_NOFIND = 0;

	}while(flag_OKSHUT == 0);



	
	while(EOF != (count = getopt(argc, argv, "p:r:t:")))
	{
		switch(count)
		{
			case 'p':
				//printf("arg p is port number\r\n");
				portstring = optarg;
				break;

			case 'r':
				//printf("arg r specifies the directory\r\n");
				file_Directory = optarg;
				break;

			case 't':
				//printf("arg t is for authentication token\r\n");
				terminate_Code = optarg;
				break;

			case '?':
				//printf("Unknown Argument Passed! : %c\r\n", optopt);
				break;

			default:
				break;
		}
	}

}