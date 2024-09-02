#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#define MAX_BUFFER 1024
#define MAX_COMMANDS 5
#define MAX_PROCESSES 5

pid_t fg_process_pid = 0;
pid_t bg_processes[MAX_PROCESSES];
int num_bg_processes = 0;


void handle_sigint(int sig) {
 
    if (num_bg_processes > 0 || fg_process_pid != 0) {
     
        printf("\nVocê tem certeza que deseja finalizar a shell? (y/n): ");
        char c = getchar();
        if (c == 'y' || c == 'Y') {
            printf("Finalizando shell...\n");
            exit(0);
        } else {
            printf("Continuando shell...\n");

            while (getchar() != '\n');
        }
    } else {
        printf("Finalizando shell...\n");
        exit(0);
    }
}

void handle_sigtstp(int sig) {
    printf("\nRecebido SIGTSTP, suspendendo processos filhos...\n");

    if (fg_process_pid != 0) {
        kill(fg_process_pid, SIGSTOP);
    }

    for (int i = 0; i < num_bg_processes; i++) {
        if (bg_processes[i] != 0) {
            kill(bg_processes[i], SIGSTOP);
        }
    }
}

void execute_command(char *command) {

    if (strcmp(command, "die") == 0) {
        
        if (fg_process_pid != 0) {
            kill(fg_process_pid, SIGKILL);
        }

        for (int i = 0; i < num_bg_processes; i++) {
            if (bg_processes[i] != 0) {
                kill(bg_processes[i], SIGKILL);
            }
        }

        exit(0);

        return;

    } else if (strcmp(command, "waitall") == 0) {

        while (1) {
            int status;
            pid_t pid = waitpid(-1, &status, WNOHANG);

            if (pid <= 0) {
                if (pid == -1 && errno == EINTR) {
                    continue;
                }
                break;
            }
        }

        return;
    }

}

int main() {

    struct sigaction sa_int;
    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = handle_sigint;
    sigfillset(&sa_int.sa_mask);
    if (sigaction(SIGINT, &sa_int, NULL) == -1) {
        perror("Erro ao configurar o handler para SIGINT");
        return 1;
    }

    struct sigaction sa_tstp;
    memset(&sa_tstp, 0, sizeof(sa_tstp));
    sa_tstp.sa_handler = handle_sigtstp;
    sigfillset(&sa_tstp.sa_mask);
    if (sigaction(SIGTSTP, &sa_tstp, NULL) == -1) {
        perror("Erro ao configurar o handler para SIGTSTP");
        return 1;
    }

    char buffer[MAX_BUFFER];

    while (1) {

        printf("fsh> "); 
        fflush(stdout);

        if (fgets(buffer, MAX_BUFFER, stdin) == NULL) {
            perror("Erro ao ler o comando");
            continue;
        }

        buffer[strcspn(buffer, "\n")] = '\0';

        char *commands[MAX_COMMANDS] = { NULL };
        char *token = strtok(buffer, "#");
        int cmd_count = 0;

        while (token && cmd_count < MAX_COMMANDS) {
            commands[cmd_count++] = token;
            token = strtok(NULL, "#");
        }

        if (cmd_count > 0) {
            execute_command(commands[0]); 
        
            for (int i = 1; i < cmd_count; i++) {
                execute_background(commands[i]);
            }
        }

    }
}