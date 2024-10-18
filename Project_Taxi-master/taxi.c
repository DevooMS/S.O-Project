#define _GNU_SOURCE
#include<limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>   
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h> 
#include <sys/shm.h>
#include <sys/msg.h>
#include "taxi_game.h"

void genera_taxi();
void ricevi_mess(int);
void muovi_taxi();
void libera_aggiorna();
void sigusr1Taxi(int );
void operazione();

int riga, col, sid, qid, pos, sr_dest, r_dest, sc_dest, c_dest, SO_TIMEOUT, status, num_celle, num_rich;
float tempo;
struct sembuf sops;
struct sembuf sops_stat;
struct shared * map;
struct stat * statistiche;
pid_t pid;

int main(int argc, char * argv[]) {
	int mid, mid_stat;
	struct sembuf sops;
	struct sigaction sa;
	sigset_t  my_mask;
	
	/* blocco dei segnali iniziale */
	sigfillset(&my_mask);
	sigprocmask(SIG_BLOCK, &my_mask, NULL);
	
	/* lettura parametri */
	mid = atoi(argv[1]);
	mid_stat = atoi(argv[2]);
	sid = atoi(argv[3]);
	qid = atoi(argv[4]);
	SO_TIMEOUT = atoi(argv[5]);
	
	/* attribuzione handler segnale SIGUSR1 */
	sa.sa_handler = sigusr1Taxi;
	sa.sa_flags = 0;
	sigemptyset(&my_mask);       
	sa.sa_mask = my_mask;
	
	sigaction(SIGUSR1, &sa, NULL);
	
	/* attribuzione handler segnale SIGTERM */
	sa.sa_handler = libera_aggiorna;
	sa.sa_flags = 0;
	sigemptyset(&my_mask);       
	sa.sa_mask = my_mask;
	
	sigaction(SIGTERM, &sa, NULL);
	
	/* aggancio memoria condivisa mappa*/
	map = shmat(mid, NULL, 0);
	
	/* aggancio memoria condivisa statistiche */
	statistiche = shmat(mid_stat, NULL, 0);
	
	genera_taxi(map);
	
	pid = getpid();
	/*printf("taxi %d %d %d\n",getpid(),riga, col);*/
	
	/* sblocco segnali per l'esecuzione */
	sigfillset(&my_mask);
	sigdelset(&my_mask,SIGUSR1);
	sigdelset(&my_mask,SIGTERM);
	sigprocmask(SIG_SETMASK, &my_mask, NULL);
	
	/* Informo il master che il taxi è stato generato */
	sops.sem_num = SEM_TAXI;
	sops.sem_op = -1;
	sops.sem_flg = 0;
	semop(sid, &sops, 1);
	
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
	
	kill(pid, SIGUSR1);
}

/* handler segnale SIGTERM */
void libera_aggiorna(){
	sigset_t  my_mask;
	
	sigfillset(&my_mask);
	sigprocmask(SIG_BLOCK, &my_mask, NULL);
	
	LOCK_STATISTICHE;

	if(status == OCCUPATO)
		statistiche->abortiti++;

	if(num_celle >= statistiche->max_numc){
		statistiche->max_c = getpid();
		statistiche->max_numc = num_celle;
	}

	if(num_rich >= statistiche->max_numr){
		statistiche->max_r = getpid();
		statistiche->max_numr = num_rich;
	}

	UNLOCK_STATISTICHE;
		
	sops.sem_num = SEM_TAXI;
	sops.sem_op = 1;
	sops.sem_flg = 0;
	semop(sid, &sops, 1);

	sops.sem_num = map->mappa[riga][col].id_sem;
	sops.sem_op = 1;
	sops.sem_flg = 0;
	semop(sid, &sops, 1);
	
	sops.sem_num = SEM_STAMPA;
	sops.sem_op = -1;
	sops.sem_flg = 0;
	semop(sid, &sops, 1);
	
	/*printf("morto %d %d %d\n",getpid(),riga,col);*/
	exit(EXIT_SUCCESS);
}

/* handler segnale SIGUSR1 */
void sigusr1Taxi(int sig){
	operazione();
}

