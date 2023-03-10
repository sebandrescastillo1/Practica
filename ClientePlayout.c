
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h> 
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include <ctype.h>


#define TRUE 1
#define _GNU_SOURCE         /* See feature_test_macros(7) */
#define PORTNUMBER_SERVER  47204 //otro puerto de aragorn por donde se comunica con el cliente 2
#define PORTNUMBER_PLAYOUT 12345 //puerto playout
#define MAXLINE 64000

struct Redirect{
  struct sockaddr_in src;
  struct sockaddr_in dst;
};

int sockS_UDP, sockS_TCP, sock_PLAYOUT; 

void getOptions(int socket, char* option, int len);
void exitHandler(int signum);
void killUDPthread(pthread_t tid);
int checkPollResult(struct pollfd *pfds, int len, pthread_t tid);
void *startUDPredirect(void *redirectSocks);  /*thread*/
void exit_handler(int signum);




int main(int argc, char *argv[])
{
  pthread_t UDP_tid;
	char hostname[64];
  struct pollfd pfds[2];
	int n;
  int optionTRUE = TRUE;
	
	struct hostent *hp;
	struct sockaddr_in addr_SERVER, addr_PLAYOUT, addr_FROM, addr_me;
  

	signal(SIGINT,exit_handler);
  //get server hostname
	strcpy(hostname, argv[1]);
	hp = gethostbyname(hostname);
  if(hp==NULL){
    perror("Get host by name");
    exit(-1);
  }

  //create sockets
	sockS_UDP = socket(AF_INET, SOCK_DGRAM, 0);
	sockS_TCP = socket(AF_INET, SOCK_STREAM, 0);
	sock_PLAYOUT = socket(AF_INET, SOCK_DGRAM, 0);

  //prepare sockets structs
	memset(&addr_SERVER, 0, sizeof(addr_SERVER));
	memset(&addr_PLAYOUT, 0, sizeof(addr_PLAYOUT));
	memset(&addr_FROM, 0, sizeof(addr_FROM));
	memset(&addr_FROM, 0, sizeof(addr_me));

  //addr server info
	addr_SERVER.sin_family = AF_INET;
  addr_SERVER.sin_port = htons(PORTNUMBER_SERVER);
  memcpy(&addr_SERVER.sin_addr, hp->h_addr_list[0], hp->h_length);

  //addr playout info
  addr_PLAYOUT.sin_family = AF_INET;
  addr_PLAYOUT.sin_port = htons(PORTNUMBER_PLAYOUT);
  addr_PLAYOUT.sin_addr.s_addr = inet_addr("10.2.51.11"); //ip playout:10.2.50.11


  
  if (setsockopt(sockS_UDP, SOL_SOCKET, SO_REUSEADDR, (char *)&optionTRUE, sizeof(optionTRUE)) < 0)
  {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }

  //Connect to server
  printf("-_Conectando con servidor...\n");
  if ( (connect(sockS_TCP, (struct sockaddr*) &addr_SERVER, sizeof(addr_SERVER))) < 0)
	{
		perror("Error in connection");
		exit(-1);
	}else{
		printf("-_Conexion establecida\n");
	}

  //get port in use with TCP
  socklen_t len = sizeof(addr_me);
  if( getsockname(sockS_TCP, (struct sockaddr *) &addr_me, &len) <0 )
    perror("getsockname socket TCP");
  if( bind(sockS_UDP, (struct sockaddr *) &(addr_me), sizeof(addr_me)) <0 )
    perror("Bind UDP socket");


  //set poll() pfds for stdin and server  
  pfds->fd = sockS_TCP; //[0] is server
  pfds->events = POLLIN;
  (pfds+1)->fd = STDIN_FILENO;  //[1] is stdin
  (pfds+1)->events = POLLIN;
  



  /*prepare redirect struct do redirect UDP video*/
  struct Redirect videoAddr;
  videoAddr.dst=addr_PLAYOUT;
  videoAddr.src=addr_SERVER;

  /* Start negotiation*/
  char response[16], option[16];

  if ( pthread_create(&UDP_tid, NULL, startUDPredirect, (void*)&videoAddr) < 0)      /*start UDP recvfrom() and sendto()*/
  {
    perror("UDP redirect thread ");
    kill(getpid(),SIGINT);
  }

  while (1)
  {

    do{
    printf(/*\nInsert STOP : to stop video and choose another*/"\nInsert BYE : to close the client\n");
    poll(pfds, 2, -1);                            /*blocking poll() for serverTCP and stdin*/
    }while(checkPollResult(pfds, 2, UDP_tid) == 1);   /*stop udpThread(STOP) or SIGINT to end(BYE) or wrong message(check again:1)*/
    /*loop again to get new options*/
    system("clear -x");
    printf("AGAIN\n");
  }

  
  /*never*/
  close(sockS_UDP);
  exit(0);
}



