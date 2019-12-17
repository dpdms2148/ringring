#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

#define FDCNT 30  /* connection restrict to 30 */
#define MAXBUF 1024


typedef struct {
        int fd;
        char ip[20];
        int client_index;
} Client;

typedef struct {
        char client_ip[20];
        int client_file_fd;
        char file_name[100];
                int index;
} File;

int clientCount = 0;
int sockfd_connect[FDCNT];
void *file_receve(void *clientPtr);

Client client_data[FDCNT] = {0};
File file_data[MAXBUF] = {0};

char file[MAXBUF+1];
char buf[MAXBUF+1];
char rmbuf[MAXBUF+1];
pthread_mutex_t mutx=PTHREAD_MUTEX_INITIALIZER;

int main(){
        int i;
        int tempfd = 0;
        unsigned int clnt_addr_size = 0;
        int sockfd_listen;
        int flag;
        struct sockaddr_in server;
        struct sockaddr_in clnt_addr;

        pthread_t ptid[FDCNT]={0,};

        memset(&server, 0, sizeof(server));
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = htonl(INADDR_ANY);
        server.sin_port = htons(5008);

        if((sockfd_listen = socket(AF_INET, SOCK_STREAM, 0))==-1){
                printf("fail to call socket()\n");
                return 1;
        }

        if(bind(sockfd_listen, (struct sockaddr *)&server, sizeof(struct sockaddr_in))==-1){
                printf("fail to call bind()\n");
                return 1;
        }

        if(listen(sockfd_listen, 5)==-1){
                printf("fail to call listen()\n");
                return 1;
        }

        clnt_addr_size = sizeof(clnt_addr);

        while(1){
                tempfd = accept(sockfd_listen, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
                flag=fcntl(tempfd,F_GETFL,0);
                fcntl(tempfd,flag|O_NONBLOCK);
                if(clientCount == FDCNT){
                        printf("server is full.\n");
                        close(tempfd);
                        continue;
                }

                if(tempfd<0){
                        printf("fail to call accept().\n");
                        continue;
                }
                /*add client info to client_data */
                for(i=0; i<FDCNT; i++){
                        if(strcmp(client_data[i].ip,inet_ntoa(clnt_addr.sin_addr))==0){//already exist client
                                client_data[i].fd = tempfd;
                                break;
                        }

                        else if(client_data[i].fd==0){//new client
                                client_data[i].fd = tempfd;
                                clientCount++;
                                break;
                        }
                }

                strcpy(client_data[i].ip, inet_ntoa(clnt_addr.sin_addr));
                client_data[i].client_index=i; /*client number*/
                printf("Client %d access to server.(%d/%d)\n",client_data[i].client_index,i+1,FDCNT);
                pthread_create(&ptid[i], NULL, file_receve, (void*)&client_data[i]); /*create thread for client */

        }

        for(int i=0;i<FDCNT;i++){
                if(client_data[i].fd)
                        pthread_join(ptid[i],NULL);
        }
        close(sockfd_listen);

        return 0;
}

void* file_receve(void *cp){
        int j, c=0,k;
        int des_fd;//file num
        int file_read_len, read_len;
        char membuf[MAXBUF+1];
        char mmbuf[MAXBUF+1];

        Client* clientPtr = (Client*)cp;

        while(1){
                memset(membuf, 0x00, MAXBUF);
                read_len=read(clientPtr->fd,membuf,MAXBUF);

                if(strcmp(membuf,"q")==0||strcmp(membuf,"Q")==0||read_len==0){
                        break;
                }
                else if(strcmp(membuf, "restore")==0){ //when client want to restore file
                        char res_file[MAXBUF], resbuf[MAXBUF];
                        int res_len, lens, com, res_fd;

                        //send file list
                        pthread_mutex_lock(&mutx);
                        char filesend[20000];
                        strcpy(filesend,"\n----file list----\n");
                        for(int i=0;i<MAXBUF;i++){
                                if(strcmp(file_data[i].client_ip,clientPtr->ip)==0){
                                        strcat(filesend,file_data[i].file_name);
                                        strcat(filesend,"\n");

                                }
                        }
                        strcat(filesend,"------------------\n");
                        write(clientPtr->fd,filesend,strlen(filesend)+1);

                        pthread_mutex_unlock(&mutx);

                        //recieve file name
                        res_len = read(clientPtr->fd, res_file,MAXBUF);
                        if(res_len>0){
                                com=0;

                                 printf("client %d want to restore file %s.\n",clientPtr->client_index,res_file);
                                for(int i=0 ;i<MAXBUF; i++){
                                        /*if file exist in server*/
                                        if(file_data[i].client_file_fd&&strcmp(file_data[i].client_ip, clientPtr->ip)==0&&strcmp(file_data[i].file_name, res_file)==0){         if(file_data[i].index==0)
                                                 res_fd = open(file_data[i].file_name, O_RDONLY);//open file
                                                else {
                                                 char newname1[100];
                                                 sprintf(newname1, "%s%d", file_data[i].file_name, file_data[i].index);
                                                 res_fd = open(newname1, O_RDONLY);
                                                 //printf("file: %s open success.\n",file_data[i].flie_name);
                                                }
                                                printf("file: %s open success.\n",file_data[i].file_name);
                                                send(clientPtr->fd,"right",6,0);


                                                while(1){
                                                        lens=read(res_fd,resbuf,MAXBUF);//read file
                                                        send(clientPtr->fd,resbuf,lens,0);//send file
                                                        if(lens==0){
                                                                printf("finish sending file %s!\n",file_data[i].file_name);
                                                                remove(file_data[i].file_name);
                                                                strcpy(file_data[i].client_ip,"");
                                                                strcpy(file_data[i].file_name,"");
                                                                file_data[i].client_file_fd=0;
                                                                close(res_fd);
                                                                com = 1;
                                                                break;
                                                        }
                                                }

                                        }
                                        if(com == 1){
                                                break;
                                        }
                                }
                                if(com == 0){  /*if file not exist in server*/
                                        printf("file %s not exist in server.\n",res_file);
                                        send(clientPtr->fd,"fail",5,0);}

                        }

                }

                //rm file
                else if(read_len>0){
                        pthread_mutex_lock(&mutx);
                        int count=0;

                        for(j=0;j<MAXBUF; j++){
                                if((strcmp(clientPtr->ip,file_data[j].client_ip)==0)&&(strcmp(membuf,file_data[j].file_name)==0)){ /* same ip, same file_name */
                                        c=j;
                                        break;
                                }
                                else if (strcmp(membuf, file_data[j].file_name) == 0) { /*when server has same file, index++ ( differnet ip , same file)*/
                                        count++;
                                }

                                if(!(file_data[j].client_file_fd)){//find empty space
                                        for(k=0;k<MAXBUF; k++){
                                                if((strcmp(clientPtr->ip,file_data[k].client_ip)==0)&&(strcmp(membuf,file_data[k].file_name)==0)){
                                                        c=k;//파일이 있는index
                                                        break;
                                                }
                                        }
                                        if(k>=MAXBUF){
                                                strcpy(file_data[j].client_ip,clientPtr->ip);
                                                strcpy(file_data[j].file_name,membuf);
                                                file_data[j].index = count;
                                                c=j;
                                                break;
                                        }


                                }
                        }
                        /*if file name already exist in server, then we make "filename"+"index" file */
                        if (file_data[c].index == 0)
                                des_fd = open(file_data[c].file_name, O_WRONLY | O_CREAT|O_TRUNC, 0700);
                        else {
                                char newname[100];
                                sprintf(newname,"%s%d",file_data[c].file_name,file_data[c].index);
                                des_fd = open(newname, O_WRONLY | O_CREAT, 0700);
                        }

                        if(!des_fd){ /*file open fail. go to pthread main while */
                                perror("file open error : ");
                                pthread_mutex_unlock(&mutx);
                                continue;
                        }

                        else{ /*open file success*/
                                file_data[c].client_file_fd = des_fd;
                                printf("open file:%s for client %d.\n", file_data[c].file_name,clientPtr->client_index);
                        }

                        //save file
                        while(1){
                                memset(mmbuf, 0x00, MAXBUF);
                                file_read_len = read(clientPtr->fd, mmbuf, MAXBUF);

                                write(des_fd,mmbuf,file_read_len);
                                if(file_read_len<MAXBUF||file_read_len == EOF){
                                        printf("finish file.\n");
                                        break;
                                }


                        }
                        /*when client cut off connection*/
                        close(des_fd);
                        pthread_mutex_unlock(&mutx);
                }
        }


        printf("Client %d leave server.\n", clientPtr->client_index);
        close(clientPtr->fd);
        return NULL;
}
