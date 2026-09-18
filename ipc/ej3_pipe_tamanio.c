/*
 * EJERCICIO 3 - Pipes: el tamaño del mensaje debe ser acordado
 *
 * Un pipe es un flujo de bytes SIN estructura. El kernel no sabe dónde
 * termina "un mensaje": solo ve bytes. Por eso emisor y receptor TIENEN
 * que ponerse de acuerdo en cuántos bytes significan "un mensaje".
 *
 * Fase A: se envían enteros. Cada mensaje = sizeof(int) bytes.
 *         El emisor escribe enteros de a uno; el receptor lee de a uno.
 * Fase B: se envían structs {tipo, contenido}. Cada mensaje = sizeof(Mensaje).
 *
 * PISTA: write() y read() se usan con el tamaño del objeto que se envía.
 * El receptor debe leer SIEMPRE exactamente ese tamaño, y verificar la
 * cantidad de bytes devueltos para detectar lecturas parciales.
 *
 * Nota: el struct puede tener "padding" (bytes de relleno) que se copian
 * también; como ambos procesos son el mismo binario, coincide siempre.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_CONT 100
#define CANT_INT 5   /* cuántos enteros se envían en la fase A */
#define CANT_MSG 4   /* cuántos structs se envían en la fase B */

/* Tipo de mensaje para la fase B. */
typedef struct {
    int tipo;               /* 1 = info, 2 = aviso, 3 = error, ... */
    char contenido[MAX_CONT];
} Mensaje;

/* ------------------------------------------------------------------ */
/* FASE A: enviar enteros de a uno.                                   */
/* ------------------------------------------------------------------ */
static void fase_enteros(void) {
    int fd[2];
    if (pipe(fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        /* ---- HIJO: receptor ---- */
        close(fd[1]);        /* no escribe */
        int valor;
        printf("  (receptor) leyendo enteros de a uno...\n");
        for (int i = 0; i < CANT_INT; i++) {
            /* Leemos exactamente sizeof(int) bytes = un entero. */
            ssize_t r = read(fd[0], &valor, sizeof(int));
            if (r != sizeof(int)) {
                fprintf(stderr, "  (receptor) error: lectura parcial\n");
                break;
            }
            printf("  (receptor) leí: %d\n", valor);
        }
        close(fd[0]);
        exit(EXIT_SUCCESS);
    }

    /* ---- PADRE: emisor ---- */
    close(fd[0]);            /* no lee */
    int numeros[CANT_INT] = {100, 200, 300, 400, 500};

    for (int i = 0; i < CANT_INT; i++) {
        /* Un write() de sizeof(int) bytes por cada entero. */
        if (write(fd[1], &numeros[i], sizeof(int)) != sizeof(int))
            perror("write entero");
        usleep(400000);      /* pausa para ver que el hijo lee de a uno */
    }
    close(fd[1]);
    wait(NULL);
}

/* ------------------------------------------------------------------ */
/* FASE B: enviar structs Mensaje de a uno.                           */
/* ------------------------------------------------------------------ */
static void fase_mensajes(void) {
    int fd[2];
    if (pipe(fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        /* ---- HIJO: receptor ---- */
        close(fd[1]);
        Mensaje m;
        printf("  (receptor) leyendo structs de a uno...\n");
        for (int i = 0; i < CANT_MSG; i++) {
            /* Leemos exactamente sizeof(Mensaje) bytes = un mensaje completo. */
            ssize_t r = read(fd[0], &m, sizeof(Mensaje));
            if (r != sizeof(Mensaje)) {
                fprintf(stderr, "  (receptor) error: lectura parcial\n");
                break;
            }
            printf("  (receptor) tipo=%d contenido='%s'\n",
                   m.tipo, m.contenido);
        }
        close(fd[0]);
        exit(EXIT_SUCCESS);
    }

    /* ---- PADRE: emisor ---- */
    close(fd[0]);
    const char *textos[CANT_MSG] = {"Hola", "Mundo", "Inter-Process", "Fin"};

    for (int i = 0; i < CANT_MSG; i++) {
        Mensaje m;
        m.tipo = i + 1;
        memset(m.contenido, 0, MAX_CONT);           /* limpiar el buffer */
        strncpy(m.contenido, textos[i], MAX_CONT - 1);

        if (write(fd[1], &m, sizeof(Mensaje)) != sizeof(Mensaje))
            perror("write Mensaje");
        usleep(400000); /* pausa para observar cada lectura */
    }
    close(fd[1]);
    wait(NULL);
}

int main(void) {
    puts("=== FASE A: envío de enteros ===");
    fase_enteros();

    puts("\n=== FASE B: envío de struct ===");
    fase_mensajes();

    return 0;
}