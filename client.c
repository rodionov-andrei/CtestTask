#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), connect(), sendto(), and recvfrom() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <sys/types.h>  /* for open*/
#include <sys/stat.h>
#include <signal.h>     /*for kill*/
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

#include "main.h"
#include "mtrace.h"

extern void DieWithError ( char *errorMessage );
extern int TestIfStrCorrect ( char *str);
int CheckIdInfile ( );
void WriteIdTofile ( int id );
void Disconnect (int sock,  struct sockaddr_in ServAddr, int id, int parent_pid);
void SendOptions (int sock,  struct sockaddr_in ServAddr, int id);
void SetDefaultConfig ();

int main(int argc, char *argv[]) {
    int sock;                        
    struct sockaddr_in ServAddr;    /* Адрес сервера */
    struct sockaddr_in fromAddr;     /* Наш адрес */
    unsigned short ServPort;     /* Порт сервера */
    unsigned int fromSize;           /* размер адреса для recvfrom() */
    char *servIP;                    /* IP сервера */

    /* Проверяем что пользователь ввел правильное количество аргументов */
    if (argc != 3) {   
        fprintf(stderr,"Usage: %s <Server IP> <Echo Port>\n", argv[0]);
        exit(1);
    }

    servIP = argv[1];           
    ServPort = atoi(argv[2]);  
   
    /* создаем UDP сокет */
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() failed");

    /* заполняем адрес сервера */
    memset(&ServAddr, 0, sizeof(ServAddr));    
    ServAddr.sin_family = AF_INET;                 
    ServAddr.sin_addr.s_addr = inet_addr(servIP);  
    ServAddr.sin_port   = htons(ServPort);     

    /*Регистрация на сервере*/
    int id; // наш id, по нему сервер различает клиентов
    printf("Создать файл с настройками по умолчанию? (y/n)\n");
    int c = getchar();
    if (c=='y')
        SetDefaultConfig();
    
    id = CheckIdInfile(); // у нас уже есть ID? 
    char msg[MAXMSG];

    if (id == 0){ // если у нас еще нет ID, то делаем запрос на его получение
        /* Send the IDREQUEST to the server */
        if (sendto(sock, "IDREQUEST\0", 10, 0, (struct sockaddr *) &ServAddr, sizeof(ServAddr)) <0 )
            DieWithError("sendto() sent a different number of bytes than expected");
        TRACE (printf("Отослали IDREQUEST\n"));
    } else { // если есть ID то пересылаем его
        sprintf (msg, "%d", id);
        strcat (msg, " IDOFFER");
        if (sendto(sock, msg, strlen(msg), 0, (struct sockaddr *) &ServAddr, sizeof(ServAddr)) <0 )
            DieWithError("sendto() sent a different number of bytes than expected");
        TRACE (printf("Отослали IDOFFER %d\n", id));
    }
            
    /*Получаем ID от сервера*/
    fromSize = sizeof(fromAddr);
    if ((recvfrom(sock, msg, strlen(msg)+1, 0, (struct sockaddr *) &fromAddr, &fromSize )) < 0)
        DieWithError("recvfrom() failed");
    id = atoi (msg);
    TRACE(printf("Получили id %d\n", id));
    WriteIdTofile(id);

    /*подвтерждаем получение*/
    if (sendto(sock, "ACK\0", 4, 0, (struct sockaddr *) &ServAddr, sizeof(ServAddr)) <0 )
        DieWithError("sendto() sent a different number of bytes than expected");
    /*завершили регистрацию*/

    SendOptions (sock, ServAddr, id);
    /*дочерний процесс, ожидает нажатия Enter, чтобы закрыть клиента, отключиться от сервера и записать в файл id = 0*/
    int parent_pid = getpid(), child_pid, ch;
    if ((child_pid = fork()) == -1)
        DieWithError("fork error\n");
    else if (child_pid == 0) {
        printf("Нажмите 'q' и 'Enter' для выхода\n");
        while(1){
            ch = getchar();
            if (ch == 'q')
                Disconnect (sock, ServAddr, id, parent_pid);         
        }
    }

    /* прием сообщений от сервера и подтверждения */
    while(1){
        fromSize = sizeof(fromAddr);
        if ((recvfrom(sock, msg, MAXMSG, 0, (struct sockaddr *) &fromAddr, &fromSize)) < 0)
            DieWithError("recvfrom() failed");

        printf("Получили уведомление: %s\n", msg);
    }


    close(sock);
    exit(0);
}

