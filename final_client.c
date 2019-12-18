
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAXBUF 1024
typedef struct{
        int fd;
        char ip[20];
}CLIENT;

void receive(CLIENT clientPtr);
int execute(char *com[]);
char *makestring(char *);

int main(int argc,char *argv[]){
        int sockfd;
        char *com[20];
        char argbuf[128];
        char *command[128];
        char buf[MAXBUF+1];
        struct sockaddr_in server;
        CLIENT client_data;
        pthread_t tid;
        /*argv[1]= server ip address, argv[2]=portnumber*/
        if(argc<3){
                printf("Usage: %s Server_Ip server_Port.\n",argv[0]);
                exit(1);
        }
        /*set socket*/
        memset(&server,0,sizeof(server));
        server.sin_family=AF_INET;
        server.sin_addr.s_addr=inet_addr(argv[1]);
        server.sin_port=htons(atoi(argv[2]));

        if((sockfd=socket(AF_INET,SOCK_STREAM,0))==-1){
                printf("fail to call socket().\n");
                return 1;
        }
        if(connect(sockfd,(struct sockaddr *)&server,sizeof(struct sockaddr_in))==-1){
                printf("fail to call connect().\n");
                return 1;
        }
        client_data.fd=sockfd;


        sleep(1);
        while(1){
                int i=0;
                int file_fd;
                printf("ring ring >> ");
                fgets(argbuf,128,stdin);
                command[0]=makestring(argbuf);

                char *ptr=strtok(command[0]," ");
                while(ptr!=NULL){
                        com[i++]=ptr;
                        ptr=strtok(NULL," ");
                }
                com[i]=NULL;

                if(strcmp(com[0],"rm")==0){ /*send file to server and remove file*/
                        int len;
                        if((file_fd=open(com[1],O_RDONLY))==-1){
                                fprintf(stderr,"file is not exist.\n");
                                continue;
                        }
                        /*send file to server*/
                        len=strlen(com[1]);
                        send(sockfd,com[1],len,0);

                        while(1){
                                len=read(file_fd,buf,1024);
                                send(sockfd,buf,len,0);
                                if(len==0)
                                        break;
                        }
                        /*rm file*/
                        execute(com);
                        close(file_fd);
                }
                /*when input = 'q' or 'Q', then close*/
                else if(strcmp(com[0],"Q")==0||strcmp(com[0],"q")==0){
                        send(sockfd,"q",2,0);
                        break;
                }

                /*restore file*/
                else if(strcmp(com[0],"restore")==0){
                        int len;
                        char namebuf[128];
                        char n[10];
                        char *name;

                        send(sockfd,"restore",8,0);
                        /*receive file list from server*/
                        receive(client_data);

                        /*send file name that client want to restore*/
                        printf("which file to restore?\n");
                        fgets(namebuf,128,stdin);
                        name=makestring(namebuf);
                        send(sockfd,name,strlen(name)+1,0);

                        read(sockfd,n,10); /*if file exist n=right, not exist n=fail*/
                        if(strcmp(n,"right")==0){
                                if((file_fd=open(name,O_RDWR|O_CREAT|O_TRUNC,0700))==-1)
                                        perror("file open error: ");
                                else{  /*read file from server*/
                                        while(1){
                                                memset(buf,0x00,1024);
                                                len=read(sockfd,buf,1024);
                                                write(file_fd,buf,len);
                                                if(len<1024||len==EOF){
                                                        printf("restore file complete!\n");
                                                        break;
                                                }
                                        }
                                close(file_fd);
                                }

                        }
                        else if(strcmp(n,"fail")==0){
                                printf("file is not exist.\n");
                        }

                }
                else{
                        execute(com);
                }
        sleep(1);

        }
        close(sockfd);
        return 0;
}

void receive(CLIENT clientPtr){
        int rst;
        char msg[MAXBUF+1];
        while(1){
                memset(msg,0x00,MAXBUF);
                rst=read(clientPtr.fd,msg,MAXBUF);

                printf("%s",msg);
                if(rst<MAXBUF||rst==EOF)
                        break;
        }
        //printf("complete.\n");
}

/*execute command*/
int execute(char *com[]){
        int pid;
        int wait_rv;
        pid=fork();
        switch(pid){
                case -1:
                        perror("fork failed");
                        exit(1);
                case 0:
                        execvp(com[0],com);
                        perror("execvp failed");
                        exit(1);
                default:
                        wait_rv=wait(NULL);
        }
        return 0;
}

char *makestring(char *buf){
        char *cp;
        buf[strlen(buf)-1]='\0';
        cp=malloc(strlen(buf)+1);
        if(cp==NULL){
                fprintf(stderr,"no memory\n");
                exit(1);
        }
        strcpy(cp,buf);
        return cp;
}
