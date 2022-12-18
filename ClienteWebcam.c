#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>

//#define PORTNUMBER  47203 //puerto de aragorn
#define MAXLINE 64000

pid_t stream_pid;
int sockfd_TCP;

void killStream(pid_t child_pid);
pid_t startStream(char *hostname , char* PORTNUMBER, char* device);
void exit_handler(int signum);
void cleanBuffer(char *buffer);


// client <domain> <port> <device>
int main(int argc, char *argv[])
{
	struct hostent *hp;
	struct sockaddr_in server_addr;
	char hostname[64];
	int PORTNUMBER;
	int n;
	char buffer_server[124], *device;

	//check argc
	if(argc == 3)//name domain port device	
	{
		//webcam default
		device = "1";	
	}
	else if (argc == 4)	
	{
		device = argv[3];
	}else 
	{
		printf("Usage: %s <domain> <port> [device= <1>:webcam, <2>:desktop]\n", argv[0]);
		exit(-1);
	}

	//exit handler
	signal(SIGINT,exit_handler);

	//server connection
	strcpy(hostname, argv[1]);
	hp = gethostbyname(hostname);
	PORTNUMBER = atoi(argv[2]);
	if( hp == NULL){
		perror("Get host by name");
		exit(-1);
	}

	sockfd_TCP = socket(AF_INET, SOCK_STREAM, 0);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORTNUMBER);
	memcpy(&server_addr.sin_addr, hp->h_addr, hp->h_length);
	printf("TCP Server host NAME: %s\n", hp->h_name);
	printf("TCP Server INET ADDRESS: %s:%d\n", inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));

	printf("Connecting...\n");

	if ( (connect(sockfd_TCP, (struct sockaddr*) &server_addr, sizeof(server_addr))) < 0)
	{
		perror("Error in connection");
		exit(-1);
	}else{
		printf("Connection established\n");
	}


	sleep(1);
	// check for EOF with read if too much clients on server and disconnected
	if ( recv(sockfd_TCP, buffer_server, sizeof(buffer_server), MSG_DONTWAIT) == 0){
		printf("Server too busy, dump connection\n");
		close(sockfd_TCP);
		exit(-1);
	}

	//say your name to server and start sending video
	char message[50];
	int nbytes;
	do{
		printf("Please insert your name...\n");
		nbytes=read(0, message, sizeof(message));
	}while(nbytes<1);
	message[nbytes-1]='\0';

	if (getlogin_r(message, sizeof(message)) == !0)
		perror("Get login name");

	if ( (send(sockfd_TCP, &message, strlen(message), MSG_NOSIGNAL)) < 0 )
	{
		perror("Username message to server");
		close(sockfd_TCP);
		exit(-1);
	}
	printf("User name sended: %s\n", message);


	
	
	printf("Listening to server...\n");
	while( (n=read(sockfd_TCP, &buffer_server, sizeof(buffer_server))) > 0 )
	{
		buffer_server[n]='\0';
		printf("Server : %s\n", buffer_server);
		if( strcmp("START", buffer_server) == 0)
		{
			printf("START stream\n");
			stream_pid = startStream(hostname, argv[2], device);

		}
		else if( strcmp("STOP", buffer_server) == 0)
		{
			printf("STOP stream\n");
			killStream(stream_pid);
		}
		else if( strcmp("BYE", buffer_server) == 0)
		{
			printf("Server closed abruptly\n");
			kill(getpid(), SIGINT);
		}
		else{
			printf("Server message doesnt match: %s\n", buffer_server);
		}

		cleanBuffer(buffer_server);
		printf("Listening to server...\n");
	}
		

	printf("Server closed abruptly\n");
	kill(getpid(), SIGINT);

	return 0;
}


void cleanBuffer(char *buffer)
{
	int i;
	i=0;
	while(*(buffer+i) != '\0')
	{
		*(buffer+i)='\0';
		i++;
	}
	return;
}



void killStream(pid_t child_pid)
{
	int status;
	
	//end child proces gracefully
	system("killall ffmpeg");
	if (kill(child_pid, SIGTERM)<0)
		perror("Kill, SIGTERM");
	
	sleep(1);
	if (waitpid(child_pid, &status, WNOHANG)==0)
	{
		//chil not ended
		kill(child_pid, SIGINT);//by force
		waitpid(child_pid, &status, 0);

		perror("Stop stream by SIGKILL");
	}else{
		perror("Stop stream by SIGTERM");
	}
	
	//printf("Listening to server...\n");
	return;
}


pid_t startStream(char *hostname , char *PORTNUMBER, char* device)
{
	pid_t pid;
	if((pid = fork()) < 0)
	{
        printf("fork() failed: %s\n", strerror(errno));
    }
	if(pid == 0)
	{	
		char buf[1024];

        //child code
		char* webcamStream_path = realpath("webcamStream.sh", buf);
		if (webcamStream_path == NULL){
			perror("Get realpath");
        	kill(getppid(), SIGINT);
		}
		printf("\nStarting video streaming via ffmepg...\n");
        execl(webcamStream_path, "webcamStream.sh" , hostname, PORTNUMBER, device, NULL);

        perror("execute webcamStream.sh");
        kill(getppid(), SIGINT);
		
		
		//"1>/dev/null"
    }else
	{
		printf("Child process created\n");
		return pid;
	}
	
	//never
	return -1;
}



void exit_handler(int signum)
{
	printf("\nTerminating client...\n");
	char message[] = "BYE";

	if ( (send(sockfd_TCP, &message, sizeof(message), MSG_NOSIGNAL)) < 0 )
	{
		perror("Bye message to server");
	}

	close(sockfd_TCP);
	exit(0);
}