/* generazione posizione taxi */
void genera_taxi(){
	int cond;
	struct timespec rand_riga;
	struct timespec rand_col;
	struct timespec timeout;
	
	tempo = 0;
	timeout.tv_sec=0;
	timeout.tv_nsec= 500000000;
	
	do{
		cond = 0;
		clock_gettime(CLOCK_REALTIME ,&rand_riga);
		riga = rand_riga.tv_nsec%SO_HEIGHT;
		clock_gettime(CLOCK_REALTIME ,&rand_col);
		col = rand_col.tv_nsec%SO_WIDTH;
		
		if(map->mappa[riga][col].type != HOLES){
			sops.sem_num = map->mappa[riga][col].id_sem;
			sops.sem_op = -1;
			sops.sem_flg = 0;
			
			if(semtimedop(sid, &sops, 1, &timeout) == -1){
				if(errno == EAGAIN)
					cond = 1;
			}
		}else
			cond = 1;
	}while(cond == 1);
	
	status = LIBERO;
	num_celle = 0;
	num_rich = 0;
}

/* scelta di quale messaggio prelevare */
void operazione(){
	float tempo_parz;
	sigset_t  my_mask, old_mask;
	struct timespec rand_riga;
	struct timespec rand_col;
	
	if(map->mappa[riga][col].type == SOURCES){
		ricevi_mess(map->mappa[riga][col].type_msg);
		
		muovi_taxi(r_dest, c_dest); 
		
		sigfillset(&my_mask);
		sigprocmask(SIG_BLOCK, &my_mask, &old_mask);
		
		LOCK_STATISTICHE;
		statistiche->successo++;
		if(tempo > statistiche->max_temp){
			statistiche->max_pidt = getpid();
			statistiche->max_temp = tempo;
		}
		UNLOCK_STATISTICHE;
		
		status = LIBERO;
		
		num_rich++;
		
		sigprocmask(SIG_SETMASK, &old_mask, NULL);
		
		/*printf("succ\n");*/
	}else{
		ricevi_mess(0);
		
		muovi_taxi(sr_dest, sc_dest);
		tempo_parz = tempo;
		/*printf("%f\n",tempo_parz);*/
		muovi_taxi(r_dest, c_dest);
		/*printf("%f\n",tempo);*/
		tempo = tempo_parz + tempo;
		/*printf("%f\n",tempo);*/
		
		sigfillset(&my_mask);
		sigprocmask(SIG_BLOCK, &my_mask, &old_mask);
		
		LOCK_STATISTICHE;
		statistiche->successo++;
		if(tempo > statistiche->max_temp){
			statistiche->max_pidt = getpid();
			statistiche->max_temp = tempo;
		}
		UNLOCK_STATISTICHE;
		
		status = LIBERO;
		
		num_rich++;
		
		sigprocmask(SIG_SETMASK, &old_mask, NULL);
		
		/*printf("succ\n");*/
	}
	kill(pid, SIGUSR1);
}

/* lettura messaggio dalla coda di messaggi */
void ricevi_mess(int type){
	sigset_t  my_mask, old_mask;
	struct msg_ric message;
	
	sigfillset(&my_mask);
	sigprocmask(SIG_BLOCK, &my_mask, &old_mask);
	
	LOCK_STATISTICHE;
	
	if(msgrcv(qid, &message, MSG_LEN, type, IPC_NOWAIT)==-1){
		UNLOCK_STATISTICHE;
		sigprocmask(SIG_SETMASK, &old_mask, NULL);
		kill(pid,SIGTERM);
	}else{
		statistiche->inevasi--;
		UNLOCK_STATISTICHE;
		status = OCCUPATO;
		
		sigprocmask(SIG_SETMASK, &old_mask, NULL);
	
		sr_dest = message.msg[0];
		sc_dest = message.msg[1];
		r_dest = message.msg[2];
		c_dest = message.msg[3];
		/*printf("taxi %d source %d %d riga dest : %d colonna dest: %d\n",getpid(),sr_dest, sc_dest, r_dest,c_dest);*/
	}
}

