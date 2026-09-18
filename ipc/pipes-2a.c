#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define TAM 128
#define SALIR "chau"

void leer_linea(const char *prompt, char *buf) {
    memset(buf, 0, TAM);
    printf("%s", prompt);
    fflush(stdout);
    if (fgets(buf, TAM, stdin) == NULL)   // Ctrl+D = salir
        strcpy(buf, SALIR);
    buf[strcspn(buf, "\n")] = '\0';       // saca el \n
}

int main(void) {
    int p2h[2], h2p[2];
    char buf[TAM];

    if (pipe(p2h) == -1 || pipe(h2p) == -1) { perror("pipe"); exit(1); }

    pid_t pid = fork();
    if (pid == -1) { perror("fork"); exit(1); }

    if (pid == 0) {                         // ---- HIJO ----
        close(p2h[1]);
        close(h2p[0]);

        // read devuelve 0 si el padre cerró su extremo
        while (read(p2h[0], buf, TAM) > 0) {
            printf("[hijo] el padre dice: %s\n", buf);
            if (strcmp(buf, SALIR) == 0) break;

            leer_linea("[hijo] > ", buf);
            write(h2p[1], buf, TAM);
            if (strcmp(buf, SALIR) == 0) break;
        }

        close(p2h[0]);
        close(h2p[1]);
        exit(0);
    }

    // ---- PADRE ----
    close(p2h[0]);
    close(h2p[1]);

    while (1) {
        leer_linea("[padre] > ", buf);
        write(p2h[1], buf, TAM);
        if (strcmp(buf, SALIR) == 0) break;

        if (read(h2p[0], buf, TAM) <= 0) break;   // espera respuesta
        printf("[padre] el hijo dice: %s\n", buf);
        if (strcmp(buf, SALIR) == 0) break;
    }

    close(p2h[1]);
    close(h2p[0]);
    wait(NULL);
    return 0;
}