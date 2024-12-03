#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <error.h>
#include <ctype.h>

typedef struct CMD
{
    char *name;
    char *desc;
    int (*cmd)(int argc, char *argv[]);
} CMD;

int cmd_cd(int argc, char *arv[]);
int cmd_pwd(int argc, char *arv[]);
int cmd_exit(int argc, char *arv[]);
int cmd_help(int argc, char *arv[]);
int cmd_history(int argc, char *arv[]);
int cmdProcessing(void);
void add_history(char *command);
int cmd_test(int argc, char *arv[]);

CMD builtin[] = {
    {"cd", "작업 디렉터리 바꾸기", cmd_cd},
    {"pwd", "현재 작업 디렉터리는?", cmd_pwd},
    {"exit", "셸 실행을 종료합니다", cmd_exit},
    {"help", "도움말 보여 주기", cmd_help},
    {"history", "명령어 기록 보여 주기", cmd_history},
    {"hello", "테스트", cmd_test}};
const int builtins = sizeof(builtin) / sizeof(CMD);

int main(void)
{
    int isExit = 0;

    while (!isExit)
        isExit = cmdProcessing();
    fputs("My Shell을 종료합니다\n", stdout);
    return 0;
}

#define STR_LEN 1024
#define MAX_TOKENS 128
#define MAX_LINE 5

// 기록을 저장할 큐
typedef struct HISTORY
{
    int num;
    char command[STR_LEN];
} HISTORY;

HISTORY history[MAX_LINE];
int head = 0;
int count = 0;
int total_count = 0;

void add_history(char *command)
{
    int currentIdx = (head + count) % MAX_LINE;
    strncpy(history[currentIdx].command, command, STR_LEN - 1);
    history[currentIdx].command[STR_LEN - 1] = '\0';
    history[currentIdx].num = ++total_count;

    if (count == MAX_LINE)
    {
        head = (head + 1) % MAX_LINE;
    }
    else
    {
        count++;
    }
}

#include <stdlib.h> // strtol 사용

int is_number(const char *str)
{
    char *endptr;

    if (str == NULL || *str == '\0')
        return 0;

    strtol(str, &endptr, 10);
    return *endptr == '\0';
}

int cmd_history(int argc, char *argv[])
{
    int iter;

    if (argc == 1)
    {
        iter = count;
    }
    else if (argc == 2)
    {
        if (argv[1][0] == '-')
        {
            fprintf(stderr, "옵션 지원 안함\n");
            return 0;
        }
        else if (is_number(argv[1]))
        {
            int requested = atoi(argv[1]);
            if (requested > count)
            {
                iter = count;
            }
            else if (requested >= 0)
            {
                iter = requested;
            }
        }
        else
        {
            fprintf(stderr, "history: %s: numeric argument required\n", argv[1]);
            return 0;
        }
    }
    else
    {
        fprintf(stderr, "history: too many arguments\n");
        return 0;
    }

    for (int i = 0; i < iter; ++i)
    {
        int idx = (head + i) % MAX_LINE;
        printf("%3d %s\n", history[idx].num, history[idx].command);
    }
    return 0;
}

int cmdProcessing(void)
{
    char cmdLine[STR_LEN];
    char *cmdTokens[MAX_TOKENS];
    char delim[] = " \t\n\r";
    char *token;
    int tokenNum;

    int exitCode = 0;
    fputs("[mysh v0.1] $ ", stdout);
    fgets(cmdLine, STR_LEN, stdin);
    add_history(cmdLine);

    tokenNum = 0;
    token = strtok(cmdLine, delim);

    while (token)
    {
        cmdTokens[tokenNum++] = token;
        token = strtok(NULL, delim);
    }

    cmdTokens[tokenNum] = NULL;
    if (tokenNum == 0)
        return exitCode;
    for (int i = 0; i < builtins; ++i)
    {
        if (strcmp(cmdTokens[0], builtin[i].name) == 0)
            return builtin[i].cmd(tokenNum, cmdTokens);
    }

    pid_t pid;

    pid = fork();
    if (pid > 0)
    {
        int status; // 포인터로 설정하지 않아도 된다
        waitpid(pid, &status, 0);
    }
    else if (pid == 0)
    {
        // 명령어, 옵션을 넘김
        execvp(cmdTokens[0], cmdTokens);

        // 실패 시 -1 반환 성공 시 무반환
        perror("Command Error");
        exit(1);
    }
    else
    {
        perror("Create Child Process Error");
    }

    return exitCode;
}

int cmd_cd(int argc, char *argv[])
{
    if (argc < 2)
    {
        char *home;
        home = getenv("HOME");

        // 홈 디렉터리가 없을 수 있나? 생각했지만 환경 변수를 수정했다면 충분히 가능
        if (home == NULL)
        {
            perror("NO HOME");
            return 0;
        }
        // 환경 변수에 다른 home을 설정했다면 오류 발생 가능
        if (chdir(home) != 0)
        {
            perror("cd");
            return 0;
        }
    }
    else if (argc == 2)
    {
        if (argv[1][0] == '-')
        {
            fprintf(stderr, "옵션 지원 안함\n");
            return 0;
        }
        if (chdir(argv[1]) != 0)
        {
            perror("cd");
            return 0;
        }
    }
    else
    {
        fprintf(stderr, "cd: too many argumnets\n");
        return 0;
    }

    return 0;
}

int cmd_pwd(int argc, char *argv[])
{
    char buffer[STR_LEN];
    if (getcwd(buffer, sizeof(buffer)) == NULL)
    {
        // 현재 디렉터리에 권한이 없거나와 같은 오류
        perror("pwd");
        return 0;
    }

    if (argc >= 2)
    {
        if (argv[1][0] == '-')
        {
            // 2번째 인자로 옵션 오는 경우는 작동함
            fprintf(stderr, "옵션 지원 안함\n");
            return 0;
        }
    }

    printf("%s\n", buffer);
    return 0;
}

int cmd_exit(int argc, char *argvp[])
{
    return 1;
}

int cmd_help(int argc, char *argv[])
{
    if (argc == 1)
    {
        for (int i = 0; i < builtins; ++i)
        {
            printf("%-10s : %s\n", builtin[i].name, builtin[i].desc);
        }
    }
    else if (argc >= 2)
    {
        if (argv[1][0] == '-')
        {
            fprintf(stderr, "옵션 지원 안함\n");
            return 0;
        }

        for (int j = 1; j < argc; ++j)
        {
            int isFind = 0;

            if (argv[j][0] == '-')
            {
                continue;
                ;
            }

            for (int i = 0; i < builtins; ++i)
            {
                if (!strncmp(argv[j], builtin[i].name, strlen(argv[j])))
                {
                    printf("%-10s : %s\n", builtin[i].name, builtin[i].desc);
                    isFind = 1;
                }
            }
            if (!isFind)
            {
                fprintf(stderr, "help: no help topics match '%s'.\n", argv[1]);
            }
        }
    }

    return 0;
}

int cmd_test(int argc, char *arv[])
{
    return 0;
}