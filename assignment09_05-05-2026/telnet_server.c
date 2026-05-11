#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/wait.h>

#define BUF_SIZE 1024

void signal_handler(int sig){
    while(waitpid(-1,NULL,WNOHANG)>0);
}

int check_login(char *user,char *pass){
    FILE *f=fopen("user.txt","r");

    if(!f) return 0;

    char u[50],p[50];

    while(fscanf(f,"%49s %49s",u,p)!=EOF){
        if(strcmp(u,user)==0&&strcmp(p,pass)==0){
            fclose(f);
            return 1;
        }
    }

    fclose(f);

    return 0;
}

int main(){
    int listen_sock;

    struct sockaddr_in server;

    listen_sock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);

    setsockopt(listen_sock,SOL_SOCKET,SO_REUSEADDR,&(int){1},sizeof(int));

    server.sin_family=AF_INET;
    server.sin_port=htons(8080);
    server.sin_addr.s_addr=INADDR_ANY;

    bind(listen_sock,(struct sockaddr *)&server,sizeof(server));

    listen(listen_sock,5);

    signal(SIGCHLD,signal_handler);

    printf("Telnet server running on port 8080...\n");

    while(1){
        int client=accept(listen_sock,NULL,NULL);

        if(client<0) continue;

        printf("[Client %d] connected\n",client);

        if(fork()==0){
            close(listen_sock);

            int logged_in=0;

            char username[50]={0};

            char *msg="Enter username password:\n";

            send(client,msg,strlen(msg),0);

            while(1){
                char buffer[BUF_SIZE]={0};

                int ret=recv(client,buffer,BUF_SIZE-1,0);

                if(ret<=0) break;

                buffer[ret]=0;

                buffer[strcspn(buffer,"\r\n")]=0;

                if(strlen(buffer)==0) continue;

                if(!logged_in){
                    char user[50]={0};

                    char pass[50]={0};

                    if(sscanf(buffer,"%49s %49s",user,pass)!=2){
                        char *err="Invalid format\nEnter username password:\n";

                        send(client,err,strlen(err),0);

                        continue;
                    }

                    if(check_login(user,pass)){
                        logged_in=1;

                        strcpy(username,user);

                        printf("[Client %d] Login OK: %s\n",client,user);

                        char *ok="Login OK\n";

                        send(client,ok,strlen(ok),0);

                        char prompt[100];

                        sprintf(prompt,"%s@server> ",username);

                        send(client,prompt,strlen(prompt),0);
                    }
                    else{
                        printf("[Client %d] Login FAIL\n",client);

                        char *fail="Login FAIL\nEnter username password:\n";

                        send(client,fail,strlen(fail),0);
                    }
                }
                else{
                    printf("[Client %d - %s] cmd: %s\n",client,username,buffer);

                    char filename[100];

                    sprintf(filename,"out_%d.txt",getpid());

                    char cmd[512];

                    sprintf(cmd,"%s > %s 2>&1",buffer,filename);

                    system(cmd);

                    FILE *fp=fopen(filename,"r");

                    if(fp){
                        char out_buf[BUF_SIZE];

                        while(fgets(out_buf,sizeof(out_buf),fp)){
                            send(client,out_buf,strlen(out_buf),0);
                        }

                        fclose(fp);
                    }
                    else{
                        char *err="Command error\n";

                        send(client,err,strlen(err),0);
                    }

                    char prompt[100];

                    sprintf(prompt,"%s@server> ",username);

                    send(client,prompt,strlen(prompt),0);
                }
            }

            printf("[Client %d] disconnected\n",client);

            close(client);

            exit(0);
        }

        close(client);
    }

    close(listen_sock);

    return 0;
}