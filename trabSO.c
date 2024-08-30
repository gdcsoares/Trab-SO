#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

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

int main() {

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