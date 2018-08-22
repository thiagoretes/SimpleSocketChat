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

#define PORT 45678
#define TRUE 1
#define FALSE 0
#define CLIENT_NAME_SIZE 25

typedef enum op_code
{
    SET_NAME = 1, SEND_MESSAGE = 2
} Op_code;

void setClientName(int socket, char* name)
{
    int sizeofName = strlen(name);
    if(sizeofName > CLIENT_NAME_SIZE) sizeofName = CLIENT_NAME_SIZE;
    char* packet = (char*)malloc(sizeofName+2);//1 for opCode + 1 for end of name justincase
    packet[0] = SET_NAME;
    //packet[1] = '\0';
    strcat(packet,name);
    send(socket, packet, sizeof(packet),0);
    free(packet);
}

void sendClientMessage(int socket, char* msg, char* net_buf)
{
    int sizeOfMsg = strlen(msg);
    net_buf[0] = SEND_MESSAGE;
    net_buf[1] = '\0';
    strcat(net_buf, msg);
    send(socket, net_buf, sizeof(net_buf), 0);
    
}

void handleUserCommand(int socket, char* net_buf, char* cmd_buf)
{
    char command[51], content[1001];
    int i = 0;
    fscanf(stdin, "%50[^ \t]", cmd_buf);
    fflush(stdin);
    //printf("cmd_buf:%s\n", cmd_buf);
    sscanf(cmd_buf, "%50[^ \t]", command);
    for(int i = 0; i < 51; i++)
    {
        command[i] = tolower(command[i]);
    }
    if(strcmp("setname", command) == 0)//Command is setname
    {
        fscanf(stdin, "%1001s", cmd_buf);
        sscanf(cmd_buf, "%1001s", content);
        fflush(stdin);
        printf("Content: %s\n", content);
        //sprintf(net_buf,"%c%s",SET_NAME,content);
        setClientName(socket, content);
    }
    if(strcmp("say", command) == 0)//Command is sendMessage
    {
        //fgets(cmd_buf, sizeof(cmd_buf), stdin);
        fscanf(stdin,"%1001s", cmd_buf);
        sscanf(cmd_buf, "%1001s", content);
        printf("content: %s\n", content);
        fflush(stdin);
        sendClientMessage(socket, content, net_buf);
    }
}

int main(int argc, char *argv[])
{
    struct sockaddr_in address, serv_addr;//IP Address struct
    char *hello = "Hello from client";
    char buffer[1024] = {0};
    char command_buffer[1050] = {0};
    int sock = 0, valread;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0))<0)
    {
        perror("Socket Creation");
        exit(EXIT_FAILURE);
    }
    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0)
    {
        perror("Converting IP Address to Binary Form");
        exit(EXIT_FAILURE);
    }

    if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("Trying to connect");
        exit(EXIT_FAILURE);
    }

    if(send(sock, hello, strlen(hello),0)<0)
    {
        perror("send hello");
        exit(EXIT_FAILURE);
    }
    printf("Sent Hello from client\n");
    while(TRUE)
    {
        if((valread=recv(sock,buffer,sizeof(buffer),0)) < 0)
        {
            perror("Recv\n");
            exit(EXIT_FAILURE);
        }

        if(valread>0){
            buffer[valread] = '\0';
            printf("Received: %s\n", buffer);
        }
        
        memset(buffer,0,sizeof(buffer));
        memset(command_buffer,0,sizeof(command_buffer));

        printf("Enter command: ");
        handleUserCommand(sock,buffer,command_buffer);
        
        //fgets(command_buffer,sizeof(command_buffer),stdin);
        //send(sock,buffer,strlen(buffer),0);        
    }
    

    
    printf("Hello message sent\n");
    valread = read(sock, buffer, 1024);
    printf("%s\n", buffer);
    return 0;
}