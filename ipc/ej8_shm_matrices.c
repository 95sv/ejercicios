/*
 * EJERCICIO 2 - Memoria compartida: suma paralela de dos matrices 9x9
 *
 * Objetivo: sumar A + B = R (matrices de 9x9 de enteros) usando varios
 * procesos y un único segmento de memoria compartida que contiene las
 * TRES matrices. Cada proceso calcula un conjunto de FILAS de R.
 *
 * ¿Por qué NO hace falta semáforo aquí (a diferencia del ejercicio 7)?
 *   Porque se reparten las filas: la fila i de R la calcula UN solo proceso.
 *   Ningún par de procesos escribe en la misma celda, de modo que no hay
 *   región crítica ni carreras. (Si dos procesos escribieran la MISMA celda
 *   sí haría falta exclusión mutua.)
 *
 * Reparto de filas (strided):
 *   proceso 0 -> filas 0, 3, 6
 *   proceso 1 -> filas 1, 4, 7
 *   proceso 2 -> filas 2, 5, 8
 *
 * Al terminar, el padre espera a los hijos y muestra las 3 matrices.
 *
 * Uso:  ./ej8  [nro_de_procesos]
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define FILAS 9
#define COLS  9
#define NPROCS_DEF 3

/* Contiene las dos matrices de entrada y la resultante. Todo dentro de
 * un único segmento compartido. */
typedef struct {
    int A[FILAS][COLS];
    int B[FILAS][COLS];
    int R[FILAS][COLS];
} Matrices;

static void imprimir(const char *nombre, int M[FILAS][COLS]) {
    printf("\n%s\n", nombre);
    for (int i = 0; i < FILAS; i++) {
        for (int j = 0; j < COLS; j++)
            printf("%3d ", M[i][j]);
        putchar('\n');
    }
}

int main(int argc, char **argv) {
    int nprocs = (argc > 1) ? atoi(argv[1]) : NPROCS_DEF;
    if (nprocs < 1) nprocs = 1;

    /* 1) Crear y mapear el segmento compartido. */
    int shmid = shmget(IPC_PRIVATE, sizeof(Matrices), IPC_CREAT | 0600);
    if (shmid == -1) { perror("shmget"); return 1; }

    Matrices *m = shmat(shmid, NULL, 0);
    if (m == (void *)-1) { perror("shmat"); return 1; }

    /* 2) Cargar valores de prueba en A y B. */
    for (int i = 0; i < FILAS; i++) {
        for (int j = 0; j < COLS; j++) {
            m->A[i][j] = i + j;                    /* matriz fácil de seguir  */
            m->B[i][j] = (i * 3 + j) % 10;
            m->R[i][j] = 0;
        }
    }

    /* 3) Hijos: cada uno suma solo las filas que le tocan (disjuntas).
     *    m->R ya está mapeado en todos: lo que escribe uno lo ve el padre. */
    for (int p = 0; p < nprocs; p++) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return 1; }
        if (pid == 0) {
            for (int i = p; i < FILAS; i += nprocs)
                for (int j = 0; j < COLS; j++)
                    m->R[i][j] = m->A[i][j] + m->B[i][j];
            printf("[proceso %d] sumé filas: ", p);
            for (int i = p; i < FILAS; i += nprocs)
                printf("%d ", i);
            putchar('\n');
            exit(EXIT_SUCCESS);
        }
    }

    /* 4) El padre espera a todos los hijos. */
    for (int p = 0; p < nprocs; p++) wait(NULL);

    /* 5) Mostrar resultado. */
    imprimir("Matriz A:", m->A);
    imprimir("Matriz B:", m->B);
    imprimir("Matriz R = A + B:", m->R);

    /* 6) Limpieza. */
    shmdt(m);
    shmctl(shmid, IPC_RMID, NULL);
    return 0;
}