void muovi_taxi(riga_dest, col_dest){
	int cond, r_move, c_move, i;
	sigset_t  my_mask, old_mask;
	struct timespec start, end;
	struct sembuf sops_m[2];
	struct timespec move;
	struct timespec timeout;
	
	sigfillset(&my_mask);
	
	timeout.tv_sec=SO_TIMEOUT;
	timeout.tv_nsec=0; 
	
	clock_gettime( CLOCK_REALTIME, &start);
	
	/*printf("%d partenza %d %d\n",getpid(),riga, col);*/
	
	r_move = riga;
	c_move = col;
	
	while(riga != riga_dest){
		move.tv_sec = 0;
		move.tv_nsec = map->mappa[riga][col].tempo_attr;
		nanosleep(&move, NULL);
		/*printf("%d move %d %d\n",getpid(),riga, col);*/
		
		num_celle++;
			
		if(riga > riga_dest){
			r_move--;
				
			if(map->mappa[r_move][c_move].type == HOLES){
				r_move++;
					
				if(c_move <= col_dest){
					if(c_move+1 < SO_WIDTH)
						c_move++;
					else
							c_move--;
				}else if(c_move >= col_dest){
					if(c_move-1 >= 0)
						c_move--;
					else
						c_move++;
				}
					
				sops_m[0].sem_num = map->mappa[r_move][c_move].id_sem;
				sops_m[0].sem_op = -1;
				sops_m[0].sem_flg = 0;
				sops_m[1].sem_num = map->mappa[riga][col].id_sem;
				sops_m[1].sem_op = 1;
				sops_m[1].sem_flg = 0;
					//faccio questi operazione contemporaneamente.
				if(semtimedop(sid, sops_m, 2,&timeout)==-1){
					kill(pid,SIGTERM);
				}
			}else{
				sops_m[0].sem_num = map->mappa[r_move][c_move].id_sem;
				sops_m[0].sem_op = -1;
				sops_m[0].sem_flg = 0;
				sops_m[1].sem_num = map->mappa[riga][col].id_sem;
				sops_m[1].sem_op = 1;
				sops_m[1].sem_flg = 0;
					
				if(semtimedop(sid, sops_m, 2,&timeout)==-1){
					kill(pid,SIGTERM);
				}
			}
		}else{	
			r_move++;
				
			if(map->mappa[r_move][c_move].type == HOLES){
				r_move--;
				
				if(c_move <= col_dest){
					if(c_move+1 < SO_WIDTH)
						c_move++;
					else
						c_move--;
				}else if(c_move >= col_dest){
					if(c_move-1 >= 0)
						c_move--;
					else
						c_move++;
				}
				
				sops_m[0].sem_num = map->mappa[r_move][c_move].id_sem;
				sops_m[0].sem_op = -1;
				sops_m[0].sem_flg = 0;
				sops_m[1].sem_num = map->mappa[riga][col].id_sem;
				sops_m[1].sem_op = 1;
				sops_m[1].sem_flg = 0;
					
				if(semtimedop(sid, sops_m, 2,&timeout)==-1){
					kill(pid,SIGTERM);
				}
				
			}else{
				sops_m[0].sem_num = map->mappa[r_move][c_move].id_sem;
				sops_m[0].sem_op = -1;
				sops_m[0].sem_flg = 0;
				sops_m[1].sem_num = map->mappa[riga][col].id_sem;
				sops_m[1].sem_op = 1;
				sops_m[1].sem_flg = 0;
					
				if(semtimedop(sid, sops_m, 2,&timeout)==-1){
					kill(pid,SIGTERM);
				}
			}
				
		}
		riga = r_move;
		col = c_move;
		
		sigprocmask(SIG_BLOCK, &my_mask, &old_mask);
		
		LOCK;
		map->mappa[riga][col].count++;
		UNLOCK;
		
		sigprocmask(SIG_SETMASK, &old_mask, NULL);
		
		/*printf("%d pos %d %d\n",getpid(),riga,col);*/
	}
	
	while(col != col_dest){
		move.tv_sec = 0;
		move.tv_nsec = map->mappa[riga][col].tempo_attr;
		nanosleep(&move, NULL);
		/*printf("%d move %d %d\n", getpid(), riga, col);*/
		
		num_celle++;
		
		r_move = riga;
		c_move = col;
		
		if(col < col_dest)
			c_move++;
		else
			c_move --;
			
		if(map->mappa[r_move][c_move].type == HOLES){
			if(c_move < col_dest){
				c_move--;
				if(r_move-1 >= 0)
					r_move--;
				else
					r_move++;
				
				i=0;
				while(i<3){
					sops_m[0].sem_num = map->mappa[r_move][c_move].id_sem;
					sops_m[0].sem_op = -1;
					sops_m[0].sem_flg = 0;
					sops_m[1].sem_num = map->mappa[riga][col].id_sem;
					sops_m[1].sem_op = 1;
					sops_m[1].sem_flg = 0;
					
					if(semtimedop(sid, sops_m, 2,&timeout)==-1){
						kill(pid,SIGTERM);
					}
					
					riga = r_move;
					col = c_move; 
					
					sigprocmask(SIG_BLOCK, &my_mask, &old_mask);
					
					LOCK;
					map->mappa[riga][col].count++;
					UNLOCK;
					
					sigprocmask(SIG_SETMASK, &old_mask, NULL);
					
					/*printf("%d pos %d %d\n",getpid(),riga, col);*/
					
					move.tv_sec = 0;
					move.tv_nsec = map->mappa[riga][col].tempo_attr;
					nanosleep(&move, NULL);
					/*printf("%d move %d %d\n",getpid(),riga,col);*/
					
					num_celle++;
					
					c_move++;
					
					i++;
				}
				c_move--;
				riga = r_move;
				r_move = riga_dest;
				
				sops_m[0].sem_num = map->mappa[r_move][c_move].id_sem;
				sops_m[0].sem_op = -1;
				sops_m[0].sem_flg = 0;
				sops_m[1].sem_num = map->mappa[riga][col].id_sem;
				sops_m[1].sem_op = 1;
				sops_m[1].sem_flg = 0;
					
				if(semtimedop(sid, sops_m, 2,&timeout)==-1){
					kill(pid,SIGTERM);
				}
					
				riga = r_move;
				col = c_move; 
				
				sigprocmask(SIG_BLOCK, &my_mask, &old_mask);
				
				LOCK;
				map->mappa[riga][col].count++;
				UNLOCK;
				
				sigprocmask(SIG_SETMASK, &old_mask, NULL);
				
				/*printf("%d pos %d %d\n",getpid(),riga, col);*/
			}else{
				c_move++;
				if(r_move-1 >= 0)
					r_move--;
				else
					r_move++;
				
				i=0;
				while(i<3){
					sops_m[0].sem_num = map->mappa[r_move][c_move].id_sem;
					sops_m[0].sem_op = -1;
					sops_m[0].sem_flg = 0;
					sops_m[1].sem_num = map->mappa[riga][col].id_sem;
					sops_m[1].sem_op = 1;
					sops_m[1].sem_flg = 0;
					
					if(semtimedop(sid, sops_m, 2,&timeout)==-1){
						kill(pid,SIGTERM);
					}
					
					riga = r_move;
					col = c_move; 
					
					sigprocmask(SIG_BLOCK, &my_mask, &old_mask);
					
					LOCK;
					map->mappa[riga][col].count++;
					UNLOCK;
					
					sigprocmask(SIG_SETMASK, &old_mask, NULL);
					
					/*printf("%d pos %d %d\n",getpid(),riga, col);*/
					
					move.tv_sec = 0;
					move.tv_nsec = map->mappa[riga][col].tempo_attr;
					nanosleep(&move, NULL);
					/*printf("%d move %d %d\n",getpid(), riga,col);*/
					
					num_celle++;
					
					c_move--;
					
					i++;
				}
				c_move++;
				riga = r_move;
				r_move = riga_dest;
					
				sops_m[0].sem_num = map->mappa[r_move][c_move].id_sem;
				sops_m[0].sem_op = -1;
				sops_m[0].sem_flg = 0;
				sops_m[1].sem_num = map->mappa[riga][col].id_sem;
				sops_m[1].sem_op = 1;
				sops_m[1].sem_flg = 0;
					
				if(semtimedop(sid, sops_m, 2,&timeout)==-1){
					kill(pid,SIGTERM);
				}
					
				riga = r_move;
				col = c_move; 
				
				sigprocmask(SIG_BLOCK, &my_mask, &old_mask);
				
				LOCK;
				map->mappa[riga][col].count++;
				UNLOCK;
				
				sigprocmask(SIG_SETMASK, &old_mask, NULL);
				
				/*printf("%d pos %d %d\n",getpid(),riga, col);*/
			}
		}else{
				sops_m[0].sem_num = map->mappa[r_move][c_move].id_sem;
				sops_m[0].sem_op = -1;
				sops_m[0].sem_flg = 0;
				sops_m[1].sem_num = map->mappa[riga][col].id_sem;
				sops_m[1].sem_op = 1;
				sops_m[1].sem_flg = 0;
					
				if(semtimedop(sid, sops_m, 2,&timeout)==-1){
					kill(pid,SIGTERM);
				}
					
				riga = r_move;
				col = c_move; 
				
				sigprocmask(SIG_BLOCK, &my_mask, &old_mask);
				
				LOCK;
				map->mappa[riga][col].count++;
				UNLOCK;
				
				sigprocmask(SIG_SETMASK, &old_mask, NULL);
				
				/*printf("%d pos %d %d\n",getpid(),riga, col);*/
		}
	}
	clock_gettime( CLOCK_REALTIME, &end);
	
	tempo = (end.tv_sec - start.tv_sec) + (1e-9)*(end.tv_nsec - start.tv_nsec);
	
	/*printf("%d %f\n",getpid(),tempo);*/
	/*printf("%d pos attuale %d %d, arrivato\n", getpid(), riga, col);*/
}
