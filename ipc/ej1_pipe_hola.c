/*
 * EJERCICIO 1 - Pipes: comunicar padre e hijo
 *
 * PIPE: mecanismo de comunicación unidireccional provisto por el sistema
 * operativo. La llamada pipe(fd) crea un conducto con dos extremos:
 *   - fd[0]: extremo de LECTURA
 *   - fd[1]: extremo de ESCRITURA
 *
 * Cuando se hace fork(), el hijo hereda una COPIA de los descriptores, por lo
 * que ambos procesos comparten el mismo pipe físico.
 *
 * REGLA DE ORO: cada proceso debe CERRAR los extremos que no usa, porque:
 *   1. Evita quedarse bloqueado: read() devuelve 0 (fin de datos) recién
 *      cuando NO queda ningún escritor con el extremo abierto.
 *   2. Evita fugas de descriptores.
 *
 * Como aquí necesitamos ida y vuelta (padre->hijo y hijo->padre), se usa el
 * truco de crear DOS pipes (doble canal): uno por cada sentido.
 *
 * Escribir un pipe:
 *   write(fd[1], datos, nbytes)   -> mueve nbytes al conducto.
 * Leer un pipe:
 *   read(fd[0], buf, nbytes)      -> bloquea hasta que lleguen datos
 *                                    (o devuelve 0 si se cerró el otro extremo).
 *
 * IMPORTANTE: write() y read() del pipe suelen ser atómicos para escrituras
 * menores a PIPE_BUF (4096 bytes). Aquí escribimos un mensaje de tamaño fijo
 * TAM, y el receptor lee exactamente TAM bytes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define TAM 64 /* tamaño fijo de cada mensaje (emisor y receptor acuerdan) */

int main(void) {
    int p2h[2]; /* pipe padre -> hijo  (el padre escribe, el hijo lee)  */
    int h2p[2]; /* pipe hijo -> padre  (el hijo escribe, el padre lee)  */
    char buf[TAM] = {0};

    /* Se crean LOS DOS pipes antes de fork() para que ambos queden
     * disponibles en los dos procesos. */
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
        /* Cierra lo que no usa:
         *   - el extremo de ESCRITURA del pipe p2h (no va a escribir ahí)
         *   - el extremo de LECTURA del pipe h2p  (no va a leer de ahí)  */
        close(p2h[1]);
        close(h2p[0]);

        /* read() se BLOQUEA hasta que el padre escriba "Hola". */
        ssize_t n = read(p2h[0], buf, TAM);
        if (n <= 0) {
            perror("read");
            exit(EXIT_FAILURE);
        }
        printf("[hijo] recibí: %s\n", buf);

        /* Responde por el pipe hijo -> padre. */
        strcpy(buf, "Hola Mundo!");
        if (write(h2p[1], buf, TAM) == -1) {
            perror("write");
            exit(EXIT_FAILURE);
        }

        close(p2h[0]);
        close(h2p[1]);
        exit(EXIT_SUCCESS);
    }

    /* ==================== PADRE ==================== */
    /* Cierra lo que no usa:
     *   - el extremo de LECTURA del pipe p2h (no va a leer ahí)
     *   - el extremo de ESCRITURA del pipe h2p (no va a escribir ahí)  */
    close(p2h[0]);
    close(h2p[1]);

    strcpy(buf, "Hola");
    if (write(p2h[1], buf, TAM) == -1) {
        perror("write");
        exit(EXIT_FAILURE);
    }

    /* Bloquea esperando la respuesta del hijo. */
    ssize_t n = read(h2p[0], buf, TAM);
    if (n <= 0) {
        perror("read");
        exit(EXIT_FAILURE);
    }
    printf("[padre] recibí: %s\n", buf);

    close(p2h[1]);
    close(h2p[0]);

    /* Espera al hijo para no dejar procesos zombies. */
    wait(NULL);
    return 0;
}