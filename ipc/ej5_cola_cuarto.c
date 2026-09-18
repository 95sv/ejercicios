/*
 * EJERCICIO 2 - Colas de mensajes: cuarto proceso que lee ambos tipos
 *
 * Extiende el ejercicio 1 agregando un proceso que puede leer CUALQUIERA
 * de los dos tipos. La clave es msgrcv() con tipo = 0:
 *
 *     msgrcv(qid, &m, bytes, 0, flags)
 *
 *   con tipo 0 la función toma el mensaje de MENOR tipo disponible
 *   (y, entre mensajes del mismo tipo, el más antiguo, FIFO).
 *   Esto permite leer "lo que venga", sin fijar la clase.
 *
 * LECTURA NO BLOQUEANTE + AVISO DE FIN:
 *   Los lectores usan la bandera IPC_NOWAIT (no se duermen si la cola
 *   está vacía, devuelve -1) y hacen polling cada pocos milisegundos.
 *   Para saber cuándo terminar (porque la cola no dice "no hay más"),
 *   el emisor escribe un flag en un pequeño segmento de memoria
 *   compartida:  mientras fin == 0 siguen esperando datos; cuando fin
 *   cambia a 1 abandonan. Así ningún proceso queda bloqueado para siempre.
 *
 * Cada proceso CUENTA lo que leyó y lo informa al terminar.
 * La suma de lo leído por los tres = total de mensajes reales enviados.
 *
 * Uso:  ./ej5  [cant_tipo1] [cant_tipo2]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>

#define TAM_TEXTO 64

typedef struct {
    long mtype;
    char mtext[TAM_TEXTO];
} Mensaje;

/* Proceso lector genérico.
 *   tipo = 0  -> lee cualquier tipo (comportamiento del 4º proceso)
 *   tipo > 0  -> lee solo ese tipo
 * Con IPC_NOWAIT nunca se bloquea: si no hay mensaje, mira el flag `fin`. */
static void proceso_lector(int qid, long tipo, volatile const int *fin,
                           const char *nombre) {
    Mensaje m;
    int contador = 0;

    for (;;) {
        if (msgrcv(qid, &m, sizeof(m.mtext), tipo, IPC_NOWAIT) != -1) {
            printf("  [%-16s] tipo=%ld : %s\n", nombre, m.mtype, m.mtext);
            contador++;
            continue;
        }

        /* No había mensaje de mi tipo: ¿el emisor ya avisó el fin? */
        if (*fin)
            break;
        usleep(20000);           /* polling ligero: 20 ms */
    }
    printf("  [%-16s] total leídos: %d\n", nombre, contador);
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    int n1 = 5, n2 = 4;
    if (argc > 1) n1 = atoi(argv[1]);
    if (argc > 2) n2 = atoi(argv[2]);

    /* Cola de mensajes. */
    int qid = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
    if (qid == -1) { perror("msgget"); exit(EXIT_FAILURE); }

    /* Flag de "fin" en un segmento compartido, para avisar a los lectores. */
    int shmid = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0600);
    if (shmid == -1) { perror("shmget"); exit(EXIT_FAILURE); }
    int *fin = shmat(shmid, NULL, 0);
    if (fin == (void *)-1) { perror("shmat"); exit(EXIT_FAILURE); }
    *fin = 0;

    /* Tres lectores: tipo 1, tipo 2 y el NUEVO "cualquiera" (tipo 0).
     * Los tres compiten por los mensajes: los dos primeros solo pueden
     * tomar su tipo; el cuarto toma cualquiera que haya disponible. */
    pid_t pa = fork();
    if (pa == 0) proceso_lector(qid, 1, fin, "solo-tipo1");
    pid_t pb = fork();
    if (pb == 0) proceso_lector(qid, 2, fin, "solo-tipo2");
    pid_t pc = fork();
    if (pc == 0) proceso_lector(qid, 0, fin, "cualquiera");

    /* El emisor envía los mensajes reales, intercalando turnos para que
     * los tres procesos participen. */
    Mensaje m;
    for (int i = 0; i < n1; i++) {
        m.mtype = 1;
        snprintf(m.mtext, TAM_TEXTO, "dato tipo1 #%d", i);
        if (msgsnd(qid, &m, sizeof(m.mtext), 0) == -1) { perror("msgsnd"); exit(1); }
        usleep(150000);
    }
    for (int i = 0; i < n2; i++) {
        m.mtype = 2;
        snprintf(m.mtext, TAM_TEXTO, "dato tipo2 #%d", i);
        if (msgsnd(qid, &m, sizeof(m.mtext), 0) == -1) { perror("msgsnd"); exit(1); }
        usleep(150000);
    }

    /* Damos tiempo a que los datos se consuman y recién entonces ponemos
     * el flag de fin. Los lectores que estén haciendo polling lo verán
     * en unos milisegundos y saldrán. */
    sleep(2);
    *fin = 1;

    wait(NULL); wait(NULL); wait(NULL);

    shmdt(fin);
    shmctl(shmid, IPC_RMID, NULL);
    msgctl(qid, IPC_RMID, NULL);
    puts("Cola eliminada. Fin.");
    return 0;
}