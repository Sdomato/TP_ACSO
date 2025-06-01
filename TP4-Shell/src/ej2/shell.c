#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <wordexp.h>

#define MAX_COMMANDS 200

char *trim(char *str) {
    while (*str == ' ') str++;
    char *end = str + strlen(str) - 1;
    while (end > str && *end == ' ') {
        *end = '\0';
        end--;
    }
    return str;
}

// Chequea si hay sintaxis inválida con pipes (| al inicio, al final o dobles)
int sintaxis_invalida(char *linea) {
    char *p = linea;
    // Saltar espacios iniciales
    while (*p == ' ') p++;
    if (*p == '|') return 1;  // Empieza con pipe

    // Chequear doble pipe o triple pipe
    for (int i = 0; linea[i] != '\0'; i++) {
        if (linea[i] == '|') {
            int j = i + 1;
            while (linea[j] == ' ') j++;  // saltar espacios
            if (linea[j] == '|' || linea[j] == '\0') {
                return 1;  // doble o final con pipe
            }
        }
    }

    // Chequear si termina con pipe
    int len = strlen(linea);
    int k = len - 1;
    while (k >= 0 && linea[k] == ' ') k--;
    if (linea[k] == '|') return 1;

    return 0;
}

int main() {
    char command[256];
    char *commands[MAX_COMMANDS];

    while (1) {
        fflush(stdout);

        if (fgets(command, sizeof(command), stdin) == NULL) {
            printf("\n");
            break;
        }

        command[strcspn(command, "\n")] = '\0';

        // Validar sintaxis de pipes
        if (sintaxis_invalida(command)) {
            fprintf(stderr, "Error: sintaxis inválida\n");
            continue;
        }

        // Separar los comandos
        int command_count = 0;
        char *token = strtok(command, "|");
        while (token != NULL && command_count < MAX_COMMANDS) {
            char *trimmed = trim(token);
            if (strlen(trimmed) > 0) {
                commands[command_count++] = trimmed;
            }
            token = strtok(NULL, "|");
        }

        if (command_count == 0) continue;

        // Si el único comando es exit, terminar
        if (strcmp(commands[0], "exit") == 0 && command_count == 1) {
            exit(0);
        }

        // Pipes
        int pipes[MAX_COMMANDS - 1][2];
        for (int i = 0; i < command_count - 1; i++) {
            if (pipe(pipes[i]) < 0) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
        }

        // Crear hijos
        for (int i = 0; i < command_count; i++) {
            pid_t pid = fork();
            if (pid == 0) {
                if (i != 0) dup2(pipes[i - 1][0], STDIN_FILENO);
                if (i != command_count - 1) dup2(pipes[i][1], STDOUT_FILENO);

                for (int j = 0; j < command_count - 1; j++) {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }

                wordexp_t p;
                if (wordexp(commands[i], &p, 0) != 0 || p.we_wordc == 0) {
                    fprintf(stderr, "Error: comando inválido\n");
                    exit(EXIT_FAILURE);
                }

                if (execvp(p.we_wordv[0], p.we_wordv) < 0) {
                    perror("execvp");
                    wordfree(&p);
                    exit(EXIT_FAILURE);
                }
            } else if (pid < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
            }
        }

        // Padre: cerrar pipes
        for (int i = 0; i < command_count - 1; i++) {
            close(pipes[i][0]);
            close(pipes[i][1]);
        }

        // Esperar hijos
        for (int i = 0; i < command_count; i++) {
            wait(NULL);
        }
    }

    return 0;
}
