#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define TAM 64

int main(void) {
    int p2h[2], h2p[2];   // padre->hijo, hijo->padre
    char buf[TAM] = {0};

    if (pipe(p2h) == -1 || pipe(h2p) == -1) {
        perror("pipe");
        exit(1);
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {              // ---- HIJO ----
        close(p2h[1]);           // no escribe en p2h
        close(h2p[0]);           // no lee de h2p

        read(p2h[0], buf, TAM);  // bloquea hasta que el padre escriba
        printf("Hijo recibió: %s\n", buf);

        strcpy(buf, "Hola Mundo!");
        write(h2p[1], buf, TAM);

        close(p2h[0]);
        close(h2p[1]);
        exit(0);
    }

    // ---- PADRE ----
    close(p2h[0]);               // no lee de p2h
    close(h2p[1]);               // no escribe en h2p

    strcpy(buf, "Hola");
    write(p2h[1], buf, TAM);

    read(h2p[0], buf, TAM);      // espera la respuesta
    printf("Padre recibió: %s\n", buf);

    close(p2h[1]);
    close(h2p[0]);
    wait(NULL);                  // evita dejar al hijo zombie
    return 0;
}