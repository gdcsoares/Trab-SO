#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>

#define MAX_BUFFER 1024
#define MAX_COMMANDS 5
#define MAX_PROCESSES 100

pid_t fg_process_pid = 0;
pid_t bg_processes[MAX_PROCESSES];
int num_bg_processes = 0;

void handle_sigint(int sig) {
    (void)sig; // Marcar o parâmetro como utilizado para evitar avisos
    printf("\nRecebido SIGINT\n");

    if (num_bg_processes > 0 || fg_process_pid != 0) {
        printf("Você tem certeza que deseja finalizar a shell? (y/n): ");
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
    printf("fsh> "); // Imprimir prompt após manipulação de SIGINT
    fflush(stdout);
}

void handle_sigtstp(int sig) {
    (void)sig; // Marcar o parâmetro como utilizado para evitar avisos
    printf("\nRecebido SIGTSTP, suspendendo processos...\n");

    if (fg_process_pid != 0) {
        kill(fg_process_pid, SIGSTOP);
    }

    for (int i = 0; i < num_bg_processes; i++) {
        if (bg_processes[i] != 0) {
            kill(bg_processes[i], SIGSTOP);
        }
    }
    sleep(1);
    printf("fsh> "); // Imprimir prompt após manipulação de SIGTSTP
    fflush(stdout);
}

void execute_background(char *command) {
    // Remover espaços extras do comando
    while (*command == ' ') command++;
    char *end = command + strlen(command) - 1;
    while (end > command && *end == ' ') end--;
    *(end + 1) = '\0';

    pid_t pid = fork();

    if (pid < 0) {
        perror("Erro no fork");
        return;
    }

    if (pid == 0) {  // Processo filho (background)
        signal(SIGINT, SIG_IGN); // Ignorar SIGINT
        pid_t child_pid = fork();

        if (child_pid < 0) {
            perror("Erro no fork do processo secundário");
            exit(1);
        }

        if (child_pid == 0) {  // Processo secundário (Px')
            // printf("Processo secundário '%s' iniciado (PID=%d)\n", command, getpid());
            char *args[] = { "/bin/sh", "-c", command, NULL };
            execvp(args[0], args);
            perror("Erro ao executar comando no processo secundário");
            exit(1);
        } else {
            // printf("Processo '%s' iniciado em background (PID=%d)\n", command, getpid());
            char *args[] = { "/bin/sh", "-c", command, NULL };
            execvp(args[0], args);
            perror("Erro ao executar comando em background");
            exit(1);
        }
    } else {
        if (num_bg_processes < MAX_PROCESSES) {
            bg_processes[num_bg_processes++] = pid;
        } else {
            printf("Número máximo de processos em background atingido\n");
        }
    }
}

int execute_command(char *command) {
    // Remover espaços extras do comando
    while (*command == ' ') command++;
    char *end = command + strlen(command) - 1;
    while (end > command && *end == ' ') end--;
    *(end + 1) = '\0';

    if (strcmp(command, "die") == 0) {
        printf("Comando 'die' recebido. Finalizando todos os processos...\n");

        if (fg_process_pid != 0) {
            kill(fg_process_pid, SIGKILL);
        }

        for (int i = 0; i < num_bg_processes; i++) {
            if (bg_processes[i] != 0) {
                kill(bg_processes[i], SIGKILL);
            }
        }

        exit(0);
        return 1; // Comando interno

    } else if (strcmp(command, "waitall") == 0) {
        printf("Aguardando todos os processos filhos...\n");

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
        return 1; // Comando interno

    } else { // Executa comando em foreground
        pid_t pid = fork();

        if (pid < 0) {
            perror("Erro no fork");
            return 0;
        }

        if (pid == 0) { // Processo filho (foreground)
            signal(SIGINT, SIG_IGN); // Ignorar SIGINT
            char *args[] = { "/bin/sh", "-c", command, NULL };
            execvp(args[0], args);
            perror("Erro ao executar comando em foreground");
            exit(1);
        } else { // Processo pai
            fg_process_pid = pid;
            waitpid(pid, NULL, 0);
            fg_process_pid = 0;
        }
        return 0; // Não é comando interno
    }
}

int main() {
    struct sigaction sa_int, sa_tstp;
    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = handle_sigint;
    sa_int.sa_flags = SA_RESTART; // Reiniciar chamadas de sistema interrompidas
    sigfillset(&sa_int.sa_mask);
    sigaction(SIGINT, &sa_int, NULL);

    memset(&sa_tstp, 0, sizeof(sa_tstp));
    sa_tstp.sa_handler = handle_sigtstp;
    sa_tstp.sa_flags = SA_RESTART; // Reiniciar chamadas de sistema interrompidas
    sigfillset(&sa_tstp.sa_mask);
    sigaction(SIGTSTP, &sa_tstp, NULL);

    char buffer[MAX_BUFFER];

    while (1) {
        printf("fsh> ");
        fflush(stdout);

        if (fgets(buffer, MAX_BUFFER, stdin) == NULL) {
            if (errno == EINTR) {
                continue; // Reiniciar leitura se for interrompida por um sinal
            }
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
            int is_internal = execute_command(commands[0]);

            for (int i = 1; i < cmd_count; i++) {
                if (!is_internal) {
                    execute_background(commands[i]);
                }
            }
        }

        // Verificar periodicamente a conclusão dos processos em background
        int status;
        int prompt_printed = 0; // Flag para evitar múltiplas impressões do prompt
        for (int i = 0; i < num_bg_processes; i++) {
            if (bg_processes[i] != 0) {
                pid_t result = waitpid(bg_processes[i], &status, WNOHANG);
                if (result == 0) {
                    // Processo ainda está em execução
                    continue;
                } else if (result == -1) {
                    perror("Erro ao esperar pelo processo em background");
                } else {
                    // Processo terminou
                    // printf("Processo em background (PID=%d) terminou\n", bg_processes[i]);
                    bg_processes[i] = 0; // Resetar o PID após a conclusão
                    if (!prompt_printed) {
                        // printf("fsh> "); // Imprimir prompt após a conclusão do processo em background
                        fflush(stdout);
                        prompt_printed = 1;
                    }
                }
            }
        }

        // Compactar a lista de processos em background
        int j = 0;
        for (int i = 0; i < num_bg_processes; i++) {
            if (bg_processes[i] != 0) {
                bg_processes[j++] = bg_processes[i];
            }
        }
        num_bg_processes = j; // Atualizar o contador de processos em background

        sleep(1);
    }

    return 0;
}