#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#define MAX_COMMANDS 200

int main() {
    char command[256];
    char *commands[MAX_COMMANDS];

    while (1) {
        printf("Shell> ");
        fflush(stdout);  // Importante para que se muestre el prompt

        // Lee la línea de comandos
        if (fgets(command, sizeof(command), stdin) == NULL) {
            // EOF o error
            printf("\n");
            break;
        }

        // Elimina el '\n' final si existe
        command[strcspn(command, "\n")] = '\0';

        // Reinicia el contador de comandos
        int command_count = 0;

        // Separa los comandos usando '|'
        char *token = strtok(command, "|");
        while (token != NULL && command_count < MAX_COMMANDS) {
            // Quita espacios al inicio si hay
            while (*token == ' ') token++;
            commands[command_count++] = token;
            token = strtok(NULL, "|");
        }

        // Si no hay comandos, saltar
        if (command_count == 0) continue;

        // Pipes para conectar procesos
        int pipes[MAX_COMMANDS - 1][2];

        // Crear los pipes necesarios
        for (int i = 0; i < command_count - 1; i++) {
            if (pipe(pipes[i]) < 0) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
        }

        // Crear procesos hijos para cada comando
        for (int i = 0; i < command_count; i++) {
            pid_t pid = fork();
            if (pid == 0) {
                // Hijo

                // Si no es el primer comando, redirige stdin al extremo de lectura del pipe anterior
                if (i != 0) {
                    dup2(pipes[i - 1][0], STDIN_FILENO);
                }

                // Si no es el último comando, redirige stdout al extremo de escritura del pipe actual
                if (i != command_count - 1) {
                    dup2(pipes[i][1], STDOUT_FILENO);
                }

                // Cierra todos los pipes en el hijo
                for (int j = 0; j < command_count - 1; j++) {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }

                // Divide el comando actual en argumentos separados por espacios
                char *args[100];
                int arg_count = 0;
                char *arg = strtok(commands[i], " ");
                while (arg != NULL && arg_count < 99) {
                    args[arg_count++] = arg;
                    arg = strtok(NULL, " ");
                }
                args[arg_count] = NULL;

                // Ejecuta el comando
                if (execvp(args[0], args) < 0) {
                    perror("execvp");
                    exit(EXIT_FAILURE);
                }
            } else if (pid < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
            }
            // Padre sigue creando más procesos
        }

        // Padre: cierra todos los pipes que creó
        for (int i = 0; i < command_count - 1; i++) {
            close(pipes[i][0]);
            close(pipes[i][1]);
        }

        // Espera a que todos los hijos terminen
        for (int i = 0; i < command_count; i++) {
            wait(NULL);
        }
    }

    return 0;
}
