#include <stdlib.h>     /* for atoi() and exit() */
#include <stdio.h> 
#include <errno.h>
#include <string.h>

#include "mtrace.h"

void DieWithError ( char *errorMessage ) {
    perror(errorMessage);
    exit(1);
}

//Проверяет что строка имеет вид [int] [char*]
int TestIfStrCorrect ( char *str) {  
    char *tmp;
    tmp=strtok(str, " ");
    if (atoi(tmp)==0){
        printf("Некорректный интервал задержки\n");
        return -1;
    }

    if(strtok(NULL, " ")==NULL){
        printf("Необходимо пересылаемое сообщение\n");
        return -1;
    }
    return 0;
}