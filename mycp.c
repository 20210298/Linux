#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>

int checkFileType(char* file){
    struct stat statbuf;

    if (stat(file, &statbuf) < 0) {
        if (errno == ENOENT) {
            return -1;
        } 
        else {
            perror("Stat error");
            exit(EXIT_FAILURE);
        }
    }

    switch (statbuf.st_mode & S_IFMT)
    {
    case (S_IFDIR): // 디렉터리
        return 0;  
    case (S_IFREG): // 일반 파일
        return 1;
    case (S_IFLNK): // 심볼릭 링크
        return 2;
    default:
        fprintf(stderr,"Unsupported file type\n"); 
        exit(EXIT_FAILURE);
    }
}

void setPermissionOfdest(char* src, char* dest) {
    struct stat src_stat;

    if (stat(src, &src_stat) < 0) {
        perror("Stat error on source file");
        exit(EXIT_FAILURE);
    }

    if(chmod(dest,src_stat.st_mode)<0){
        perror("Chmod Error");
        exit(EXIT_FAILURE);
    }
}

int openSymboliclink(char* symlink){
    char target[1024];
    int len;

    len = readlink(symlink, target, sizeof(target)-1);
    if(len <0){
        perror("Readlink Error");
        exit(EXIT_FAILURE);
    }

    target[len]='\0';

    if(checkFileType(target)!=1){
        printf("This is not a regular file\n");
        exit(EXIT_FAILURE);
    }

    int fd = openFile(target, O_RDONLY);
    return fd;
}

int openFile(char* fileName, int flags) {
    int fd;
    fd = open(fileName, flags);
    if (fd <0){ 
        return -1;       
    }

    return fd;    
}

DIR* openDir(char* dirName) {
    DIR* dir = opendir(dirName);
    if (!dir) {
        return 0;  
    }
    return dir;
}

void doCopy(char* src, char* dest){
    
    int src_fd, dest_fd;
    char buffer[1024];
    int contains;

// dest 타입 검사
    switch(checkFileType(dest)){        
        case -1:    // 파일이 없는 경우
            dest_fd = openFile(dest, O_WRONLY | O_TRUNC | O_CREAT);
            setPermissionOfdest(src, dest);   
            break;
        case 0: // 디렉터리인 경우
            // char newDestPath[1024];

            // 아래의 코드를 한 줄로 간단히 처리 가능
            // snprintf(newDestPath, sizeof(newDestPath), "%s/%s", dest, strrchr(src, '/') ? strrchr(src, '/') + 1 : src);

            char newDestPath[1024];
            char currentDir[1024];
            struct stat pathStat;

            // 현재 디렉터리 가져오기
            if (getcwd(currentDir, sizeof(currentDir)) == NULL) {
                perror("Failed to get current working directory");
                exit(EXIT_FAILURE);
            }

            // dest가 상대 경로이므로, 절대 경로로 변환
            if (strlen(currentDir) + strlen(dest) + 2 > sizeof(newDestPath)) {
                fprintf(stderr, "Error: Path is too long\n");
                exit(EXIT_FAILURE);
            }

            strcpy(newDestPath, currentDir);
            strcat(newDestPath, "/");
            strcat(newDestPath, dest);

            // 슬래시 추가 
            if (newDestPath[strlen(newDestPath) - 1] != '/') {
                strcat(newDestPath, "/");
            }

            // src 이름 추가
            strcat(newDestPath, strrchr(src, '/') ? strrchr(src, '/') + 1 : src);

            // 권한 검사
            if (!openDir(dest)) {
                fprintf(stderr, "mycp: cannot stat '%s/%s': ", dest, src);
                perror("");
                exit(EXIT_FAILURE);
            }

            // 파일 이름이 디렉터리로 존재하는지 확인
            if (stat(newDestPath, &pathStat) == 0 && S_ISDIR(pathStat.st_mode)) {
                fprintf(stderr, "mycp: cannot overwrite directory '%s/%s' with non-directory\n",dest,src);
                close(src_fd);
                exit(EXIT_FAILURE);
            }

            close(src_fd);
            doCopy(src, newDestPath);
            return;
        case 1:     //일반 파일인 경우
            dest_fd = openFile(dest, O_WRONLY | O_TRUNC);
            break;
        default:
            printf("Unsupported file type\n"); 
            exit(EXIT_FAILURE);  
    }

    // src 타입 검사 
    switch(checkFileType(src)){        
        case 0: // 디렉터리인 경우
            printf("mycp: -r not specified; omitting directory '%s'\n", src); 
            exit(EXIT_FAILURE);             
        case 1: // 일반 파일인 경우
            src_fd = openFile(src, O_RDONLY); 
            if (src_fd < 0) {
                fprintf(stderr, "mycp: cannot open '%s' for reading: ", src);
                perror("");
                exit(EXIT_FAILURE);
            }
            break; 
        case 2: // 심볼릭 파일인 경우
            src_fd = openSymboliclink(src);
            break;
        default:
            printf("Unsupported file type\n"); 
            exit(EXIT_FAILURE);  

    }  
    
    // 복사 
    while ((contains = read(src_fd, buffer, sizeof(buffer))) > 0) {
         if (write(dest_fd, buffer, contains) != contains) {
            perror("Write error");
            close(src_fd);
            close(dest_fd);
            exit(EXIT_FAILURE);
        }
    }

    if (contains < 0) {
        perror("Read error");
    }

    close(src_fd);
    close(dest_fd);
}

int main(int argc, char* argv[]) {
    
    int opt;

    while ((opt = getopt(argc, argv, "")) != -1) {
        printf("Unsupported options\n");
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
            if (checkFileType(argv[argc-1])){
                printf("mycp: target '%s' is not a directory\n",argv[argc-1]);
                exit(EXIT_FAILURE);
            }
            for(int i=1;i<argc-1;++i){
                doCopy(argv[i],argv[argc-1]);
            }
            exit(EXIT_SUCCESS);
    }
}