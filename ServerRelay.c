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

#define PORTNUMBER2 47204   //Puerto UDP de reenvio de video de SERVER a CLIENTE PLAYOUT y además por el que recibe 
                            //clientesWebcam por TCP

#define PORT_47200 47200
#define PORT_47201 47201


#define MAXLINE 64000       //MaX Size packet UDP       

#define NUM_CLIENTS 3       //Clientes que transmiten video 

struct clientStruct{    /*contain fd(if -1 not connected) and name of a client*/
    int fd;
    char name[50];
    int port;
    struct sockaddr_in addr;
};
struct Redirect{
  struct sockaddr_in src;
  struct sockaddr_in dst;
  int positionWebcam_i;
};

/*Ordenes por TCP*/
int Clients_TCP_handler(int sfd);
int conmutador=47204;
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
    char* ACK_msg = "ACK";
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

    printf("-_Recibiendo registro de Webcams por puerto: %d\n", PORTNUMBER);

    if (listen(master_socket, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    //Esperando conexiones
    addrlen = sizeof(address);
    printf("-_Esperando conexiones con Webcams\n");

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
            printf("-_Aceptando nueva conexion Webcam...\n");
            
            pthread_mutex_lock(&lock);

            newSocket = accept(master_socket, (struct sockaddr *)&from, &addrlen);
            newPosition = get_free_listClient(lista_clientes);
            if (newPosition  != -1){    // there is free space
                lista_clientes[newPosition].fd = newSocket;
                FD_SET(lista_clientes[newPosition].fd, &readfds_Copy);

                n = recv(newSocket, msg, sizeof(msg), 0);    //get webcam name
                msg[n]='\0';
                strcpy( lista_clientes[newPosition].name, msg);
                printf("-_Nombre nuevo cliente Webcam: %s\n", lista_clientes[newPosition].name);
                
                
                if ((send(newSocket, (char *)ACK_msg, strlen(ACK_msg),0)) < 0)  
                {
                      perror("Send ACK\n");
                }
                
                n = recv(newSocket, msg, sizeof(msg),0);
                msg[n]='\0';
                lista_clientes[newPosition].port=atoi(msg);
                //printf("-_Puerto por el que envia video Webcam: %d", lista_clientes[newPosition].port);
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
    printf("Hilo UDP activado\n");
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

    int sock_47200, sock_47201;
    struct sockaddr_in addr_47200, addr_47201;
    socklen_t len_47200, len_47201;
    len_47200 = sizeof(addr_47200);
    len_47201 = sizeof(addr_47201);

    addr_47200.sin_family=AF_INET;
    addr_47200.sin_port=htons(PORT_47200);
    addr_47200.sin_addr.s_addr =htonl(INADDR_ANY);

    addr_47201.sin_family=AF_INET;
    addr_47201.sin_port=htons(PORT_47201);
    addr_47201.sin_addr.s_addr =htonl(INADDR_ANY);

    sock_47200 = socket(AF_INET, SOCK_DGRAM, 0);
    sock_47201 = socket(AF_INET, SOCK_DGRAM, 0);

    if (bind(sock_47200, (struct sockaddr *) &addr_47200, len_47200)<0)
    {
        perror("UDP socket error");
    }

    if (bind(sock_47201, (struct sockaddr *) &addr_47201, len_47201)<0)
    {
        perror("UDP socket error");
    }
    
    //pthread_cleanup_push(close, sock_dst);  //"rutina de salida"
    //pthread_cleanup_push(close, sock_src);     
    
    if (bind(sock_src, (struct sockaddr *) &address, len) < 0 )
        perror("UDP socket error");
    
    if (bind(sock_dst, (struct sockaddr *) &serv_addr_2, len_2) < 0 )
        perror("UDP socket error");
    
    
    
    char buffer_2[128];
    int count;
    int k=0;
    while(1){ 
        if (conmutador==47200)
        {   
            
            n=recvfrom(sock_47200, (char *)buffer, MAXLINE, 0, (struct sockaddr *) &(addr_47200), &len_47200);
            buffer[n]='\0'; //for printf
            k=recvfrom(sock_dst, (char *)buffer_2, 128, 0, (struct sockaddr *)&(UDPaddr.dst), &lenDST); //recv "ping" to know addr            
            if(sendto(sock_dst, buffer, n, 0, (struct sockaddr *) &(UDPaddr.dst), lenDST) < 0)
            break;
            
        }
        
        if (conmutador==47201){
           
            n=recvfrom(sock_47201, (char *)buffer, MAXLINE, 0, (struct sockaddr *) &(addr_47201), &len_47201);
            buffer[n]='\0'; //for printf
            k=recvfrom(sock_dst, (char *)buffer_2, 128, 0, (struct sockaddr *)&(UDPaddr.dst), &lenDST); //recv "ping" to know addr
            if(sendto(sock_dst, buffer, n, 0, (struct sockaddr *) &(UDPaddr.dst), lenDST) < 0)
            break;
            
        }
        /*
        if (conmutador==47204){
            
            n=recvfrom(sock_src, (char *)buffer, MAXLINE, 0, (struct sockaddr *) &(UDPaddr.src), &lenSRC);
            k=recvfrom(sock_dst, (char *)buffer_2, 128, 0, (struct sockaddr *)&(UDPaddr.dst), &lenDST); //recv "ping" to know addr
            if(sendto(sock_dst, buffer, n, 0, (struct sockaddr *) &(UDPaddr.dst), lenDST) < 0)
            break;
            
        }
        */
        
        
        //printf("Resend package count: %d\n", count++);
    }


    perror("Redirect UDP video in thread");
    Stop_stream(lista_clientes[UDPaddr.positionWebcam_i].fd);  //funcion para que video cliente pare
    close(sock_src);
    close(sock_dst);

    //pthread_cleanup_pop(1);
    //pthread_cleanup_pop(1);
    pthread_exit(NULL);    
}

void* TCP_path(void *param){
    
    int option_playout;
    char opcion[16];
    char buf[128];
    int n, i;
    socklen_t len;
    struct sockaddr_in name;
	int option = TRUE;
    struct Redirect UDPAddrs;
    pthread_create(&Hilo_UDP, NULL, UDP_path, (void*)&UDPAddrs);
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
        
        printf("-_Esperando conexion con cliente Playout...\n");
        new_socket_playout = accept(socket_playout, (struct sockaddr *)&name, &len);
        printf("-_Cliente Playout aceptado satisfactoriamente\n");
        //FALTA AGREGAR CASO EN QUE LISTA ESTE VACIA
        UDPAddrs.dst=name;


        START_ENVIO_INFO: /*label to start the cycle of sending stream options*/
        
        
        strcpy(informacion, "-_Escoja entre los siguientes flujos de video:\n");
        printf("%s", informacion); 

        pthread_mutex_lock(&lock);
        for (i = 0; i < NUM_CLIENTS; i++)
        {   
            //printf("Checking client n°%d\n",i);
            if (lista_clientes[i].fd != -1) //fd connected
            {   
                //printf("To send valid client n°%d\n",i);
                sprintf(informacion,"-_Opcion %d ---> %s, puerto %d",(i+1), lista_clientes[i].name, lista_clientes[i].port);
                printf("%s\n", informacion);
            }                        
        }
        pthread_mutex_unlock(&lock);

        printf("-_-1 ---> Actualizar lista\n");
        
        char string[16];

        do
        {
            n = read(STDIN_FILENO, string, sizeof(string));
            string[n-1]='\0';
            fflush(STDIN_FILENO);
        } while (atoi(string)==0);
        
        int largo = sizeof(opcion);
        strncpy(opcion, string, largo-1);
        opcion[largo-1]='\0';
        option_playout=atoi(opcion);
        printf("Opcion escogida:%d\n", option_playout);
        
        pthread_mutex_lock(&lock);
        if ((option_playout>0)&&((option_playout-1)<NUM_CLIENTS)&& (lista_clientes[option_playout-1].fd != -1))
        {
            conmutador=lista_clientes[option_playout-1].port;
            pthread_mutex_unlock(&lock);
            goto START_ENVIO_INFO;
        }
        else{
            pthread_mutex_unlock(&lock);
            goto START_ENVIO_INFO;
        }
            

        
    }


    close(socket_playout);
    pthread_join(Hilo_UDP, NULL);
    pthread_exit(NULL); 
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

    //Agregar matar hilos

    
	close(socket_playout);
	close(new_socket_playout);
	exit(0);
}
