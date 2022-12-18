#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include <pthread.h>
#include <semaphore.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>

#define TRUE 1
#define FALSE 0

#define PORTNUMBER  47203   //Puerto escucha/envia ordenes TCP Clientes Webcam
                            //y ademas puerto para recibir video UDP

#define PORTNUMBER2 47204   //Puerto escucha/envia ordenes TCP Cliente Playout 
                            //y ademas puerto de reenvio video UDP

#define MAXLINE 64000       //MaX Size packet UDP       

#define NUM_CLIENTS 3       //Clientes que transmiten video 

struct clientStruct{    /*contain fd(if -1 not connected) and name of a client*/
    int fd;
    char name[50];
    struct sockaddr_in addr;
};
struct Redirect{
  struct sockaddr_in src;
  struct sockaddr_in dst;
  int positionWebcam_i;
};

/*Ordenes por TCP*/
int Clients_TCP_handler(int sfd);
void Start_stream(int sfd);
void Stop_stream(int sfd);
void Bye_strem(int sfd);
int get_free_listClient(struct clientStruct *list);

//=======================//
struct clientStruct lista_clientes[NUM_CLIENTS];

//=========================//

//MUTEX//
pthread_mutex_t lock;


void exit_handler(int signum);
//UDP PATH//
void killUDPthread(pthread_t tid);
pthread_t Hilo_UDP;
void* UDP_path(void* param); //Funcion encargada crear flujo video por UDP

//TCP PATH//
pthread_t Hilo_TCP;
void* TCP_path(void* param); //Funcion encargada socket TCP con playout

int socket_playout, new_socket_playout; //cerrar en handler
struct sockaddr_in address; //addres webcam server
struct sockaddr_in serv_addr_2;

