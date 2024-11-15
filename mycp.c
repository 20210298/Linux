#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

int openFile(char* fileName, int flags) {
    int fd;

    fd = open(fileName, flags);
    if (fd <0){       
        perror("File Open Error");
        printf("Error number: %d\n", errno); 
        // perror와 printf의 순서를 바꿔도 똑같은 결과가 출력된다 (\n이 없을 때)
        exit(EXIT_FAILURE);
    }

    return fd;    
}

void setPermissionOfdest(char* src, char* dest) {
    struct stat src_stat;

    if(stat(src, &src_stat)<0){
        perror("chmod Error");
        exit(EXIT_FAILURE);
    }

    if(chmod(dest,src_stat.st_mode)<0){
        perror("chmod Error");
        exit(EXIT_FAILURE);
    }
}

void doCopy(char* src, char* dest){
    
    int src_fd, dest_fd;

    src_fd = openFile(src, O_RDONLY);
    dest_fd = openFile(dest, O_WRONLY | O_TRUNC | O_CREAT);

    setPermissionOfdest(src, dest);

    // copy 진행
    char buffer[1024];
    int contains;

    while ((contains = read(src_fd, buffer, sizeof(buffer))) > 0) {
        write(dest_fd, buffer, contains);
    }

    close(src_fd);
    close(dest_fd);
}

int main(int argc, char* argv[]) {

    int opt;

    while ((opt = getopt(argc, argv, "")) != -1) {
        printf("You cannot use this option\n");
        exit(EXIT_FAILURE);
    }

    switch(argc){
        case 1:
            printf("mycp: missing file operand\n");
            exit(EXIT_FAILURE);
        case 2:
            printf("mycp: missing destinantion file operand after '%s'\n", argv[1]);
            exit(EXIT_FAILURE);
        case 3:
            doCopy(argv[1], argv[2]);
            exit(EXIT_SUCCESS);
        default:
            printf("mycp: target '%s' is not a directory\n", argv[argc-1]);
            printf("Copying to directory is not supported\n");
            exit(EXIT_FAILURE);
    }
}