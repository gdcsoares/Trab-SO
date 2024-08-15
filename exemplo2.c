#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_CMD_LEN 256
#define MAX_ARGS 3
#define MAX_PROCESSES 5

// Variáveis globais para controle de processos
pid_t fg_process_pid = 0;  // Armazena o PID do processo em foreground
pid_t bg_processes[MAX_PROCESSES];  // Armazena os PIDs dos processos em background
int num_bg_processes = 0;  // Número de processos em background

// Função para lidar com o sinal SIGINT (Ctrl-C)
void handle_sigint(int sig) {
    if (fg_process_pid != 0) {
        // Se houver um processo de foreground rodando, ignore o SIGINT
        return;
    }
    if (num_bg_processes > 0) {
        // Se houver processos em background, pergunte se o usuário deseja finalizar
        printf("\nVocê tem certeza que deseja finalizar a shell? (y/n): ");
        char c = getchar();
        if (c == 'y' || c == 'Y') {
            printf("Finalizando shell...\n");
            exit(0);
        } else {
            printf("Continuando shell...\n");
            // Limpar entrada extra (como newline)
            while (getchar() != '\n');
        }
    } else {
        printf("Finalizando shell...\n");
        exit(0);
    }
}

// Função para lidar com o sinal SIGTSTP (Ctrl-Z)
void handle_sigtstp(int sig) {
    for (int i = 0; i < num_bg_processes; i++) {
        if (bg_processes[i] != 0) {
            kill(bg_processes[i], SIGTSTP);  // Suspende todos os processos em background
        }
    }
    if (fg_process_pid != 0) {
        kill(fg_process_pid, SIGTSTP);  // Suspende o processo de foreground
    }
    printf("\nProcessos suspensos.\n");
}

// Função para ignorar SIGINT nos processos filhos
void ignore_sigint() {
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;  // Ignora SIGINT
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
}

// Função para executar comandos internos da shell
int execute_internal_command(char *command) {
    if (strcmp(command, "waitall") == 0) {
        // Comando waitall: espera por todos os processos em background
        int status;
        pid_t pid;
        while ((pid = waitpid(-1, &status, 0)) > 0) {
            // Espera por todos os processos filhos
        }
        return 1;
    } else if (strcmp(command, "die") == 0) {
        // Comando die: termina a shell
        for (int i = 0; i < num_bg_processes; i++) {
            if (bg_processes[i] != 0) {
                kill(bg_processes[i], SIGKILL);  // Mata todos os processos em background
            }
        }
        if (fg_process_pid != 0) {
            kill(fg_process_pid, SIGKILL);  // Mata o processo em foreground
        }
        printf("Shell finalizada.\n");
        exit(0);
    }
    return 0;
}

// Função para executar um comando externo
void execute_command(char *cmd) {
    char *args[MAX_ARGS + 1];
    char *token;
    int arg_count = 0;

    // Dividir o comando em argumentos
    token = strtok(cmd, " ");
    while (token != NULL && arg_count < MAX_ARGS) {
        args[arg_count++] = token;
        token = strtok(NULL, " ");
    }
    args[arg_count] = NULL;  // Último argumento deve ser NULL para execvp

    // Tenta executar o comando
    if (execvp(args[0], args) < 0) {
        perror("Erro no exec");
        exit(EXIT_FAILURE);
    }
}

// Função principal da shell
int main() {
    char command[MAX_CMD_LEN];
    struct sigaction sa_int, sa_tstp;

    // Configura o tratador de sinal para SIGINT
    sa_int.sa_handler = handle_sigint;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, NULL);

    // Configura o tratador de sinal para SIGTSTP
    sa_tstp.sa_handler = handle_sigtstp;
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = 0;
    sigaction(SIGTSTP, &sa_tstp, NULL);

    while (1) {
        printf("fsh> ");
        fflush(stdout);

        // Leitura da linha de comando
        if (fgets(command, sizeof(command), stdin) == NULL) {
            if (errno == EINTR) {
                // Se a leitura foi interrompida por um sinal, continue o loop
                continue;
            } else {
                perror("Erro ao ler o comando");
                continue;
            }
        }

        // Remover o newline do comando
        command[strcspn(command, "\n")] = 0;

        // Separar os comandos pelo símbolo #
        char *commands[MAX_PROCESSES];
        int cmd_count = 0;
        commands[cmd_count++] = strtok(command, "#");
        while ((commands[cmd_count++] = strtok(NULL, "#")) != NULL);

        // Executar o primeiro comando em foreground
        if (execute_internal_command(commands[0])) {
            continue;
        }
        fg_process_pid = fork();
        if (fg_process_pid == 0) {
            ignore_sigint();  // Ignora SIGINT no processo filho
            execute_command(commands[0]);
        } else if (fg_process_pid > 0) {
            int status;
            waitpid(fg_process_pid, &status, 0);
            fg_process_pid = 0;
        } else {
            perror("Erro no fork");
            continue;
        }

        // Executar os comandos restantes em background com processos secundários
        for (int i = 1; i < cmd_count - 1; i++) {
            pid_t pid = fork();
            if (pid == 0) {
                ignore_sigint();  // Ignora SIGINT no processo filho
                execute_command(commands[i]);
            } else if (pid > 0) {
                bg_processes[num_bg_processes++] = pid;

                // Criar o processo secundário em background
                pid_t secondary_pid = fork();
                if (secondary_pid == 0) {
                    ignore_sigint();  // Ignora SIGINT no processo secundário
                    execute_command(commands[i]);
                } else if (secondary_pid > 0) {
                    bg_processes[num_bg_processes++] = secondary_pid;
                } else {
                    perror("Erro no fork secundário");
                }
            } else {
                perror("Erro no fork");
                continue;
            }
        }
    }
    return 0;
}
