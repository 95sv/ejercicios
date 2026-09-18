#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
    int fd[2];
    if (pipe(fd) == -1) {
        perror("pipe");
        exit(1);
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        // Receptor
        close(fd[1]); // no escribe

        int n;
        ssize_t r;

        while ((r = read(fd[0], &n, sizeof(n))) > 0) {
            if (r != sizeof(n)) {
                fprintf(stderr, "Lectura parcial de entero\n");
                break;
            }
            printf("Receptor: entero = %d\n", n);
        }

        close(fd[0]);
        exit(0);
    } else {
        // Emisor
        close(fd[0]); // no lee

        int datos[] = {10, 20, 30, 40, 50};

        // Opción A: enviar de a uno
        for (int i = 0; i < 5; i++) {
            if (write(fd[1], &datos[i], sizeof(int)) != sizeof(int)) {
                perror("write entero");
                exit(1);
            }
        }

        // Opción B: enviar todos juntos
        // if (write(fd[1], datos, sizeof(datos)) != sizeof(datos)) {
        //     perror("write datos");
        //     exit(1);
        // }

        close(fd[1]);
        wait(NULL);
    }

    return 0;
}