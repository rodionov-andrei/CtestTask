#define MAX_STRING_LEN 50
#define MAXCLIENTS 5 // максимальное количество клиентов
#define MAXMSG 100     /* Максимальная длина сообщения */

typedef struct  {
    int interval1 [MAXCLIENTS];
    int interval2 [MAXCLIENTS];
    char msg1 [MAXCLIENTS][MAXMSG];
    char msg2 [MAXCLIENTS][MAXMSG];
}ClntOpt;
