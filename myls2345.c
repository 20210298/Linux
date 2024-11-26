#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <pwd.h>
#include <string.h>
#include <grp.h>
#include <time.h>

// 파일 권한을 문자열로 변환
void getPermissions(mode_t mode, char *perm) {
    perm[0] = (S_ISDIR(mode)) ? 'd' :
              (S_ISLNK(mode)) ? 'l' :
              (S_ISCHR(mode)) ? 'c' :
              (S_ISBLK(mode)) ? 'b' :
              (S_ISSOCK(mode)) ? 's' :
              (S_ISFIFO(mode)) ? 'p' : '-';
    perm[1] = (mode & S_IRUSR) ? 'r' : '-';
    perm[2] = (mode & S_IWUSR) ? 'w' : '-';
    perm[3] = (mode & S_IXUSR) ? 'x' : '-';
    perm[4] = (mode & S_IRGRP) ? 'r' : '-';
    perm[5] = (mode & S_IWGRP) ? 'w' : '-';
    perm[6] = (mode & S_IXGRP) ? 'x' : '-';
    perm[7] = (mode & S_IROTH) ? 'r' : '-';
    perm[8] = (mode & S_IWOTH) ? 'w' : '-';
    perm[9] = (mode & S_IXOTH) ? 'x' : '-';
    perm[10] = '\0'; // 문자열 종료
}

char checkFileType(char *fileName, char *dirName) {
    struct stat statbuf;
    char fullPath[1024];

    // 전체 경로 생성
    snprintf(fullPath, sizeof(fullPath), "%s/%s", dirName, fileName);

    // 심볼릭 링크 자체 정보를 가져오기 위해 lstat 사용
    if (lstat(fullPath, &statbuf) < 0) {
        perror("Stat error");
        exit(EXIT_FAILURE);
    }

    // 파일 타입 판별
    switch (statbuf.st_mode & S_IFMT) {
    case S_IFDIR: // 디렉터리
        return '/';
    case S_IFREG: // 일반 파일
        if (statbuf.st_mode & S_IXUSR) { // 실행 파일
            return '*'; 
        }
        return ' '; 
    case S_IFLNK: // 심볼릭 링크
        return '@';
    case S_IFSOCK: // 소켓
        return '=';
    case S_IFIFO: // FIFO 파이프
        return '|';
    case S_IFCHR: // 문자 장치 파일
    case S_IFBLK: // 블록 장치 파일
        return '>';
    default:
        fprintf(stderr, "Unsupported file type\n");
        exit(EXIT_FAILURE);
    }
}


blksize_t getFileSize(char* fileName, char* dirName){
    struct stat statbuf;
    char fullPath[1024];

    snprintf(fullPath, sizeof(fullPath), "%s/%s", dirName, fileName);
     if (lstat(fullPath, &statbuf) == 0) {
        if (S_ISLNK(statbuf.st_mode)) {
            return 0;   // 심볼릭 링크 0
        }
        return statbuf.st_blocks / 2; 
    }    
     else {
        return 0;
    }
} 

DIR* openDir(char* dirName) {
    DIR* dir = opendir(dirName);

    if (dir == NULL && errno==13) { // 권한 오류만 처리
        fprintf(stderr,"myls: cannot open directory '%s': ",dirName);
        perror("");
    }

    return dir;
}

blksize_t getTotal(char *dirName, int flags[]){
    blksize_t total = 0;
    struct dirent *direntry;

    DIR* dir=openDir(dirName);

    if (dir == NULL) {
        return;
    }

    while ((direntry = readdir(dir)) != NULL) {
        if (flags[0] || direntry->d_name[0] != '.') { 
            total += getFileSize(direntry->d_name, dirName); // 블록 크기 합산
        }
    }
    closedir(dir);
    return total;
}

void operateLOption(char *fileName, char *dirName, int flags[]){
    struct stat statbuf;
    char fullPath[1024];
    char perm[11];
    struct passwd *pwd;
    struct group *grp;
    char timeStr[32];

    snprintf(fullPath, sizeof(fullPath), "%s/%s", dirName, fileName);

    if (lstat(fullPath, &statbuf) < 0) {
        perror("lstat error");
        return;
    }

    // 권한
    getPermissions(statbuf.st_mode, perm);

    // 링크 수
    printf("%s %ld ", perm, statbuf.st_nlink);

    // 소유자와 그룹
    pwd = getpwuid(statbuf.st_uid);
    grp = getgrgid(statbuf.st_gid);
    printf("%s %s ", pwd ? pwd->pw_name : "unknown", grp ? grp->gr_name : "unknown");

    // 파일 크기
    printf("%5ld ", statbuf.st_size);

    // 마지막 수정 시간
    char *modTime = ctime(&statbuf.st_mtime);
    if (modTime) {
        modTime[strcspn(modTime, "\n")] = '\0'; // 개행 문자 제거
        printf("%.12s ", modTime + 4);
    } else {
        perror("ctime error");
    }

    // 파일 이름
    printf("%s", fileName);

    // 심볼릭 링크 처리
    if (S_ISLNK(statbuf.st_mode)) {
        char linkTarget[1024];
        ssize_t len = readlink(fullPath, linkTarget, sizeof(linkTarget) - 1);
        if (len != -1) {
            linkTarget[len] = '\0';
            printf(" -> %s", linkTarget); 

            // 링크 대상의 파일 타입 확인
            struct stat targetStat;
            if (stat(linkTarget, &targetStat) == 0) {
                if (flags[3]) { // -F 옵션
                    if (S_ISDIR(targetStat.st_mode)) {
                        printf("/");
                    } else if (S_ISREG(targetStat.st_mode) && (targetStat.st_mode & S_IXUSR)) {
                        printf("*");
                    } else if (S_ISSOCK(targetStat.st_mode)) {
                        printf("=");
                    } else if (S_ISFIFO(targetStat.st_mode)) {
                        printf("|");
                    } else if (S_ISCHR(targetStat.st_mode) || S_ISBLK(targetStat.st_mode)) {
                        printf(">");
                    }
                }
            }
        }  
    } else if(flags[3]){
        printf("%c", checkFileType(fileName, dirName));
    }

     printf("\n");
}

