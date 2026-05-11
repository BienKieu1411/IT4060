#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>

#define PORT 8080
#define BUF_SIZE 256

void signal_handler(int sig){
    while(waitpid(-1,NULL,WNOHANG)>0);
}

int main(){
    int listener;
    struct sockaddr_in addr;

    listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);

    if(listener<0){
        perror("socket() failed");
        return 1;
    }

    setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&(int){1},sizeof(int));

    memset(&addr,0,sizeof(addr));

    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    addr.sin_port=htons(PORT);

    if(bind(listener,(struct sockaddr *)&addr,sizeof(addr))<0){
        perror("bind() failed");
        close(listener);
        return 1;
    }

    if(listen(listener,5)<0){
        perror("listen() failed");
        close(listener);
        return 1;
    }

    signal(SIGCHLD,signal_handler);

    printf("Time Server listening on port %d...\n",PORT);

    while(1){
        int client=accept(listener,NULL,NULL);

        if(client<0)
            continue;

        printf("[Client %d] Connected\n",client);

        if(fork()==0){
            close(listener);

            char *welcome_msg=
                "Welcome to Time Server\n"
                "Command format: GET_TIME [format]\n"
                "Supported formats:\n"
                "- dd/mm/yyyy\n"
                "- dd/mm/yy\n"
                "- mm/dd/yyyy\n"
                "- mm/dd/yy\n";

            send(client,welcome_msg,strlen(welcome_msg),0);

            while(1){
                char buf[BUF_SIZE]={0};

                int len=recv(client,buf,sizeof(buf)-1,0);

                if(len<=0)
                    break;

                buf[len]=0;

                buf[strcspn(buf,"\r\n")]=0;

                printf("[Client %d] Request: %s\n",client,buf);

                char cmd[32]={0};
                char format[32]={0};
                char extra[32]={0};

                int n=sscanf(buf,"%31s %31s %31s",cmd,format,extra);

                if(n!=2||strcmp(cmd,"GET_TIME")!=0){
                    char *err=
                        "Error: Invalid syntax\n"
                        "Usage: GET_TIME [format]\n";

                    send(client,err,strlen(err),0);

                    continue;
                }

                time_t now=time(NULL);

                struct tm *t=localtime(&now);

                char result[64];

                int valid=1;

                if(strcmp(format,"dd/mm/yyyy")==0){
                    strftime(result,sizeof(result),"%d/%m/%Y\n",t);
                }
                else if(strcmp(format,"dd/mm/yy")==0){
                    strftime(result,sizeof(result),"%d/%m/%y\n",t);
                }
                else if(strcmp(format,"mm/dd/yyyy")==0){
                    strftime(result,sizeof(result),"%m/%d/%Y\n",t);
                }
                else if(strcmp(format,"mm/dd/yy")==0){
                    strftime(result,sizeof(result),"%m/%d/%y\n",t);
                }
                else{
                    valid=0;
                }

                if(valid){
                    send(client,result,strlen(result),0);

                    printf("[Client %d] Sent time: %s",client,result);
                }
                else{
                    char *err="Error: Unsupported time format\n";

                    send(client,err,strlen(err),0);

                    printf("[Client %d] Invalid format\n",client);
                }
            }

            printf("[Client %d] Disconnected (PID %d)\n",client,getpid());

            close(client);

            exit(0);
        }

        close(client);
    }

    close(listener);

    return 0;
}