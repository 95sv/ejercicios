/*
 * EJERCICIO 3 - Colas de mensajes: simulación de pedidos con prioridades
 *
 * Se simula una cocina donde los pedidos llegan con distinta "urgencia":
 *
 *    PIZZA        -> prioridad ALTA   (cada 10 s)   -> mtype = 1
 *    PAPAS        -> prioridad MEDIA  (cada  5 s)   -> mtype = 2
 *    HAMBURGUESA  -> prioridad BAJA   (cada  1 s, 2 procesos) -> mtype = 3
 *
 * ¿Cómo se implementa la prioridad con colas de mensajes?
 *   msgrcv(qid, ..., tipo=0, ...) DEvuelve siempre el mensaje de MENOR
 *   campo mtype. Entonces, si asignamos un mtype MENOR a la prioridad
 *   MAYOR, el receptor que usca "cualquiera" siempre tomará primero
 *   lo más prioritario (y FIFO entre iguales). Es el mecanismo natural
 *   del System V.
 *
 * Productores (4 procesos):
 *   - 2 procesos "hamburguesa": cada uno produce un pedido cada 1 s.
 *   - 1 proceso "papas": un pedido cada 5 s.
 *   - 1 proceso "pizza": un pedido cada 10 s.
 * Receptor (1 proceso): lee "cualquiera" (tipo 0 => respeta prioridad),
 *   muestra el pedido, y entre pedido y pedido espera un tiempo aleatorio
 *   de 3 a 5 segundos. La cola se va llenando de pedidos de baja prioridad
 *   mientras trabaja, y los de mayor prioridad "saltan" adelante.
 *
 * Cada productor emite una cantidad fija y termina; el receptor deja de
 * leer cuando consumió el total. (Si no hubiera mensajes, msgrcv con
 * flags 0 quedaría esperando.)
 *
 * Uso:  ./ej6 [hamburguesas_cada_proceso] [papas] [pizzas]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define TAM_TEXTO 64

/* Las PRIORIDADES se traducen en mtype (menor = mayor prioridad). */
#define MTYPE_PIZZA        1
#define MTYPE_PAPAS        2
#define MTYPE_HAMBURGUESA  3

typedef struct {
    long mtype;
    char mtext[TAM_TEXTO];
} Mensaje;

/* Productor genérico: envía `cant` pedidos del mismo tipo (prioridad),
 * con un intervalo de `cada_ms` milisegundos entre pedido y pedido. */
static void productor(int qid, long mtype, const char *pedido,
                      int cada_ms, int cant) {
    Mensaje m = {0};
    m.mtype = mtype;
    for (int i = 0; i < cant; i++) {
        snprintf(m.mtext, TAM_TEXTO, "%s #%d", pedido, i);
        if (msgsnd(qid, &m, sizeof(m.mtext), 0) == -1) {
            perror("msgsnd");
            exit(EXIT_FAILURE);
        }
        printf("[productor %s] envié: '%s'\n", pedido, m.mtext);
        fflush(stdout);
        usleep(cada_ms * 1000);
    }
    exit(EXIT_SUCCESS);
}

/* Receptor único: lee "cualquiera" (tipo 0) => respeta prioridades.
 * Entre pedido y pedido duerme 3-5 s (aleatorio). */
static void receptor(int qid, int total) {
    srand((unsigned)time(NULL) ^ (unsigned)getpid());
    Mensaje m;
    int cuenta[4] = {0};   /* cuenta[mtipo] */

    for (int i = 0; i < total; i++) {
        /* tipo 0: toma el MENOR mtype disponible => mayor prioridad. */
        if (msgrcv(qid, &m, sizeof(m.mtext), 0, 0) == -1) {
            perror("msgrcv");
            exit(EXIT_FAILURE);
        }

        const char *cual;
        switch (m.mtype) {
            case MTYPE_PIZZA:       cual = "PIZZA (ALTA)";  break;
            case MTYPE_PAPAS:       cual = "PAPAS (MEDIA)"; break;
            default:                cual = "HAMBURGUESA (BAJA)"; break;
        }
        printf("[receptor] atendí pedido de %s : '%s'\n", cual, m.mtext);
        cuenta[m.mtype]++;

        int t = 3 + rand() % 3;     /* duración entre 3 y 5 segundos */
        printf("[receptor] procesando pedido durante %d s...\n", t);
        sleep(t);
    }

    printf("[receptor] RESUMEN -> pizza=%d, papas=%d, hamburguesas=%d\n",
           cuenta[MTYPE_PIZZA], cuenta[MTYPE_PAPAS], cuenta[MTYPE_HAMBURGUESA]);
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    int nh = 6;  /* hamburguesas POR proceso (hay 2 procesos) */
    int np = 4;  /* papas */
    int nz = 3;  /* pizzas */
    if (argc > 1) nh = atoi(argv[1]);
    if (argc > 2) np = atoi(argv[2]);
    if (argc > 3) nz = atoi(argv[3]);

    int total = 2 * nh + np + nz;
    printf("Total de pedidos a atender: %d\n\n", total);

    int qid = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
    if (qid == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    /* --- Productores (4 procesos): 2 hamburguesa, 1 papas, 1 pizza --- */
    pid_t h1 = fork();  if (h1 == 0)  productor(qid, MTYPE_HAMBURGUESA,
                                                 "hamburguesa", 1000, nh);
    pid_t h2 = fork();  if (h2 == 0)  productor(qid, MTYPE_HAMBURGUESA,
                                                 "hamburguesa", 1000, nh);
    pid_t p = fork();   if (p == 0)   productor(qid, MTYPE_PAPAS,
                                                 "papas", 5000, np);
    pid_t z = fork();   if (z == 0)   productor(qid, MTYPE_PIZZA,
                                                 "pizza", 10000, nz);

    /* --- Receptor único --- */
    pid_t r = fork();   if (r == 0)   receptor(qid, total);

    /* Esperar a que todos terminen. */
    waitpid(h1, NULL, 0); waitpid(h2, NULL, 0);
    waitpid(p,  NULL, 0); waitpid(z,  NULL, 0);
    waitpid(r,  NULL, 0);

    msgctl(qid, IPC_RMID, NULL);
    puts("\nCola eliminada. Fin.");
    return 0;
}