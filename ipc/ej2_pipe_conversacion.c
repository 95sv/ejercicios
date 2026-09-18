/*
 * EJERCICIO 2 - Pipes: conversación intercalada con espera de respuesta
 *
 * Se extiende el ejercicio 1. Ambos procesos intercambian VARIOS mensajes de
 * forma intercalada (turnos):
 *
 *   regla de turnos:
 *       El padre envía un mensaje y ESPERA la respuesta del hijo
 *       antes de volver a escribir.
 *       El hijo recibe lo escrito y envía su respuesta.
 *
 *   terminación:
 *       El proceso que decide cortar la conversación envía como mensaje
 *       la cadena especial SALIR ("chau"). El otro, al leerla, entiende
 *       que la charla terminó y deja de responder.
 *
 * DETALLE CLAVE DE SINCRONÍA:
 *   write() y read() sobre un pipe son BLOQUEANTES. Por eso el patrón
 *   "escribir y esperar respuesta" funciona como protocolo ping-pong:
 *   cuando el padre vuelve a read(), el kernel lo duerme hasta que el hijo
 *   escriba. No hace falta ningún mecanismo extra de sincronización.
 *
 * Otro detalle: el buffer de un pipe solo tiene ~64 KB. Para comunicaciones
 * largas hay que leer mientras se escribe (justo lo que hace este protocolo).
 *
 * Se usan mensajes de tamaño fijo TAM para que read() siempre obtenga
 * un mensaje completo (ver ejercicio 3).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define TAM 128
#define SALIR "chau" /* mensaje de terminación acordado */

int main(void) {
    int p2h[2]; /* pipe padre -> hijo */
    int h2p[2]; /* pipe hijo -> padre */
    char buf[TAM];

    const char *padre_dice[] = {
        "Hola hijo",
        "¿Cómo estás?",
        "Bueno, me voy...",
        SALIR
    };
    const char *hijo_dice[] = {
        "Hola papá",
        "Muy bien, gracias",
        "Contame",
        SALIR
    };
    int nm = sizeof(padre_dice) / sizeof(padre_dice[0]); /* nro. de mensajes */

    if (pipe(p2h) == -1 || pipe(h2p) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    /* ==================== HIJO ==================== */
    if (pid == 0) {
        close(p2h[1]); /* no escribe en padre->hijo */
        close(h2p[0]); /* no lee de hijo->padre   */

        for (int i = 0; i < nm; i++) {
            /* Bloquea hasta que el padre escriba su mensaje. */
            if (read(p2h[0], buf, TAM) <= 0)
                break;
            printf("[hijo]  recibí: %s\n", buf);

            /* Si el padre avisó que corta, no respondemos. */
            if (strcmp(buf, SALIR) == 0)
                break;

            strcpy(buf, hijo_dice[i]);
            printf("[hijo]  envío: %s\n", buf);
            if (write(h2p[1], buf, TAM) == -1)
                break;
        }

        close(p2h[0]);
        close(h2p[1]);
        exit(EXIT_SUCCESS);
    }

    /* ==================== PADRE ==================== */
    close(p2h[0]); /* no lee de padre->hijo */
    close(h2p[1]); /* no escribe en hijo->padre */

    for (int i = 0; i < nm; i++) {
        strcpy(buf, padre_dice[i]);
        printf("[padre] envío: %s\n", buf);
        if (write(p2h[1], buf, TAM) == -1)
            break;

        /* Si decidimos terminar, no esperamos respuesta. */
        if (strcmp(buf, SALIR) == 0)
            break;

        /* ESPERA la respuesta del hijo antes del siguiente mensaje. */
        if (read(h2p[0], buf, TAM) <= 0)
            break;
        printf("[padre] recibí: %s\n", buf);

        if (strcmp(buf, SALIR) == 0)
            break;
    }

    close(p2h[1]);
    close(h2p[0]);
    wait(NULL);
    return 0;
}