/*==========================================================================================================================================================================*/
/*==========================================================================================================================================================================*/
/*==========================================================================================================================================================================*/





/*
  *terminate program with a bye message and closes sockets
*/
void exit_handler(int signum)
{
	printf("\nTerminating client...\n");
	char message[] = "BYE";

	if ( (send(sockS_TCP, &message, sizeof(message), MSG_NOSIGNAL)) < 0 )
	{
		perror("Bye message to server");
	}

	close(sockS_TCP);
  close(sockS_UDP);
  close(sock_PLAYOUT);
	exit(0);
}



/* 
  *receive fd with messages, and decide to ende UDP stream or terminate program
*/
int checkPollResult(struct pollfd *pfds, int len, pthread_t tid)
{
  int i, n;
  int stopStream[2]={0,0};
  char buffer[len][64]; /*no need to flush old messagges*/
  
  
  for (i=0;i<len;i++)   /*check pfds, server first and stdin second*/ 
  {
    fflush(STDIN_FILENO);
    if (pfds[i].revents == POLLIN)  /*check the ones with messages*/
    {
      if( ( n = read(pfds[i].fd, buffer[i], sizeof(buffer[i])) )>0 )   /*read new message*/
      {
        
        buffer[i][n]='\0';
        if(i==1)
          buffer[i][n-1]='\0';  //overwrites '\n'

        printf("Message received from %d(SERVER:0, STDIN:1):  %s\n", i, buffer[i]);
        if (strcmp(buffer[i],"BYE") == 0)   /*Priority check BYE to terminate client*/
        {
          killUDPthread(tid);          /*no critical locks or malloc in thread*/
          kill(getpid(), SIGINT);   /*end programm*/
          return 0; /*never*/
        }
        else if (strcmp(buffer[i],"STOP")==0) /*check STOP to terminate UDP stream*/
        {
          stopStream[i]=1;   /*maybe will stop if no BYE arrives*/ 
        }
        else
        {
          printf("WRONG message i=%d :%s\n", i , buffer[i]);  /*wrong message*/
        }

      }else //EOF form server
      {
        if( (i == 0) && (n == 0))   /* server(i=0) terminated abruptly(EOF)*/
        {
          killUDPthread(tid);         /*no critical locks or malloc in thread*/
          kill(getpid(), SIGINT);    /*end programm*/
          return 0;                  /*never*/
        }
      }
    }
  }


    
  return 0;


}

/*
  *start UDP recfrom() from server and UPD sendto() to PLAYOUT until canceled
*/
void *startUDPredirect(void *redirectSocks)
{
  int n;
  struct Redirect UDPaddr;
  UDPaddr = *(struct Redirect*) redirectSocks;
	char buffer[MAXLINE];
  socklen_t lenSRC = sizeof(UDPaddr.src);
  socklen_t lenDST = sizeof(UDPaddr.dst);
  struct timeval time;
  fd_set udpRead;
  time.tv_sec=30;
  time.tv_usec=0;

  char *hello= "Ping";

 
  

  struct sockaddr_in cli_addr;
  memset(&cli_addr, 0, sizeof(cli_addr));
  int len_3 = sizeof(cli_addr); 

  FD_ZERO(&udpRead);
  FD_SET(sockS_UDP, &udpRead);
  

  /*read and send UDP traffic, 5sec timeout*/
  sendto(sockS_UDP, (char *)hello, strlen(hello), 0, (struct sockaddr *)&(UDPaddr.src), lenSRC);
  printf("UDP redirect START...\n ");

  //(n=recvfrom(sockS_UDP, (char *)buffer, MAXLINE, 0, (struct sockaddr *) &cli_addr, &len_3))
  while( select(FD_SETSIZE, &udpRead, NULL, NULL, 0) > 0){ 
    if ( (n=recvfrom(sockS_UDP, (char *)buffer, MAXLINE, 0, (struct sockaddr *) &(cli_addr), &len_3)) < 1)
      break;

    if(sendto(sock_PLAYOUT, buffer, n, 0, (struct sockaddr *) &(UDPaddr.dst), lenDST) < 0)
      break;
    //printf("UDP package n?? %d ", count++);

    sendto(sockS_UDP, (char *)hello, strlen(hello), 0, (struct sockaddr *)&(UDPaddr.src), lenSRC);    //"ping"
    /*reset timeout*/
    time.tv_sec=30;
    time.tv_usec=0;
  }

  
  char msg[]="STOP";
  perror("ERROR UDP redirect");

  if ((send(sockS_TCP, (char *)msg, sizeof(msg),0)) < 0)  
    perror("Send STOP\n");
  

  pthread_exit(NULL);


}


void killUDPthread(pthread_t tid){
  if (pthread_cancel(tid) != 0)
    perror("UDP thread_cancel");
  if (pthread_join(tid, NULL) != 0)
    perror("UDP thread_join");
  return;
}