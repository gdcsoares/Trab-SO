#include <stdio.h>      // Biblioteca para entrada e saída padrão (printf, scanf)
#include <stdlib.h>     // Biblioteca padrão do C (malloc, free, exit)
#include <string.h>     // Biblioteca para manipulação de strings (strtok, strcmp)
#include <unistd.h>     // Biblioteca para chamadas de sistema (fork, execvp, _exit)
#include <sys/types.h>  // Definições de tipos de dados usados em chamadas de sistema
#include <sys/wait.h>   // Biblioteca para esperar processos filhos (wait, waitpid)
#include <signal.h>     // Biblioteca para manipulação de sinais (sigaction, kill)
#include <errno.h>      // Biblioteca para manipulação de erros (errno)

#define MAX_COMMANDS 5  // Número máximo de comandos permitidos em uma linha de comando
#define MAX_ARGS 3      // Número máximo de argumentos para um comando
#define MAX_BUFFER 1024 // Tamanho máximo do buffer para leitura de comandos

// Variáveis globais para gerenciar processos
pid_t fg_process = 0;                       // Armazena o PID do processo em foreground
pid_t bg_processes[MAX_COMMANDS - 1];       // Armazena os PIDs dos processos em background
int num_bg_processes = 0;                   // Contador para o número de processos em background
int has_fg_process = 0;                     // Flag para indicar se há um processo em foreground

// Tratador de sinal SIGINT (Ctrl-C)
void handle_sigint(int sig) {
    if (has_fg_process) { // Verifica se há um processo em foreground
        printf("\nVocê tem certeza que deseja finalizar a shell? (y/n): ");
        char response;
        scanf(" %c", &response); // Lê a resposta do usuário
        if (response == 'y' || response == 'Y') {
            // Se o usuário confirmar, mata todos os processos em background
            for (int i = 0; i < num_bg_processes; i++) {
                kill(bg_processes[i], SIGKILL);
            }
            exit(0); // Finaliza a shell
        }
    } else {
        exit(0); // Se não houver processos em foreground, finaliza a shell
    }
}

// Tratador de sinal SIGTSTP (Ctrl-Z)
void handle_sigtstp(int sig) {
    // Suspende todos os processos em background
    for (int i = 0; i < num_bg_processes; i++) {
        kill(bg_processes[i], SIGTSTP);
    }
    if (fg_process) { // Se houver um processo em foreground, também o suspende
        kill(fg_process, SIGTSTP);
    }
}

// Função para executar um comando em foreground
void execute_command(char *cmd) {
    char *args[MAX_ARGS + 1] = { NULL }; // Vetor para armazenar os argumentos do comando
    char *token = strtok(cmd, " ");      // Divide o comando em termos (usando espaço como delimitador)
    int arg_count = 0;

    // Preenche o vetor de argumentos
    while (token && arg_count < MAX_ARGS) {
        args[arg_count++] = token;
        token = strtok(NULL, " ");
    }

    // Verifica se o comando é "die"
    if (strcmp(args[0], "die") == 0) {
        // Mata todos os processos filhos vivos
        for (int i = 0; i < num_bg_processes; i++) {
            kill(bg_processes[i], SIGKILL);
        }
        if (fg_process) {
            kill(fg_process, SIGKILL);
        }
        exit(0); // Finaliza a shell
    } 
    // Verifica se o comando é "waitall"
    else if (strcmp(args[0], "waitall") == 0) {
        // Espera todos os processos filhos terminarem antes de continuar
        while (waitpid(-1, NULL, WNOHANG) > 0);
    } 
    // Caso seja outro comando, executa normalmente
    else {
        pid_t pid = fork(); // Cria um novo processo

        if (pid < 0) { // Erro na criação do processo
            perror("Erro no fork");
            return;
        } 
        // Processo filho
        else if (pid == 0) {
            // Ignora SIGINT no processo filho
            signal(SIGINT, SIG_IGN);
            execvp(args[0], args); // Executa o comando
            perror("Erro no exec"); // Se execvp falhar, imprime um erro
            exit(1);
        } 
        // Processo pai
        else {
            fg_process = pid; // Armazena o PID do processo em foreground
            has_fg_process = 1;
            waitpid(pid, NULL, 0); // Espera o processo filho terminar
            fg_process = 0;        // Reseta o PID do processo em foreground
            has_fg_process = 0;
        }
    }
}

// Função para executar um comando em background
void execute_background(char *cmd) {
    char *args[MAX_ARGS + 1] = { NULL }; // Vetor para armazenar os argumentos do comando
    char *token = strtok(cmd, " ");      // Divide o comando em termos (usando espaço como delimitador)
    int arg_count = 0;

    // Preenche o vetor de argumentos
    while (token && arg_count < MAX_ARGS) {
        args[arg_count++] = token;
        token = strtok(NULL, " ");
    }

    pid_t pid = fork(); // Cria um novo processo

    if (pid < 0) { // Erro na criação do processo
        perror("Erro no fork");
        return;
    } 
    // Processo filho
    else if (pid == 0) {
        // Ignora SIGINT no processo filho
        signal(SIGINT, SIG_IGN);
        execvp(args[0], args); // Executa o comando
        perror("Erro no exec"); // Se execvp falhar, imprime um erro
        exit(1);
    } 
    // Processo pai
    else {
        // Cria um processo secundário em background
        pid_t secondary_pid = fork();
        if (secondary_pid == 0) {
            // Ignora SIGINT no processo secundário
            signal(SIGINT, SIG_IGN);
            execvp(args[0], args); // Executa o mesmo comando no processo secundário
            perror("Erro no exec secundário"); // Se execvp falhar, imprime um erro
            exit(1);
        }

        // Armazena o PID do processo em background no vetor de processos
        bg_processes[num_bg_processes++] = pid;
    }
}

// Função principal da shell
int main() {
    char buffer[MAX_BUFFER];       // Buffer para armazenar a linha de comando
    struct sigaction sa;           // Estrutura para configurar tratamento de sinais

    // Configurando tratador de sinal para SIGINT (Ctrl-C)
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    // Configurando tratador de sinal para SIGTSTP (Ctrl-Z)
    sa.sa_handler = handle_sigtstp;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTSTP, &sa, NULL);

    // Loop principal da shell
    while (1) {
        printf("fsh> "); // Exibe o prompt
        fflush(stdout);  // Garante que o prompt seja exibido imediatamente

        // Lê a linha de comando do usuário
        if (fgets(buffer, MAX_BUFFER, stdin) == NULL) {
            perror("Erro ao ler o comando");
            continue;
        }

        buffer[strcspn(buffer, "\n")] = '\0'; // Remove o caractere de nova linha

        char *commands[MAX_COMMANDS] = { NULL }; // Vetor para armazenar os comandos separados por '#'
        char *token = strtok(buffer, "#");       // Separa os comandos pelo operador '#'
        int cmd_count = 0;

        // Armazena os comandos separados por '#'
        while (token && cmd_count < MAX_COMMANDS) {
            commands[cmd_count++] = token;
            token = strtok(NULL, "#");
        }

        // Executa o primeiro comando em foreground
        if (cmd_count > 0) {
            execute_command(commands[0]); 
            // Executa os demais comandos em background
            for (int i = 1; i < cmd_count; i++) {
                execute_background(commands[i]);
            }
        }
    }

    return 0; // Finaliza o programa (nunca deve ser atingido)
}
