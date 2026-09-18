#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_CONT 100

typedef struct {
    int tipo;
    char contenido[MAX_CONT];
} Mensaje;

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
        close(fd[1]);

        Mensaje m;
        ssize_t r;

        while ((r = read(fd[0], &m, sizeof(m))) > 0) {
            if (r != sizeof(m)) {
                fprintf(stderr, "Lectura parcial de Mensaje\n");
                break;
            }
            printf("Receptor: tipo=%d, contenido='%s'\n", m.tipo, m.contenido);
        }

        close(fd[0]);
        exit(0);
    } else {
        // Emisor
        close(fd[0]);

        Mensaje mensajes[3];

        mensajes[0].tipo = 1;
        strcpy(mensajes[0].contenido, "Hola");

        mensajes[1].tipo = 2;
        strcpy(mensajes[1].contenido, "Mundo");

        mensajes[2].tipo = 3;
        strcpy(mensajes[2].contenido, "Pipes");

        for (int i = 0; i < 3; i++) {
            if (write(fd[1], &mensajes[i], sizeof(Mensaje)) != sizeof(Mensaje)) {
                perror("write Mensaje");
                exit(1);
            }
        }

        close(fd[1]);
        wait(NULL);
    }

    return 0;
}