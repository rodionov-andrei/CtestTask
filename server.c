#include <sys/time.h>       /* for struct timeval {} */
#include <fcntl.h>          /* for fcntl() */
#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), bind(), and connect() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <pthread.h>
#include <wait.h>
#include <errno.h>

#include "main.h"
#include "mtrace.h"

extern void DieWithError ( char *errorMessage );
int CreateUDPServerSocket ( unsigned short port );
int SendByTime (int servSock,  struct sockaddr_in ClntAddr, int CurrId, ClntOpt myOpt);
int ChooseId ( int id[], char msg[MAXMSG]);
void SetOptions ( char *msg, ClntOpt *myOpt);

int main(int argc, char *argv[]) {
    int servSock;                   
    int maxDescriptor;               
    fd_set sockSet;                  /* Набор дескрипторов для select */
    int running = 1;
    unsigned short portNo;           /* Номер порта */
    unsigned int cliAddrLen; 
    struct sockaddr_in ClntAddr; // Адрес клиента

    ClntOpt myOpt;

    if (argc != 2) {     /* Test for correct number of arguments */
        fprintf(stderr, "Usage:  %s <Port UDP> ...\n", argv[0]);
        exit(1);
    }

    maxDescriptor = -1;
    portNo = atoi(argv[1]);  
    servSock = CreateUDPServerSocket(portNo);

    if (servSock > maxDescriptor)
        maxDescriptor = servSock;
    
    int id [MAXCLIENTS];
    int CurrId;
    for (int i = 0; i < MAXCLIENTS; ++i) {
        id[i]=0;
    }
    char msg[MAXMSG];
    int child_pid[MAXCLIENTS];

    printf("Сервер уведомлений запущен: нажмите Enter для отключения\n");
    while (running) {
        FD_ZERO(&sockSet);
        FD_SET(STDIN_FILENO, &sockSet);
        FD_SET(servSock, &sockSet);

        select(maxDescriptor + 1, &sockSet, NULL, NULL, NULL);
        
        /* Проверяем не нажат ли Enter */
        if (FD_ISSET(0, &sockSet)) {
            printf("Shutting down server\n");
            running = 0;
            kill ( -getpid(), SIGTERM );
        }

        if (FD_ISSET(servSock, &sockSet)) {
            int recvMsgSizeU;
            cliAddrLen = sizeof (ClntAddr);
            
            if ((recvfrom (servSock, msg, MAXMSG, 0, (struct sockaddr *) &ClntAddr, &cliAddrLen  )) < 0)
                DieWithError("recvfrom() failed");

            if (strcmp(msg, "IDREQUEST") == 0 || strstr(msg, "IDOFFER")!=NULL){
                CurrId = ChooseId ( id, msg);
                sprintf (msg, "%d", CurrId);

                if (( sendto (servSock, msg, strlen(msg), 0,(struct sockaddr *) &ClntAddr, sizeof(ClntAddr)) <0 ))
                    DieWithError("send sent a different number of bytes than expected");

                if ((recvMsgSizeU = recvfrom (servSock,msg, MAXMSG, 0, (struct sockaddr *) &ClntAddr, &cliAddrLen  )) < 0)
                    DieWithError("recvfrom() failed");

                if (strcmp(msg, "ACK") == 0)
                    TRACE (printf("Зарегистрирован новый клиент, id %d\n", CurrId));
                else
                    printf("Ошибка, неизвестный ответ клиента\n");
            }else if (strstr (msg, "EXIT") != 0){
                CurrId = atoi(strtok ( msg, " "));
                for (int i = 0; i < MAXCLIENTS; ++i) {
                    if (id[i]==CurrId){
                        id[i]=0;
                        TRACE (printf("Клиент %d удален\n", CurrId));
                        kill (child_pid[CurrId], 9);
                    }
                }
            }else if (strstr (msg, "SETTOID") != 0){ //SETTOID [id] [interval1] [msg1] [interval2] [msg2]
                SetOptions ( msg, &myOpt);

                if ((child_pid [CurrId] = fork()) == -1)
                    DieWithError("fork error\n");
                else if (child_pid [CurrId] == 0) {
                    SendByTime(servSock, ClntAddr, CurrId, myOpt);
                    exit(0);
                }
            }
        }
    }
   
    close(servSock);
    exit(0);
}