int CheckIdInfile ( ) {  
    int id; // идентефикатор клиента
    int fd; // дискриптор файла с логом
    fd = open("./config", O_RDONLY | O_CREAT, 0666);
    if (fd < 0)
        perror("Open file");
    
    char buf[2];
    if (read (fd, buf, 1 )<0)
        perror("read id");
    buf[1]='\0';

    id = atoi (buf);
    return id;
}

void WriteIdTofile ( int id ) {
    int fd; // дискриптор файла с логом
        fd = open("./config", O_WRONLY | O_CREAT, 0666);
        if (fd < 0)
            perror("Open file");
    char str[30];
    sprintf (str, "%d", id);

    int nwritten;
    nwritten = write(fd, str, strlen(str));

    if (nwritten < 0 && errno != EINTR)
        perror("Write id to file");
}

void Disconnect (int sock,  struct sockaddr_in ServAddr, int id, int parent_pid) {
    char msg[30];
    sprintf (msg, "%d", id);
    strcat(msg, " EXIT");
    /*отправили серверу запрос на отключение*/
    if (sendto(sock, msg, strlen(msg)+1, 0, (struct sockaddr *) &ServAddr, sizeof(ServAddr)) <0 )
        DieWithError("sendto() sent a different number of bytes than expected");

    TRACE (printf("Отправлен запрос на отключение %s\n", msg));
    close (sock);
    WriteIdTofile(0);
    printf("Disconnected\n");
    kill (-parent_pid, SIGTERM ); // завершили дерево процессов
}

void SendOptions (int sock,  struct sockaddr_in ServAddr, int id) {
    FILE *mf;
    mf = fopen ("config","r");
    char tmpid[2];
    if (mf == NULL)
      DieWithError("ошибка открытия файла\n"); 

    char *estr;
    char *str = (char *)malloc(sizeof(char *)*MAX_STRING_LEN); // сюда поочередно складываем считанные с входного файла строки
    char *msg = (char *)malloc(sizeof(char *)*MAX_STRING_LEN); // строка для отправки
        strcat (msg, "SETTOID ");
        sprintf (tmpid, "%d", id);
        strcat(msg, tmpid);
    estr = fgets (str,MAX_STRING_LEN,mf); // первая строка не нужна
    int i=0; // счетчик строк кода
    while ( i< 2 ){
        estr = fgets (str,MAX_STRING_LEN,mf);
        //Проверка на конец файла или ошибку чтения
        if (estr == NULL){
            if ( feof (mf) != 0){  //файл закончился
                DieWithError("Файл конфигурации содержит <3 строк");
                break;
            }else{ //Если при чтении произошла ошибка, выводим сообщение  об ошибке и выходим из бесконечного цикла
                DieWithError("\nОшибка чтения из файла\n");
                break;
            }
        }
       // if (str[strlen(str)-1]=='\n') // меняем перенос строки на конец строки
        str[strlen(str)-1]='\0';
        i++;
        
        strcat(msg, " ");
        strcat(msg, str);
        TestIfStrCorrect (str);
    }
    if (sendto(sock, msg, strlen(msg)+1, 0, (struct sockaddr *) &ServAddr, sizeof(ServAddr)) <0 )
        DieWithError("sendto() sent a different number of bytes than expected");
    free(str);
    free(msg);
}

void SetDefaultConfig (){
    FILE * fd;
    fd = fopen ("config","w");
    fprintf(fd, "0\n");
    fprintf(fd, "3 Разминка_для_глаз\n");
    fprintf(fd, "10 Обед\n");

    fprintf(fd, "#1ая строка - id, 2ая, 3ая - интервал и сообщение\n");
    fclose (fd);
}




