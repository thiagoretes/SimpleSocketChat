#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>

#define FALSE 0
#define TRUE 1
#define MAXCLIENTS 2
#define PORT 45678


typedef enum op_code
{
    SET_NAME = 1, SEND_MESSAGE = 2
} Op_code;


typedef struct client {
    int socket_desc;
    char name[25];
} Client;

int setClientName(Client* client, char* name)
{
    char* response =(char*)malloc(strlen(client->name)+34);
    if(strncpy(client->name, name, 24) == NULL)
    {
        perror("settingClientName");
        exit(EXIT_FAILURE);
    }
    client->name[24] = '\0';
    sprintf(response, "Successfully changed name to \"%s\"", client->name);
    
    send(client->socket_desc, response, strlen(response), 0);

    free(response);
    
    return strlen(client->name);
}

int sendClientMessage(Client* origin, Client connected_clients[], char* msg)
{
    int i, counter = 0;
    char* strToSend = (char*)malloc(strlen(msg)+strlen(origin->name));
    sprintf(strToSend, "[%s]:%s", origin->name, msg);
    for(i = 0; i < MAXCLIENTS; i++)
    {
        if(connected_clients[i].socket_desc != 0){
            send(connected_clients[i].socket_desc, msg, strlen(msg), 0);
            counter++;
        }
    }

    free(strToSend);
    return counter;
}


int handleMessage(Client* origin, Client connected_clients[], char* msg, int msgSize)//Socket Descriptor, Buffer, msgSize
{
    if(msgSize < 0) return 0;
    Op_code msgType = msg[0];
    int result;
    char *msgContent = (char*)calloc(msgSize-1,sizeof(char));
    switch (msgType)
    {
        case SET_NAME:
            sscanf(&msg[1], "%s", msgContent);
            setClientName(origin, msgContent);
            break;
        case SEND_MESSAGE:
            sscanf(&msg[1], "%s", msgContent);
            result = sendClientMessage(origin, connected_clients, msgContent);
            printf("\"%s\", sent to %d clients, from client with name %s\n", msgContent, result, origin->name);
            break;
        default:
            break;
    }
    return 0;
}


int main(int argc, char *argv[])
{
    int new_socket, valread, master_socket, activity, i, sd, max_sd;
    Client client_list[MAXCLIENTS];
    struct sockaddr_in address;//Structure used to specify which socket to connect.
    int opt = TRUE;
    int addrlen = sizeof(address);
    char buffer[1025] = {0};//Initialize all elements with zero. 1k BUFFER + END STRING
    char hello[] = "Connection accepted!\nSimpleTextTransfer 1.0\n";
    fd_set readfds; //Set of File Descriptors

    //Initializing client sockets
    for(i = 0; i < MAXCLIENTS; i++)
    {
        strncpy(client_list[i].name, "Undefined", sizeof("Undefined"));
        client_list[i].socket_desc = 0;
    }

    //Creating socket file descriptor
    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket Creation");
        exit(EXIT_FAILURE);
    }

    //Setting socket options : Using SOL_SOCKET to operate at socket API level
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("Attaching Socket to Port");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;//TODO: MAKE A FUNCTION FOR THIS BLOCK
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );

    //Forcing socket attachment to desired port
    if (bind(master_socket, (struct sockaddr*) &address, sizeof(address))<0)
    {
        perror("Binding socket");
        exit(EXIT_FAILURE);
    }

    if (listen(master_socket, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Waiting for connections...\n");
    while(TRUE)
    {
        FD_ZERO(&readfds);//clear the set
        FD_SET(master_socket, &readfds);//Add master socket to the read file descriptor set
        max_sd = master_socket;

        //Add client sockets to set
        for(i = 0; i < MAXCLIENTS; i++)
        {
            //socket descriptor
            sd = client_list[i].socket_desc;
            //Checking if the socket is initialized before adding to the set
            if (sd > 0)
                FD_SET(sd, &readfds);
            
            //highest file descriptor number
            if(sd > max_sd)
                max_sd = sd;
        }

        //Wait for activity in one of the sockets... Timeout is null, so it will wait until someone sends
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if((activity<0) && errno!=EINTR)
        {
            perror("select waiting activity");
            exit(EXIT_FAILURE);
        }

        if (FD_ISSET(master_socket, &readfds))//Operations of the master socket
        {
            if((new_socket = accept(master_socket, (struct sockaddr*)&address, (socklen_t*)&addrlen))<0)
            {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            //Inform send and receive commands
            printf("Nova conexão: socketFd: %d,IP: %s, port: %d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            //Send Accepted Message
            if(send(new_socket,hello,strlen(hello),0) != strlen(hello))
            {
                perror("send welcome message");
                printf("deu ruim aqui\n");
                exit(EXIT_FAILURE);
            }

            printf("Sent accepted message to %s on socketFd %d.\n", inet_ntoa(address.sin_addr), new_socket);
            for(i = 0; i < MAXCLIENTS; i++)
            {
                if(client_list[i].socket_desc == 0)//Position is free
                {
                    client_list[i].socket_desc = new_socket;
                    printf("New Socket added to the socket array as %d.\n", i);
                    break;
                }
            }
        }
        //Operations of client_sockets
        for(i = 0; i < MAXCLIENTS; i++)
        {
            sd = client_list[i].socket_desc;
            if(FD_ISSET(sd,&readfds))
            {
                if((valread = read(sd,buffer,1024)) == 0)
                {
                    //happened a disconnection
                    getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
                    printf("Host Disconnected\tIP: %s\tPort: %d\n",inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                    close(sd);
                    strncpy(client_list[i].name, "Undefined", sizeof("Undefined"));
                    client_list[i].socket_desc = 0;

                }
                else //Treat message that came in
                {
                    handleMessage(&client_list[i],client_list,buffer,valread);
                    //send(sd,buffer,strlen(buffer),0);
                }
            }
        }
    }
    
    return 0;
}