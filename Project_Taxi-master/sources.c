#define _GNU_SOURCE
#include<limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>   
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h> 
#include <sys/shm.h>
#include <sys/msg.h>
#include "taxi_game.h"

void genera_sources(struct shared *);
void invia_mess(struct shared *, struct stat *);
void uscitaSources(int );
void alarmSources(int );
void sigusr1Sources(int );
void sigusr2Sources(int );

int riga, col, sid, qid;
pid_t pid;
struct sembuf sops;
struct sembuf sops_stat;
struct shared * map;
struct stat * statistiche;

int main(int argc, char * argv[]) {
	int mid, mid_stat;
	struct sigaction sa;
	struct timespec time;
	sigset_t  my_mask;
	
	/* blocco dei segnali iniziale */
	sigfillset(&my_mask);
	sigprocmask(SIG_BLOCK, &my_mask, NULL);
	
	/* attribuzione handler segnale SIGTERM */
	sa.sa_handler = uscitaSources;
	sa.sa_flags = 0;
	sigemptyset(&my_mask);       
	sa.sa_mask = my_mask;
	
	sigaction(SIGTERM, &sa, NULL);
	
	/* attribuzione handler segnale SIGALRM */
	sa.sa_handler = alarmSources;
	sa.sa_flags = 0;
	sigemptyset(&my_mask);       
	sa.sa_mask = my_mask;
	
	sigaction(SIGALRM, &sa, NULL);
	
	/* attribuzione handler segnale SIGUSR1 */
	sa.sa_handler = sigusr1Sources;
	sa.sa_flags = 0;
	sigemptyset(&my_mask);       
	sa.sa_mask = my_mask;
	
	sigaction(SIGUSR1, &sa, NULL);
	
	/* attribuzione handler segnale SIGUSR2 */
	sa.sa_handler = sigusr2Sources;
	sa.sa_flags = 0;
	sigemptyset(&my_mask);       
	sa.sa_mask = my_mask;
	
	sigaction(SIGUSR2, &sa, NULL);
	
	/* lettura parametri */
	mid = atoi(argv[1]);
	mid_stat = atoi(argv[2]);
	sid = atoi(argv[3]);
	qid = atoi(argv[4]);
	
	/* aggancio memoria condivisa mappa*/
	map = shmat(mid, NULL, 0);
	
	/* aggancio memoria condivisa statistiche */
	statistiche = shmat(mid_stat, NULL, 0);
	
	/* generazione effettiva della cella sorgenete */
	genera_sources(map);
	
	pid=getpid();
	
	/* sblocco segnali per l'esecuzione */
	sigfillset(&my_mask);
	sigdelset(&my_mask,SIGTERM);
	sigdelset(&my_mask,SIGALRM);
	sigdelset(&my_mask,SIGUSR1);
	sigdelset(&my_mask,SIGUSR2);
	sigprocmask(SIG_SETMASK, &my_mask, NULL);
	
	/* Informo il master che la cella sources è stata generata */
	sops.sem_num = SEM_SOURCES;
	sops.sem_op = -1;
	sops.sem_flg = 0;
	semop(sid, &sops, 1);
	
	/*printf("sources %d riga %d col %d\n",getpid(), riga, col);*/
	
	/* attendo che il master avvii la simulazione */
	sops.sem_num = SEM_MASTER;
	sops.sem_op = 0;
	sops.sem_flg = 0;
	semop(sid, &sops, 1);
	
	/* incemento il semaforo di stampa */
	sops.sem_num = SEM_STAMPA;
	sops.sem_op = 1;
	sops.sem_flg = 0;
	semop(sid, &sops, 1);
	
	invia_mess(map, statistiche);
}

/* handler segnale SIGTERM */
void uscitaSources(int sig){
	sigset_t my_mask;

	sigfillset(&my_mask);
	sigprocmask(SIG_BLOCK, &my_mask, NULL);
	
	sops.sem_num = SEM_STAMPA;
	sops.sem_op = -1;
	sops.sem_flg = 0;
	semop(sid, &sops, 1);
	
	exit(EXIT_SUCCESS);
}

/* handler segnale SIGALRM */
void alarmSources(int sig) {
	invia_mess(map, statistiche);
}

/* handler segnale SIGUSR1 */
void sigusr1Sources(int sig) {
	alarm(5);
	pause();
}

/* handler segnale SIGUSR2 */
void sigusr2Sources(int sig) {
	invia_mess(map, statistiche);
}

/* generazione posizione cella sorgente */
void genera_sources(struct shared * map){
	int cond;
	struct timespec rand_riga;
	struct timespec rand_col;
	
	do{
		cond = 0;
		clock_gettime(CLOCK_REALTIME ,&rand_riga);
		riga = rand_riga.tv_nsec%SO_HEIGHT;
		clock_gettime(CLOCK_REALTIME ,&rand_col);
		col = rand_col.tv_nsec%SO_WIDTH;


		LOCK;
		if(map->mappa[riga][col].type==NOT_HOLES  && map->mappa[riga][col].type != SOURCES){
			map->mappa[riga][col].type = SOURCES;	
			UNLOCK;
		}else{
			UNLOCK;
			cond = 1;
		}
	}while(cond==1);
}

/* invio messaggio nella coda di messaggi */
void invia_mess(struct shared * map, struct stat * statistiche){
	int cond, riga_dest, col_dest;
	struct msg_ric message;
	struct timespec rand_riga;
	struct timespec rand_col;
	sigset_t  my_mask, old_mask;
	
	do{
		cond = 0;
		clock_gettime(CLOCK_REALTIME ,&rand_riga);
		riga_dest = rand_riga.tv_nsec%SO_HEIGHT;
		clock_gettime(CLOCK_REALTIME ,&rand_col);
		col_dest = rand_col.tv_nsec%SO_WIDTH;
		
		if(map->mappa[riga_dest][col_dest].id_sem != map->mappa[riga][col].id_sem  && map->mappa[riga_dest][col_dest].type != HOLES){
			message.mtype = map->mappa[riga][col].type_msg;
			message.msg[0] = riga;
			message.msg[1] = col;
			message.msg[2] = riga_dest;
			message.msg[3] = col_dest;
			/*printf("%ld\n",map->mappa[riga][col].type_msg);*/
			sigfillset(&my_mask);
			sigprocmask(SIG_BLOCK, &my_mask, &old_mask);
			LOCK_STATISTICHE;
			if(msgsnd(qid, &message, MSG_LEN, IPC_NOWAIT)==-1){
				UNLOCK_STATISTICHE;
			}else{
				statistiche->tot++;
				statistiche->inevasi++;
				UNLOCK_STATISTICHE;
			}
			sigprocmask(SIG_SETMASK, &old_mask, NULL);
			/*printf("source %d %d riga dest : %d colonna dest: %d\n",message.msg[0], message.msg[1], message.msg[2], message.msg[3]);*/
			break;
		}else{
			cond = 1;
		}
	}while(cond==1);
	kill(pid,SIGUSR1);
}
