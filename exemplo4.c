#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#define MAX_COMMANDS 5
#define MAX_ARGS 3

void handle_sigint(int sig);
void handle_sigtstp(int sig);
void execute_command(char *command, int is_foreground);
void wait_for_children();
void terminate_shell();

pid_t foreground_pid = -1;
pid_t background_pids[MAX_COMMANDS - 1][2]; // Store P2, P2', P3, P3', etc.
int background_count = 0;

int main() {
    char input[256];
    struct sigaction sa_int, sa_tstp;

    // Setup signal handlers
    sa_int.sa_handler = handle_sigint;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, NULL);

    sa_tstp.sa_handler = handle_sigtstp;
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = 0;
    sigaction(SIGTSTP, &sa_tstp, NULL);

    while (1) {
        printf("fsh> ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            perror("fgets");
            continue;
        }

        // Remove newline character
        input[strcspn(input, "\n")] = 0;

        // Split input into commands
        char *commands[MAX_COMMANDS];
        int command_count = 0;
        char *token = strtok(input, "#");
        while (token != NULL && command_count < MAX_COMMANDS) {
            commands[command_count++] = token;
            token = strtok(NULL, "#");
        }

        // Execute commands
        for (int i = 0; i < command_count; i++) {
            if (strcmp(commands[i], "waitall") == 0) {
                wait_for_children();
            } else if (strcmp(commands[i], "die") == 0) {
                terminate_shell();
            } else {
                execute_command(commands[i], i == 0);
            }
        }

        // Wait for foreground process to finish
        if (foreground_pid > 0) {
            int status;
            waitpid(foreground_pid, &status, 0);
            foreground_pid = -1;
        }
    }

    return 0;
}

void handle_sigint(int sig) {
    if (foreground_pid > 0 || background_count > 0) {
        printf("\nThere are still running processes. Are you sure you want to exit? (y/n): ");
        char response = getchar();
        if (response == 'y' || response == 'Y') {
            terminate_shell();
        }
    } else {
        exit(0);
    }
}

void handle_sigtstp(int sig) {
    for (int i = 0; i < background_count; i++) {
        kill(background_pids[i][0], SIGTSTP);
        kill(background_pids[i][1], SIGTSTP);
    }
    if (foreground_pid > 0) {
        kill(foreground_pid, SIGTSTP);
    }
}

void execute_command(char *command, int is_foreground) {
    char *args[MAX_ARGS + 1];
    int arg_count = 0;
    char *token = strtok(command, " ");
    while (token != NULL && arg_count < MAX_ARGS) {
        args[arg_count++] = token;
        token = strtok(NULL, " ");
    }
    args[arg_count] = NULL;

    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        if (is_foreground) {
            signal(SIGINT, SIG_IGN);
        }
        execvp(args[0], args);
        perror("execvp");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        if (is_foreground) {
            foreground_pid = pid;
        } else {
            background_pids[background_count][0] = pid;
            pid_t secondary_pid = fork();
            if (secondary_pid == 0) {
                // Secondary child process
                execvp(args[0], args);
                perror("execvp");
                exit(EXIT_FAILURE);
            } else if (secondary_pid > 0) {
                background_pids[background_count][1] = secondary_pid;
                background_count++;
            }
        }
    } else {
        perror("fork");
    }
}

void wait_for_children() {
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0) {
        if (errno == EINTR) {
            continue;
        }
    }
}

void terminate_shell() {
    for (int i = 0; i < background_count; i++) {
        kill(background_pids[i][0], SIGKILL);
        kill(background_pids[i][1], SIGKILL);
    }
    if (foreground_pid > 0) {
        kill(foreground_pid, SIGKILL);
    }
    exit(0);
}