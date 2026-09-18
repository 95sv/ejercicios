/*
 * EJERCICIO 1 - Memoria compartida: contadores de vocales
 *
 * La MEMORIA COMPARTIDA del System V permite que varios procesos lean y
 * escriban la MISMA región de memoria física (no una copia, como pasa con
 * fork() en las variables comunes; esas copias no se comparten).
 *
 * Funciones usadas:
 *   shmget(key, tamaño, IPC_CREAT|0600) : reserva el segmento en el kernel
 *   shmat(shmid, NULL, 0)               : "engancha" (mapea) el segmento
 *                                         al espacio de direcciones del
 *                                         proceso. Devuelve un puntero.
 *   shmdt(puntero)                      : desengancha.
 *   shmctl(shmid, IPC_RMID, NULL)       : borra el segmento.
 *
 * Como los hijos heredan el puntero del padre (el mapa viene de fork()),
 * cada hijo escribe DIRECTAMENTE en los contadores compartidos.
 *
 * PROBLEMA DE CONCURRENCIA:
 *   Varios hijos incrementan los MISMOS contadores. incrementar no es una
 *   operación atómica (es leer-sumar-escribir), así que puede haber carreras.
 *   Para evitarlas se usa un SEMÁFORO del System V como exclusión mutua:
 *     sem_op(-1)  -> esperar/marcar ocupado
 *     sem_op(+1)  -> liberar
 *   Así la suma a los contadores queda protegida (región crítica).
 *
 * División del trabajo:
 *   - El padre recibe por argv[1] el nombre del archivo de texto a procesar.
 *   - Cada hijo procesa una RODAJA (bloque de bytes) distinta del archivo.
 *   - Los resultados se suman en el segmento compartido.
 *   - Al final el padre muestra los totales.
 *
 * Nota: se cuentan vocales ASCII (a,e,i,o,u). Las vocales acentuadas en
 * UTF-8 ocupan 2 bytes y no se cuentan (se puede extender con wchar).
 *
 * Uso:  ./ej7  archivo.txt  [nro_de_procesos]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define NPROCS_DEF 4

/* Estructura alojada EN el segmento compartido. Como vive dentro de
 * memoria compartida no puede tener punteros dinámicos ni strings. */
typedef struct {
    int a, e, i, o, u;
} Contadores;

/* --- utilidades sobre el semáforo (un solo semáforo, índice 0) --- */
static void sem_op(int semid, int op) {
    struct sembuf sb = {0, op, 0};   /* {índice, operación, flags} */
    if (semop(semid, &sb, 1) == -1) {
        perror("semop");
        exit(EXIT_FAILURE);
    }
}

/* Hijo: cuenta vocales en el rango [inicio, fin) del archivo y suma
 * los resultados al segmento compartido bajo el semáforo (región crítica). */
static void contar_rango(const char *archivo, long inicio, long fin,
                         Contadores *c, int semid) {
    FILE *f = fopen(archivo, "r");
    if (!f) { perror("fopen"); exit(EXIT_FAILURE); }

    fseek(f, inicio, SEEK_SET);
    long tam = fin - inicio;
    char *buf = malloc((size_t)tam);
    if (!buf) { perror("malloc"); exit(EXIT_FAILURE); }
    if (tam > 0 && fread(buf, 1, (size_t)tam, f) != (size_t)tam) {
        perror("fread");
        exit(EXIT_FAILURE);
    }
    fclose(f);

    /* Cómputo LOCAL en buffers privados (sin carreras). */
    int a = 0, e = 0, i = 0, o = 0, u = 0;
    for (long k = 0; k < tam; k++) {
        switch (tolower((unsigned char)buf[k])) {
            case 'a': a++; break;
            case 'e': e++; break;
            case 'i': i++; break;
            case 'o': o++; break;
            case 'u': u++; break;
            default: break;
        }
    }
    free(buf);

    /* REGIÓN CRÍTICA: sumar al segmento compartido, protegida por semáforo.
     * Sin esto, dos hijos pueden perder incrementos (carrera). */
    sem_op(semid, -1);         /* P: espero / tomo el recurso */
    c->a += a; c->e += e;
    c->i += i; c->o += o; c->u += u;
    sem_op(semid, +1);         /* V: libero el recurso */

    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s archivo.txt [nro_de_procesos]\n", argv[0]);
        return 1;
    }
    int nprocs = (argc > 2) ? atoi(argv[2]) : NPROCS_DEF;
    if (nprocs < 1) nprocs = 1;

    /* Tamaño del archivo para repartirlo en rodajas. */
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END);
    long tam = ftell(f);
    fclose(f);

    printf("Archivo: %s (%ld bytes), procesos: %d\n", argv[1], tam, nprocs);

    /* 1) Crear + mapear el segmento compartido de Contadores. */
    int shmid = shmget(IPC_PRIVATE, sizeof(Contadores),
                       IPC_CREAT | 0600);
    if (shmid == -1) { perror("shmget"); return 1; }

    Contadores *c = shmat(shmid, NULL, 0);
    if (c == (void *)-1) { perror("shmat"); return 1; }
    c->a = c->e = c->i = c->o = c->u = 0;

    /* 2) Crear el semáforo (1 solo, valor inicial 1 -> mutex libre). */
    int semid = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
    if (semid == -1) { perror("semget"); return 1; }
    if (semctl(semid, 0, SETVAL, 1) == -1) { perror("semctl"); return 1; }

    /* 3) Lanzar hijos: cada uno se encarga de una rodaja disjunta. */
    long base = tam / nprocs;   /* el último hijo se queda con el resto */
    for (int i = 0; i < nprocs; i++) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return 1; }
        if (pid == 0) {
            long inicio = i * base;
            long fin = (i == nprocs - 1) ? tam : inicio + base;
            contar_rango(argv[1], inicio, fin, c, semid);
        }
    }

    /* 4) Esperar a todos y mostrar los totales (leídos de la shm). */
    for (int i = 0; i < nprocs; i++) wait(NULL);

    printf("\nTOTAL de vocales en '%s':\n", argv[1]);
    printf("  a=%d  e=%d  i=%d  o=%d  u=%d  (total=%d)\n",
           c->a, c->e, c->i, c->o, c->u, c->a + c->e + c->i + c->o + c->u);

    /* 5) Limpieza. */
    shmdt(c);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
    return 0;
}