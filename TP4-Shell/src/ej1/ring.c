#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char **argv) {
    if (argc != 4) return fprintf(stderr, "Uso: %s <n> <c> <s>\n", argv[0]), 1;

    int n = atoi(argv[1]), val = atoi(argv[2]), start = atoi(argv[3]);
    if (n < 1 || start < 0 || start >= n) return fprintf(stderr, "Parámetros inválidos\n"), 1;

    int pipes[n][2], padre_a_start[2], final_a_padre[2];
    if (pipe(padre_a_start) || pipe(final_a_padre)) { perror("pipe padre"); exit(1); }
    for (int i = 0; i < n; i++) if (pipe(pipes[i])) { perror("pipe"); exit(1); }

    for (int i = 0; i < n; i++) if (fork() == 0) {
        for (int j = 0; j < n; j++) {
            if (j != i) close(pipes[j][1]);
            if (j != (i - 1 + n) % n) close(pipes[j][0]);
        }
        close(final_a_padre[0]); close(padre_a_start[1]);

        int num;
        int entrada = (i == start) ? padre_a_start[0] : pipes[(i - 1 + n) % n][0];
        if (read(entrada, &num, sizeof(int)) != sizeof(int)) exit(1);
        printf("Proceso %d recibió: %d\n", i, num);
        num++;

        int salida = ((i + 1) % n == start) ? final_a_padre[1] : pipes[i][1];
        if (write(salida, &num, sizeof(int)) != sizeof(int)) exit(1);

        close(entrada); close(salida);
        exit(0);
    }

    for (int i = 0; i < n; i++) close(pipes[i][0]), close(pipes[i][1]);
    close(padre_a_start[0]); close(final_a_padre[1]);

    printf("Padre escribe: %d\n", val);
    write(padre_a_start[1], &val, sizeof(int));
    close(padre_a_start[1]);

    int res;
    if (read(final_a_padre[0], &res, sizeof(int)) != sizeof(int)) { perror("read"); res = -1; }
    close(final_a_padre[0]);

    for (int i = 0; i < n; i++) wait(NULL);
    printf("Resultado final recibido por el padre: %d\n", res);
    return 0;
}