int main(int argc, char *argv[])
{   

    //Iniciamos mutex
    
	signal(SIGINT,exit_handler);
    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("Fallo en creacion de mutex\n");
        return 1;
    }

	int option = TRUE;
    int master_socket, n;
    socklen_t addrlen;
    struct sockaddr_in from;

    char msg[124]; //Data

    fd_set readfds, readfds_Copy;

    //Inicializamos todos los clientes en 0
    int i;


    for (i = 0; i < NUM_CLIENTS; ++i)
    {
        lista_clientes[i].fd = -1;
    }


    //Encargado TCP playout
    pthread_create(&Hilo_TCP, NULL, TCP_path, NULL);

    //Creamos socket master
    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    //Permitir a socket master permitir multiple conexiones
    //Buen habito

    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&option, sizeof(option)) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    //Config socket
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORTNUMBER);

    //Bind master socket
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    printf("Escuchando en puerto: %d\n", PORTNUMBER);

    if (listen(master_socket, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    //Esperando conexiones
    addrlen = sizeof(address);
    printf("Esperando conexiones\n");

    //Remueve los elementos del conjunto
    FD_ZERO(&readfds_Copy);

    //Agregamos master socket al conjunto de descriptores
    FD_SET(master_socket, &readfds_Copy);

    //=========================WEBCAMS=============================//
    int newPosition , newSocket;
    while(TRUE){
        memcpy(&readfds, &readfds_Copy, sizeof(fd_set));
        select(FD_SETSIZE, &readfds, NULL, NULL, NULL); //blockeante

        if (FD_ISSET(master_socket, &readfds)) //Cuando se trata de master_socket
        {
            printf("Aceptando una nueva conexion...\n");
            
            pthread_mutex_lock(&lock);

            newSocket = accept(master_socket, (struct sockaddr *)&from, &addrlen);
            newPosition = get_free_listClient(lista_clientes);
            if (newPosition  != -1){    // there is free space
                lista_clientes[newPosition].fd = newSocket;
                FD_SET(lista_clientes[newPosition].fd, &readfds_Copy);

                n = recv(newSocket, msg, sizeof(msg), 0);    //get webcam name
                msg[n]='\0';
                strcpy( lista_clientes[newPosition].name, msg);
                printf("New webCam Client name: %s\n", lista_clientes[newPosition].name);
                memcpy(&(lista_clientes[newPosition].addr), &from, sizeof(from));

            } else{
                close(newSocket);   //send EOF
            }

            pthread_mutex_unlock(&lock);  

        }
        for (i=0; i<NUM_CLIENTS; i++){  /*check webcam clients messages(only BYE)*/
            pthread_mutex_lock(&lock);
            if( (lista_clientes[i].fd != -1) && FD_ISSET(lista_clientes[i].fd, &readfds)){  /*if valid fd and fd is set*/ 
                
                n = recv(lista_clientes[i].fd, msg, sizeof(msg), 0);    /*get webcam bye*/
                msg[n]='\0';
                printf("Message TCP from webCam client: %s\n", msg);
                if(strcmp(msg, "BYE") != 0){
                    printf("Webcam TCP message is not BYE, n=%d\n", n);
                }
                close(lista_clientes[i].fd);
                FD_CLR(lista_clientes[i].fd, &readfds_Copy);
                lista_clientes[i].fd = -1;  /*disconnect client*/
            }
            pthread_mutex_unlock(&lock);
        }
    }

    pthread_join(Hilo_TCP, NULL);

    pthread_mutex_destroy(&lock);
	return 0;
}

/*===========================================================================================================================================================================*/
/*===========================================================================================================================================================================*/
/*===========================================================================================================================================================================*/

int get_free_listClient(struct clientStruct *list){
    int i;
    for(i=0 ; i<NUM_CLIENTS; i++){
        if(list[i].fd == -1){
            return i;   //return free position
        }
    }

    return -1;  //return no free position
}

void* UDP_path(void *param){   //Recibe video UDP por PORTNUMBER y reenvia a PORTNUMBER2, su funcion es hacer lo que hacia el codigo en su version 1
    printf("START UDP PATH\n");
    int sock_src, sock_dst, n;
    char buffer[MAXLINE];
    socklen_t lenDST, lenSRC, len;
    struct Redirect UDPaddr;
    UDPaddr = *(struct Redirect*) param;

    lenDST=sizeof(UDPaddr.dst);
    lenSRC=sizeof(UDPaddr.src);
    len = sizeof(address);
    memset(&serv_addr_2, 0, sizeof(serv_addr_2));
    int len_2= sizeof(serv_addr_2);

    serv_addr_2.sin_family = AF_INET;
    serv_addr_2.sin_port = htons(PORTNUMBER2);
    serv_addr_2.sin_addr.s_addr = htonl(INADDR_ANY);


    sock_src = socket(AF_INET, SOCK_DGRAM, 0);
    sock_dst = socket(AF_INET, SOCK_DGRAM, 0);
    pthread_cleanup_push(close, sock_dst);  //"rutina de salida"
    pthread_cleanup_push(close, sock_src);     
    
    if (bind(sock_src, (struct sockaddr *) &address, len) < 0 )
        perror("UDP socket error");
    
    if (bind(sock_dst, (struct sockaddr *) &serv_addr_2, len_2) < 0 )
        perror("UDP socket error");
    
	printf("UDP Server INET ADDRESS: %s:%d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
    
	printf("UDP src client INET ADDRESS: %s:%d\n", inet_ntoa(UDPaddr.src.sin_addr), ntohs(UDPaddr.src.sin_port));
    
	printf("UDP dst client ADDRESS: %s:%d\n", inet_ntoa(UDPaddr.dst.sin_addr), ntohs(UDPaddr.dst.sin_port));
    char buffer_2[128];
    int count;
    int k=0;
    while( (n=recvfrom(sock_src, (char *)buffer, MAXLINE, 0, (struct sockaddr *) &(UDPaddr.src), &lenSRC))  > 0){ 
        
        buffer[n]='\0'; //for printf
        k=recvfrom(sock_dst, (char *)buffer_2, 128, 0, (struct sockaddr *)&(UDPaddr.dst), &lenDST); //recv "ping" to know addr

        if(sendto(sock_dst, buffer, n, 0, (struct sockaddr *) &(UDPaddr.dst), lenDST) < 0)
            break;
        //printf("Resend package count: %d\n", count++);
    }


    perror("Redirect UDP video in thread");
    Stop_stream(lista_clientes[UDPaddr.positionWebcam_i].fd);  //funcion para que video cliente pare
    close(sock_src);
    close(sock_dst);

    pthread_cleanup_pop(1);
    pthread_cleanup_pop(1);
    pthread_exit(NULL);    
}

void* TCP_path(void *param){
    int option_playout;
    char buf[128];
    int n, i;
    socklen_t len;
    struct sockaddr_in name;
	int option = TRUE;
    struct Redirect UDPAddrs;
    char informacion[1024];
    char message_ACK[128];


    //Crea socket
    socket_playout = socket(AF_INET, SOCK_STREAM, 0);

    name.sin_family = AF_INET;
    name.sin_port = htons(PORTNUMBER2);
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    len = sizeof(struct sockaddr_in);

    if (bind(socket_playout, (struct sockaddr*)&name, len))
    {
        perror("bind error in TCP_path");
    }
    if (setsockopt(socket_playout, SOL_SOCKET, SO_REUSEADDR, (char *)&option, sizeof(option)) < 0)
    {
        perror("setsockopt error in TCP_path");
        exit(EXIT_FAILURE);
    }
    listen(socket_playout, 1);

    
    
    while(TRUE){
        
        printf("\nAccepting new PLAYOUT client\n");
        new_socket_playout = accept(socket_playout, (struct sockaddr *)&name, &len);
        printf("New PLAYOUT client accept succesfull\n");
        //FALTA AGREGAR CASO EN QUE LISTA ESTE VACIA
        UDPAddrs.dst=name;


        START_ENVIO_INFO: /*label to start the cycle of sending stream options*/
        
        
        //sleep(1);
        strcpy(informacion, "Escoja entre los siguientes flujos de video:");  
        /* send info to client*/
        if ( send(new_socket_playout, (char *)informacion, sizeof(char)*(strlen(informacion)+1),0) < 0) //Manda info titular + \0
            perror("Send optiond to playout client\n");
           // indice i de la lista +1 para evitar atoi() problem

        if( read(new_socket_playout, message_ACK, sizeof(message_ACK)) < 0) //recibe ack
            perror("Read ack from playout client\n");

        printf("\nSending options to client\n");

        pthread_mutex_lock(&lock);
        for (i = 0; i < NUM_CLIENTS; i++)
        {   
            //printf("Checking client n°%d\n",i);
            if (lista_clientes[i].fd != -1) //fd connected
            {   
                //printf("To send valid client n°%d\n",i);
                sprintf(informacion,"%d ---> %s",(i+1), lista_clientes[i].name);

                //printf("After sprintf() client n°%d\n",i);
                if( send(new_socket_playout, (char *)informacion, sizeof(char)*(strlen(informacion)+1),0) < 0) //Manda info con lista de conectados
                    perror("Send option to playout client\n");

                //printf("After send() client n°%d\n",i);
                if( read(new_socket_playout, message_ACK, sizeof(message_ACK)) < 0) //recibe ack
                    perror("Read ack from playout client\n");
                //printf("After read() client n°%d\n",i);
            }                        
        }
        pthread_mutex_unlock(&lock);

        strcpy(informacion, "END");
        send(new_socket_playout, (char *)informacion, sizeof(char)*(strlen(informacion)+1),0); //Manda info de que se acabo la lista
        
        printf("ALL options sended\n");
        


        //receive int info response from playout option
        
        printf("\nWaiting option from playout client:\n");
        if( (n = recv(new_socket_playout, &buf, sizeof(buf), 0)) > 0){

            option_playout = atoi(buf); //viene revisado por errores
            printf("Option from playout client: %d\n", option_playout);
            pthread_mutex_lock(&lock);
            
            if ( (option_playout>0) && ((option_playout-1)<NUM_CLIENTS) && (lista_clientes[option_playout-1].fd != -1))  //option info esta desfasado por 1
            {
                strcpy(informacion, "OK");
                if ( send(new_socket_playout, (char *)informacion, sizeof(char)*(strlen(informacion)),0) < 0) //Manda check OK
                    perror("Send ok to playout client");
                    
                memcpy(&UDPAddrs.src, &lista_clientes[option_playout-1].addr, sizeof(lista_clientes[option_playout-1].addr));
                UDPAddrs.positionWebcam_i = option_playout-1;
                Start_stream(lista_clientes[option_playout-1].fd); //MANDA START A WEBCAM
                //Crear camino UDP
                pthread_create(&Hilo_UDP, NULL, UDP_path, (void*)&UDPAddrs);
                pthread_mutex_unlock(&lock);
            }
            else{
                strcpy(informacion, "NO");
                send(new_socket_playout, (char *)informacion, strlen(informacion),0); //Manda info de que socket ya no esta disponible
                pthread_mutex_unlock(&lock);
                system("clear -x");
                goto START_ENVIO_INFO;
            }

            //block on recieve message from playout clietn
            
            printf("\nWaiting command (BYE,STOP) from playout client...\n");
            if ( (n = recv(new_socket_playout, &buf, sizeof(buf), 0)) > 0 ){
                buf[n]='\0';
                printf("Respones from playout client: %s\n", buf);

                if ( strcmp(buf, "BYE") == 0)   //layout client is disconnecting
                {   
                    pthread_mutex_lock(&lock);
                    if (lista_clientes[option_playout-1].fd != -1)  //Manda info de que se acabo la lista
                    {
                        Stop_stream(lista_clientes[option_playout-1].fd); //MANDA STOP A WEBCAM

                    }
                    pthread_mutex_unlock(&lock);
                    killUDPthread(Hilo_UDP); //Mata hilo
                    close(new_socket_playout);
                    continue; //got to accept() again

                }
                else if ( strcmp(buf, "STOP") == 0)   //layout client is stoping listening video
                {      
                    if (send(new_socket_playout, &buf, strlen(buf),0) < 0)   //echo message in case comes from timeout
                        perror("Echo stop message to playout client");

                    pthread_mutex_lock(&lock);
                    if (lista_clientes[option_playout-1].fd != -1)
                    {
                        Stop_stream(lista_clientes[option_playout-1].fd); //MANDA STOP A WEBCAM

                    }
                    pthread_mutex_unlock(&lock);
                    //echo stop
                    killUDPthread(Hilo_UDP); //Mata hilo
                    system("clear -x");
                    goto START_ENVIO_INFO;
                }
                else{
                    printf("Response from playout client doesn't MATCH\n");
                    continue;  //got to accept() again
                }
            }else{
                perror("Recv() commands from playout client");
            }
        }else{
            perror("Recv() option from playout client");
        }
    }


    close(socket_playout);
    pthread_join(Hilo_UDP, NULL);
    pthread_exit(NULL); 


    
    /* PSEUDO CODIGO DE LO DE ARRIBA
    while(){
        accept();
        while(){
            while(check){
                sendInfo();
                receiveInfo();
                check=checkInfo();
            }  
            pthread_create();

            if(receive()=="STOP"){
                killUDPthread();
            }else if(receive()=="BYE"){
                //to accept
                break;
            }
       }
    }
    */
}


int Clients_TCP_handler(int sfd){
    return 0;
}

void Start_stream(int sfd){  //funcion para que video cliente comience
    char* msg_start = "START"; 
    if (send(sfd, (char *)msg_start, strlen(msg_start), 0) < 0){
        perror("Enviar orden START");
    }
}

void Stop_stream(int sfd){ //funcion para que video cliente pare
    char* msg_stop = "STOP"; 
    if (send(sfd, (char *)msg_stop, strlen(msg_stop), 0) < 0){
        perror("Enviar orden STOP");
    }
}

void Bye_strem(int sfd){ //funcion para que video cliente desconecte
    char* msg_bye = "BYE"; 
    if (send(sfd, (char *)msg_bye, strlen(msg_bye), 0) < 0){
        perror("Enviar orden BYE");
    }
}

void killUDPthread(pthread_t tid){
  if (pthread_cancel(tid) != 0)
    perror("UDP thread_cancel");
  if (pthread_join(tid, NULL) != 0)
    perror("UDP thread_join");
  return;
}

void exit_handler(int signum)
{
	printf("\nTerminating server...\n");
	char message[] = "BYE";

	if ( (send(new_socket_playout, &message, sizeof(message), MSG_NOSIGNAL)) < 0 )
		perror("Bye message to playout client");

    
	close(socket_playout);
	close(new_socket_playout);
	exit(0);
}
