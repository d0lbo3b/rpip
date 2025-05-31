#include <stdio.h>
#include <direct.h>
#include "shared.h"

int main(){
    char process_name_to_check[256];
    char process_path_to_run[256];
    char process_name_to_run[256];
    char rpip_path[256];

    printf("Name of Process to Check: ");
    scanf("%255s", &process_name_to_check);
    printf("Name of Process to Run: ");
    scanf("%255s", &process_name_to_run);
    printf("Path to Process to Run: ");
    scanf("%255s", &process_path_to_run);

    FILE* fd_i = fopen("service_install.bat", "w+");
    FILE* fd_d = fopen("service_delete.bat", "w+");

    if (fd_i == NULL || fd_d == NULL){
        perror("Error openning file");
        return 1;
    }

    if (_getcwd(rpip_path, sizeof(rpip_path)) != NULL){
        sprintf(rpip_path, "%s%s", rpip_path, "\\rpip.exe");
    } else {
        perror("_getcwd() error");
        return 1;
    }

    fprintf(
        fd_i,
        "@echo off\n"
        "sc create \"%s\" binPath= \"\\\"%s\\\" %s \\\"%s\\\" %s\" start= auto\n"
        "sc start %s\n"
        "pause\n",
        SERVICE_NAME,
        rpip_path,
        process_name_to_check,
        process_path_to_run,
        process_name_to_run,
        SERVICE_NAME
    );

    fprintf(
        fd_d,
        "@echo off\n" 
        "sc stop %s\n"
        "sc delete %s\n"
        "echo Service has been deleted\n"
        "pause\n",
        SERVICE_NAME
    );

    fclose(fd_i);
    fclose(fd_d);

    return 0;
}