int reverse_alphasort(const struct dirent **a, const struct dirent **b){
    return strcmp((*b)->d_name, (*a)->d_name);
}

void doLs(char *dirName, int flags[]) {
    struct dirent **namelist;
    int n;

    // -r 옵션
    if(flags[6]){
        n = scandir(dirName, &namelist, NULL, reverse_alphasort);
    } else{
        n = scandir(dirName, &namelist, NULL, alphasort);
    }

    if (n < 0) {
        fprintf(stderr, "myls: cannot open directory '%s': ",dirName);
        perror("");
        return;
    }

    // -R 옵션
    if (flags[5]) {
        printf("%s:\n", dirName);
    }

    // -s 또는 -l 옵션일 때 total 출력
    if (flags[2] || flags[4]) {
        blksize_t totalBlocks = getTotal(dirName, flags);
        printf("total %ld\n", totalBlocks);
    }

    // 디렉터리 항목 출력
    for (int i = 0; i < n; i++) {
        if (!flags[0] && namelist[i]->d_name[0] == '.') {   // a 옵션
            free(namelist[i]);
            continue;
        }

        char fullPath[1024];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", dirName, namelist[i]->d_name);

        if(flags[1]){   // i 옵션
                printf("%ld ", namelist[i]->d_ino);
            }
        if(flags[2]){   // s 옵션
                printf(" %ld ", getFileSize(namelist[i]->d_name, dirName));
        }

        if (flags[4]) { // -l 옵션
            operateLOption(namelist[i]->d_name, dirName, flags);
        } else {
            printf("%s", namelist[i]->d_name);
            if (flags[3]) { // -F 옵션
                printf("%c  ", checkFileType(namelist[i]->d_name, dirName));
            } else {
                printf("  ");
            }
        }
        free(namelist[i]); 
    }

    if(!flags[4]){
        printf("\n");
    }

    free(namelist); 

    // -R 옵션
    if (flags[5]) {
        n = scandir(dirName, &namelist, NULL, alphasort);
        for (int i = 0; i < n; i++) {
            if (!flags[0] && namelist[i]->d_name[0] == '.') {
                free(namelist[i]);
                continue;
            }

            char fullPath[1024];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", dirName, namelist[i]->d_name);

            struct stat statbuf;
            if (lstat(fullPath, &statbuf) < 0) {
                perror("lstat error");
                free(namelist[i]);
                continue;
            }

            if (S_ISDIR(statbuf.st_mode) &&
                strcmp(namelist[i]->d_name, ".") != 0 &&
                strcmp(namelist[i]->d_name, "..") != 0) {
                printf("\n");
                doLs(fullPath, flags);
            }

            free(namelist[i]);
        }

        free(namelist);
    }
}



int main(int argc, char* argv[]){
    int opt;
    int optflags[7]={0};    // 순서대로 a, i, s, F, l, R, r

    while ((opt = getopt(argc, argv, "aisFlRr")) != -1) {
        switch(opt){
            case 'a':   
                optflags[0]=1;
                break;
            case 'i':  
                optflags[1]=1;
                break;
            case 's':
                optflags[2]=1;
                break;
            case 'F':
                optflags[3]=1;
                break;
            case 'l':
                optflags[4]=1;
                break;
            case 'R':
                optflags[5]=1;
                break;
            case 'r':
                optflags[6]=1;
                break;
            default:
                printf("Unsupported options\n");
                exit(EXIT_FAILURE);
        }        
    }

    if ((argc - optind) == 0) {
        doLs(".",optflags);
    } else {
        for(int i = optind; i<argc; ++i){            
            if(openDir(argv[i]) != NULL) {
                printf("%s:\n",argv[i]);
                doLs(argv[i],optflags);
            }
                   
        }
    }
    exit(EXIT_SUCCESS);  
}