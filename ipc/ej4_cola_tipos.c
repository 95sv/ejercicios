/*
 * EJERCICIO 1 - Colas de mensajes: mensajes clasificados por tipo
 *
 * A diferencia de los pipes (flujo anónimo de bytes), una COLA DE MENSAJES
 * del System V (POSIX) es un objeto con NOMBRE dentro del kernel donde cada
 * mensaje lleva:
 *
 *   - un TIPO (long) : valor entero que permite CLASIFICAR el mensaje,
 *   - un CONTENIDO  : bytes de datos (el texto en este ejercicio).
 *
 * Funciones usadas:
 *   msgget(IPC_PRIVATE, IPC_CREAT|0600) : crea la cola, devuelve su id.
 *   msgsnd(qid, &msg, nbytes, flags)    : ENCOLA un mensaje.
 *       - msg.mtype         : clasificación.
 *       - nbytes            : tamaño del CONTENIDO (no incluye el tipo).
 *   msgrcv(qid, &msg, nbytes, tipo, 0)  : DESENCOLA un mensaje.
 *       - tipo = valor positivo : solo recibe mensajes de ESE tipo.
 *       - tipo = 0              : recibe el primero (prioridad, ver ej. 6).
 *       - sin flags             : si NO hay mensajes, el proceso se BLOQUEA
 *                                 esperando (equivale a "si no existen
 *                                 mensajes en la cola se debe esperar").
 *   msgctl(qid, IPC_RMID, NULL): elimina la cola del kernel.
 *
 * REQUISITO: el struct de mensaje DEBE empezar con un campo long.
 *
 * Esquema:
 *   - Proceso principal: crea la cola y envía c1 mensajes de tipo 1 y
 *                        c2 mensajes de tipo 2 (con pausas, para que se
 *                        note que los lectores ESPERAN).
 *   - Proceso A: lee iterativamente los mensajes de tipo 1.
 *   - Proceso B: lee iterativamente los mensajes de tipo 2.
 *   -> Cada "tipo" funciona como una cola lógica independiente.
 *
 * Uso:  ./ej4  [cant_tipo1] [cant_tipo2]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define TAM_TEXTO 64

typedef struct {
    long mtype;                  /* tipo del mensaje (1er campo obligatorio) */
    char mtext[TAM_TEXTO];       /* contenido */
} Mensaje;

/* Proceso lector: lee `cant` mensajes del tipo indicado y los muestra. */
static void proceso_lector(int qid, long tipo, int cant) {
    Mensaje m;
    char etiqueta[32];
    snprintf(etiqueta, sizeof(etiqueta), "lector-tipo%ld", tipo);

    for (int i = 0; i < cant; i++) {
        /* Si la cola no tiene mensajes de mi tipo, msgrcv() me duerme. */
        printf("  [%s] esperando mensaje...\n", etiqueta);
        fflush(stdout);

        if (msgrcv(qid, &m, sizeof(m.mtext), tipo, 0) == -1) {
            perror("msgrcv");
            exit(EXIT_FAILURE);
        }
        printf("  [%s] recibí: '%s'\n", etiqueta, m.mtext);
    }
    printf("  [%s] terminé (leí %d mensajes)\n", etiqueta, cant);
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    int c1 = 4, c2 = 4;                 /* mensajes de tipo 1 y tipo 2 */
    if (argc > 1) c1 = atoi(argv[1]);
    if (argc > 2) c2 = atoi(argv[2]);

    /* 1) Crear la cola. IPC_PRIVATE: solo la conoce quien la crea,
     *    pero como todos son hijos del mismo padre la heredan. */
    int qid = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
    if (qid == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }
    printf("Cola creada, id = %d\n", qid);

    /* 2) Crear los dos procesos lectores (heredan el id de la cola). */
    pid_t pa = fork();
    if (pa == 0) proceso_lector(qid, 1, c1);

    pid_t pb = fork();
    if (pb == 0) proceso_lector(qid, 2, c2);

    /* 3) Proceso principal: ENVIA los mensajes.
     *    Las pausas hacen que los lectores deban esperar (bloquearse),
     *    demostrando el comportamiento pedido. */
    Mensaje m;

    m.mtype = 1;
    for (int i = 0; i < c1; i++) {
        snprintf(m.mtext, TAM_TEXTO, "mensaje tipo1 #%d", i);
        if (msgsnd(qid, &m, sizeof(m.mtext), 0) == -1) {
            perror("msgsnd");
            exit(EXIT_FAILURE);
        }
        printf("[principal] envié: '%s'\n", m.mtext);
        usleep(300000);
    }

    sleep(2); /* dejamos que el lector tipo 1 drene su cola */

    m.mtype = 2;
    for (int i = 0; i < c2; i++) {
        snprintf(m.mtext, TAM_TEXTO, "mensaje tipo2 #%d", i);
        if (msgsnd(qid, &m, sizeof(m.mtext), 0) == -1) {
            perror("msgsnd");
            exit(EXIT_FAILURE);
        }
        printf("[principal] envié: '%s'\n", m.mtext);
        usleep(300000);
    }

    /* 4) Esperar que los lectores terminen y limpiar la cola. */
    wait(NULL);
    wait(NULL);

    msgctl(qid, IPC_RMID, NULL);        /* elimina la cola del kernel */
    puts("Cola eliminada. Fin.");
    return 0;
}