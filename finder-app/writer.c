#include <stdio.h>
#include <syslog.h>
int main(int argc, char *argv[]){
    openlog("writer",LOG_PID,LOG_USER);
    if(argc != 3){
        syslog(LOG_ERR,"Invalid number of arguments : %d",argc);
        return 1;
    }
    FILE *fptr = fopen(argv[1],"w");
    if(fptr == NULL){
        syslog(LOG_ERR,"Error opening file : %s",argv[1]);
        return 1;
    }
    syslog(LOG_DEBUG,"Writing %s to %s",argv[2],argv[1]);
    fprintf(fptr,"%s",argv[2]);
    fclose(fptr);
    closelog();
    return 0;
}