int CreateUDPServerSocket ( unsigned short port ) {
    int sock;                        /* socket to create */
    struct sockaddr_in ServAddr; /* Local address */

    // Сокет для входящих UDP подключений 
    if (( sock = socket ( PF_INET, SOCK_DGRAM, IPPROTO_UDP )) < 0 )
        DieWithError("socket() failed");

    // Адрес сервера UDP
    memset(&ServAddr, 0, sizeof(ServAddr));   // очищаем структуру
    ServAddr.sin_family = AF_INET;                
    ServAddr.sin_addr.s_addr = htonl(INADDR_ANY); // Любой входящий интерфейс
    ServAddr.sin_port = htons(port);      /* Local port */

    // Привязываем UDP сокет к локальному адресу
    if (bind(sock, (struct sockaddr *) &ServAddr, sizeof(ServAddr)) < 0)
        DieWithError("bind() failed");
    
    return sock;
}

int SendByTime (int servSock,  struct sockaddr_in ClntAddr, int CurrId, ClntOpt myOpt){
    int timePass=0; 
    while(1){
        if (myOpt.interval1[CurrId-1] <= myOpt.interval2[CurrId-1]-timePass){
            sleep(myOpt.interval1[CurrId-1]);
            if (( sendto (servSock, myOpt.msg1[CurrId-1], strlen(myOpt.msg1[CurrId-1])+1, 0, (struct sockaddr *) &ClntAddr, sizeof(ClntAddr)) <0))
                DieWithError("send() failed");
            timePass += myOpt.interval1[CurrId-1];
            TRACE ( printf("Отправлено клиенту ID = %d, уведомление: %s\n", CurrId,  myOpt.msg1[CurrId-1]));
        } else {
            sleep(myOpt.interval2[CurrId-1]-timePass);
            if (( sendto (servSock, myOpt.msg2[CurrId-1], strlen(myOpt.msg2[CurrId-1])+1, 0, (struct sockaddr *) &ClntAddr, sizeof(ClntAddr)) <0))
                DieWithError("send() failed");
            timePass = 0;
            TRACE ( printf("Отправлено клиенту ID = %d, уведомление: %s\n", CurrId,  myOpt.msg2[CurrId-1]));
        }
    }
    return 0;
}

int ChooseId ( int id[], char msg[MAXMSG]) {
    int accepted =0;
    int CurrId=0;
        if (strstr(msg, "IDOFFER")!=NULL) {
            TRACE (printf("Получен запрос IDOFFER\n"));
            CurrId = atoi (strtok ( msg, " "));
            if (CurrId == 0)
                printf("Ошибка в IDOFFER\n");
        } else
            TRACE (printf("Получен запрос IDREQUEST\n"));
       
        for (int i = 0; i < MAXCLIENTS; ++i) {
            if (id[i]==CurrId && CurrId !=0){
                TRACE (printf("Принят предложенный клиентом ID %d\n", id[i]));
                accepted =1;
                break;
            }else if (id[i] == 0 && CurrId == 0){
                id[i]=i+1;
                CurrId = id [i];
                TRACE (printf("Клиенту предложен ID %d\n", id[i]));
                accepted =1;
                break;
            } 
            if (i == MAXCLIENTS -1 && accepted!= 1 ){
                for (int j = 0; j < MAXCLIENTS; ++j) {
                    if (id[j] == 0){
                        id[j]=j+1;
                        CurrId = id [j];
                        TRACE (printf("Предложенный клиентом ID не принят, присвоен ID %d\n", id[j]));
                        accepted =1;
                        break;
                    }
                }
            }
            if( i == MAXCLIENTS -1 && accepted ==0 )
                printf("Ошибка: достигнут максимум клиентов\n");
        }
    return CurrId;
}

void SetOptions ( char *msg, ClntOpt *myOpt){
    char *estr;
    estr = strtok(msg, " "); 
    
    estr = strtok(NULL, " ");
    int tmpid=atoi(estr);
    estr = strtok(NULL, " ");
    myOpt->interval1[tmpid-1]=atoi(estr);
    estr = strtok(NULL, " ");
    strcpy(myOpt->msg1[tmpid-1], estr);
    estr = strtok(NULL, " ");
    myOpt->interval2[tmpid-1]=atoi(estr);
    estr = strtok(NULL, " \0\n\t");
    strcpy(myOpt->msg2[tmpid-1], estr);
}

