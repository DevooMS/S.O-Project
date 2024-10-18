#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>  
#include <sys/shm.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include "taxi_game.h"

int verifica_dati_inzio();
int verifica_taxi();
void sigHandler(int);
void timerHandler(int );
void kids_sources_gen(char * []);
void kids_taxi_gen(char * []);
void uscita(void);
void termina_figli();

int mid, mid_stat, sid, qid,  SO_TAXI, SO_SOURCES, SO_HOLES, SO_TOP_CELLS, SO_CAP_MIN, SO_CAP_MAX, SO_TIMENSEC_MIN, SO_TIMENSEC_MAX, SO_TIMEOUT;
unsigned int SO_DURATION;
struct shared * map;
struct stat * statistiche;
pid_t pid;
pid_t * kids_taxi, * kids_sources;

int main(int argc, char * argv[]){
	int i, j;
	char * args_sources[6], * args_taxi[7];
	char mid_str[3*sizeof(mid)+1]; 
	char mid_stat_str[3*sizeof(mid)+1]; 
	char sid_str[3*sizeof(sid)+1]; 
	char timeout_str[3*sizeof(SO_TIMEOUT)+1]; 
	char qid_str[3*sizeof(SO_TIMEOUT)+1]; 
	struct sembuf sops;
	struct sigaction sa;
	struct sigevent sevp;
	struct itimerspec timer;
	sigset_t  my_mask, old_mask;
	timer_t timerid;
	timer_t inutile;

	pid = getpid();
	
	/* lettura parametri */
	SO_TAXI = atoi(argv[1]);
	SO_SOURCES = atoi(argv[2]);
	SO_HOLES = atoi(argv[3]);
	SO_TOP_CELLS = atoi(argv[4]);
	SO_CAP_MIN = atoi(argv[5]);
	SO_CAP_MAX = atoi(argv[6]);
	SO_TIMENSEC_MIN = atoi(argv[7]); 
	SO_TIMENSEC_MAX = atoi(argv[8]);
	SO_TIMEOUT  = atoi(argv[9]);
	SO_DURATION = atoi(argv[10]);
	
	/* blocco segnali in fase di avvio*/
	sigfillset(&my_mask);   #set tutti i segnali
	sigprocmask(SIG_BLOCK, &my_mask, NULL); #maskero e blocco
	
	/* controllo che i dati siano validi, altrimenti termino */
	if(verifica_dati_inizio()){
		/*printf("master pid: %d\n",getpid());*/
		
		/* attribuzione handler per i segnali SIGALRM e SIGINT */
		sa.sa_handler = sigHandler;
		sa.sa_flags = 0;
		sigemptyset(&my_mask);        # set an empty mask
		sa.sa_mask = my_mask;

		/* Setting the handler for SIGINT,SIGALARM */	
		sigaction(SIGALRM, &sa, NULL);
		sigaction(SIGINT, &sa, NULL);
		
		/* attribuzione handler per iL segnale SIGUSR1 */
		sa.sa_handler = timerHandler;   #timerHandler e il funzione che andra poi chiamare 
		sa.sa_flags = 0;
		sigemptyset(&my_mask); 
		sa.sa_mask = my_mask;
		
		sigaction(SIGUSR1, &sa, NULL); #ascolta il messagge di tipo SIUSR1
	
			sevp.sigev_notify = SIGEV_SIGNAL; //configuro sigev_noify con sigev_signal per mandare un segnale scritto da sigev_signo in questo caso Il TIPO e SIGUSR1
          	sevp.sigev_signo = SIGUSR1; //
          	sevp.sigev_value.sival_ptr = & timerid; <<?
          	
          	/* creazione timer per stampa occupazione celle */ 
          	timer_create ( CLOCK_REALTIME, &sevp, &timerid);   //fa partire la funzione SEVP che invia SIGUS1
              	 
            timer.it_value.tv_sec = 1;  //quando avvia il timer aspetta 1 secondo e poi esegue la funzione .
           	timer.it_value.tv_nsec = 0;
           	timer.it_interval.tv_sec = 1;
           	timer.it_interval.tv_nsec = 0;
           	
              	/* creazione memoria condivisa per la mappa */ 	 
		mid = shmget(IPC_PRIVATE, sizeof(* map), 0600);
		
		if(mid==-1){
			printf("Errore durante la creazione della memoria condivisa 1\n");
			exit(EXIT_FAILURE); #id dello stack
		}
		
		map = shmat(mid, NULL, 0); #map punta id dello stack
		
		/* creazione memoria condivisa per le statistiche */ 		
		mid_stat = shmget(IPC_PRIVATE, sizeof(* statistiche), 0600);
		
		if(mid_stat==-1){
			printf("Errore durante la creazione della memoria condivisa 2\n");
			shmctl(mid, 0, IPC_RMID);
			exit(EXIT_FAILURE);
		}
		
		statistiche = shmat(mid_stat, NULL, 0); #int statistiche = mid_stat
		
		/* creazione set di semafori */ 
		sid = semget(IPC_PRIVATE, NUM_SEM, 0600);

		if(sid==-1){
			printf("Errore durante la creazione dei semafori\n");
			shmctl(mid, 0, IPC_RMID);
			shmctl(mid_stat, 0, IPC_RMID);
			exit(EXIT_FAILURE);
		}
		
		/* coda di messaggi per richieste taxi */
		qid = msgget(IPC_PRIVATE, 0600);

		if(qid==-1){
			printf("Errore durante la creazione della coda di messaggi\n");
			shmctl(mid, 0, IPC_RMID);
			shmctl(mid_stat, 0, IPC_RMID);
			semctl(sid, 0, IPC_RMID);
			exit(EXIT_FAILURE);
		}
		
		/* array dinamico per i pid dei processi sources */
		kids_sources = (pid_t *) calloc(SO_SOURCES,sizeof(pid_t)); //SO_SOURCES e NMEB,SIZEOF di PID_T SO_SOURCE QUANTI QUADRATI FARE DI GRANDEZZA SIZEOF E POI SONO DEI PUNTATORI DI TIPO PID_T
		
		/* array dinamico per i pid dei processi taxi */
		kids_taxi = (pid_t *) calloc(SO_TAXI, sizeof(pid_t));
		
		/*sblocco del segnale SIGTERM */
		sigfillset(&my_mask);
		sigdelset(&my_mask,SIGTERM);
		sigprocmask(SIG_SETMASK, &my_mask, NULL);
		
		/* all'uscita rimuove gli oggetti IPC */
		atexit(uscita);   //quando sto per terminare tutto chiamo.
		
		printf("Generazione mappa in corso. Attendere...\n");
		
		printf("mid %d sid %d qid %d", mid, sid, qid);
		
		inizializza_mappa(map, statistiche, sid);
		printf("Mappa inizializzata\n");
		genera_holes(map, SO_HOLES);
		printf("Holes creati\n");
		definisci_tempi(map, SO_TIMENSEC_MIN, SO_TIMENSEC_MAX);
		printf("Tempi percorrenza definiti\n");
		definisci_capienza(map, SO_CAP_MIN, SO_CAP_MAX, sid);
		printf("Capienze definite\n");

		/* verifico se la mappa può contenere i taxi, altrimenti termino */
		if(verifica_taxi()){
			/* semaforo per sincronizzazione figli */
			semctl(sid, SEM_MASTER, SETVAL, 1);	
			//int semctl(int semid, int semnum, int cmd    //id tutto l'array del semaforo semnum rapressenta identificativo dell array e assegno 1 in quel array
			/* semaforo per la creazione dei processi sources */
			semctl(sid, SEM_SOURCES, SETVAL, SO_SOURCES); 	
			
			/* semaforo per accesso alla mappa */
			semctl(sid, SEM_MEM, SETVAL, 1);
			
			/* semaforo per accesso alle statistiche*/
			semctl(sid, SEM_STATISTICHE, SETVAL, 1);	
			
			/* semaforo per la stampa */
			semctl(sid, SEM_STAMPA, SETVAL, 0);  
		
			/* parametri per i processi sources */
			sprintf(mid_str, "%d", mid); 
			sprintf(mid_stat_str, "%d", mid_stat);
			sprintf(sid_str, "%d", sid);
			sprintf(qid_str, "%d", qid);
			args_sources[0] = "sources";
			args_sources[1] = mid_str;
			args_sources[2] = mid_stat_str;
			args_sources[3] = sid_str;
			args_sources[4] = qid_str;
			args_sources[5] = NULL;
			
			printf("Inizializzazione sources in corso. Attendere...\n");
			
			kids_sources_gen(args_sources);
			
			/* attendo che i processi sources siano stati creati */
			sops.sem_num = SEM_SOURCES;
			sops.sem_op = 0;
			sops.sem_flg = 0;
			semop(sid, &sops, 1);
			
			printf("Sources inizializzate\n");
			
			/* semaforo per la creazione dei processi taxi */
			semctl(sid, SEM_TAXI, SETVAL, SO_TAXI); 	
		
			/* parametri per i processi taxi */	
			sprintf(timeout_str, "%d", SO_TIMEOUT);
			args_taxi[0] = "taxi";
			args_taxi[1] = mid_str;
			args_taxi[2] = mid_stat_str;
			args_taxi[3] = sid_str;
			args_taxi[4] = qid_str;
			args_taxi[5] = timeout_str;
			args_taxi[6] = NULL;
			
			printf("Inizializzazione taxi in corso. Attendere...\n");
			
			kids_taxi_gen(args_taxi);
			
			/* attendo che i processi taxi siano stati creati */
			sops.sem_num = SEM_TAXI;
			sops.sem_op = 0;
			sops.sem_flg = 0;
			semop(sid, &sops, 1);
			
			printf("Taxi inizializzati\n");
			
			/* inizio la simulazione */
			sops.sem_num = SEM_MASTER;
			sops.sem_op = -1;
			sops.sem_flg = 0;
			semop(sid, &sops, 1);	
			
			printf("Inizio simulazione!\n");
			
			/* sblocco i segnali per l'esecuzione */
			sigfillset(&my_mask);
			sigdelset(&my_mask,SIGINT);
			sigdelset(&my_mask,SIGALRM);
			sigdelset(&my_mask,SIGUSR1);
			sigprocmask(SIG_SETMASK, &my_mask, NULL);
			
			/* allarme per la durata della simulazione */
			alarm(SO_DURATION);

			/* avvio timer per la stampa occupazione celle */
			timer_settime (timerid, 0, &timer, NULL);
			
			/* ciclo per generare nuovi processi taxi quando terminano */
			for(i=0; i <= SO_TAXI; i++){ 	
				if(i == SO_TAXI)
					i=0;
					
				sigfillset(&my_mask);
				sigprocmask(SIG_BLOCK, &my_mask, &old_mask);	
				
				if(waitpid(kids_taxi[i],NULL,WNOHANG) != 0){
					switch(kids_taxi[i] = fork()){
						case -1:   
							printf("Errore durante la creazione dei processi taxi\n");
							kill(pid, SIGTERM);
							break;
						case 0:
							execve("./taxi",args_taxi,NULL); 
							
							break;
						default:
							break;
					}
				}
				
				sigprocmask(SIG_SETMASK, &old_mask, NULL);
			}
		}else{
			exit(EXIT_FAILURE);
		}
	}else{
		exit(EXIT_FAILURE);
	}
}

/* controllo dati iniziale */
int verifica_dati_inizio(){
	if(SO_TAXI < 1){							//Quantita di Taxi
		printf("SO_TAXI  deve essere >0!\n");
		return 0;
	}else if(SO_SOURCES < 1){
		printf("SO_SOURCES deve essere >0!\n");	//Quantita di Sources
		return 0;
	}else if(SO_SOURCES > (SO_HEIGHT*SO_WIDTH)-SO_HOLES){
		printf("SO_SOURCES deve essere <= (SO_HEIGHT*SO_WIDTH)-SO_HOLES!\n");
		return 0;
	}else if(SO_HOLES >= ((SO_HEIGHT/2)*(SO_WIDTH/2))){
		printf("SO_HOLES troppo elevato!\n");
		return 0;
	}else if(SO_HOLES < 0 ){					//Quantita di HOLES
		printf("SO_HOLES deve essere >= 0\n");
		return 0;
	}else if(SO_CAP_MIN < 0){					//Capacita massimo di ogni cella 
		printf("SO_CAP_MIN deve essere >=0!\n");
		return 0;
	}else if(SO_CAP_MAX < 0){
		printf("SO_CAP_MAX  deve essere >=0!\n");
		return 0;
	}else if(SO_CAP_MAX < SO_CAP_MIN){
		printf("SO_CAP_MAX  deve essere >= della capacità minima!\n");
		return 0;
	}else if(SO_TIMENSEC_MIN < 0){           	//TEMPO MINIMO E MAX di attraversamento di ogni cella.
		printf("SO_TIMENSEC_MIN deve essere >= 0!\n");
		return 0;
	}else if(SO_TIMENSEC_MAX < 0){
		printf("SO_TIMENSEC_MAX deve essere >= 0!\n");
		return 0;
	}else if(SO_TIMENSEC_MAX < SO_TIMENSEC_MIN){	
		printf("SO_TIMENSEC_MAX deve essere >= SO_TIMENSEC_MIN!\n");
		return 0;
	}else if(SO_TIMEOUT < 1){					//tempo di inativita del taxi dopo il quale il taxi muore 
		printf("SO_TIMEOUT deve essere >= 1\n");
		return 0;
	}else if(SO_DURATION < 1){					//tempo della durata della simulazione.
		printf("SO_DURATION deve essere >= 1\n");
		return 0;
	}else 
		return 1;
}

/* controllo se la mappa può contenere tutti i taxi */
int verifica_taxi(){
	int i, j, tot_cap=0;
	
	for(i=0;i<SO_HEIGHT;i++){
		for(j=0;j<SO_WIDTH;j++){
			tot_cap = tot_cap + semctl(sid, map->mappa[i][j].id_sem, GETVAL);
		}
	}
	
	if(SO_TAXI > tot_cap){
		printf("Il numero di taxi è troppo elevato, avviare un'altra simulazione oppure diminuire il numero di taxi!\n");
		return 0;
	}else
		return 1;
}

/* handler segnali SIGALRM e SIGTERM */	
void sigHandler(int sig){								//termina figlio come handler 
	termina_figli();
	
	stampa_finale(map, statistiche, SO_TOP_CELLS);
	
	exit(EXIT_SUCCESS);
}

void termina_figli(){
	int i;
	struct sembuf sops;
	sigset_t my_mask;
	
	sigfillset(&my_mask);
	sigprocmask(SIG_BLOCK, &my_mask, NULL);
	
	printf("\nCalcolo statistiche in corso. Attendere...\n");
	
	for(i=0; i < SO_SOURCES; i++){							//kids_sources[] contiene i pid dei figli
		if(kids_sources[i]>0){
			kill(kids_sources[i], SIGTERM);
			/*printf("kill: %d\n",kids_sources[i]);*/
		}else
			break;
	}
	
	sops.sem_num = SEM_STAMPA;
	sops.sem_op = 0;  #aspetto tutti semafori sia 0
	sops.sem_flg = 0;
	semop(sid, &sops, 1);
	
	for(i=0; i < SO_TAXI; i++){
		if(kids_taxi[i]>0){
			kill(kids_taxi[i], SIGTERM);
			/*printf("kill: %d\n",kids_taxi[i]);*/
		}else
			break;
	}
	
	sops.sem_num = SEM_STAMPA;
	sops.sem_op = 0;
	sops.sem_flg = 0;
	semop(sid, &sops, 1);
}

/* azione di default all'uscita */
void uscita(void){
	semctl(sid, 0, IPC_RMID);					//semctl IPC_RMID rimuovo il semaforo il set di semaforo quando faccio RMID ignoro lo 0
	shmctl(mid, 0, IPC_RMID);					
	shmctl(mid_stat, 0, IPC_RMID);
	msgctl(qid, IPC_RMID, NULL);
}

/*handler segnale SIGUSR1 */
void timerHandler(int sig){
	stampa_dati(map,sid);
}

/* metodo che genera i processi sources */
void kids_sources_gen(char * args_sources[5]){
	int i;
	
	for(i=0; i < SO_SOURCES; i++){
		switch(kids_sources[i] = fork()){
			case -1:   
				printf("Errore durante la creazione dei processi sources\n");
				kill(pid, SIGTERM);
				break;
			case 0:
				
				execve("./sources",args_sources,NULL); 
				
				printf("Errore durante la creazione dei processi sources\n");
				kill(pid, SIGTERM);
				break;
			default:
				break;
		}
	}
}

/* metodo che genera i processi taxi */
void kids_taxi_gen(char * args_taxi[6]){
	int i;
	
	for(i=0; i < SO_TAXI; i++){
		switch(kids_taxi[i] = fork()){
			case -1:   
				printf("Errore durante la creazione dei processi taxi\n");
				kill(pid, SIGTERM);
				break;
			case 0:
				execve("./taxi",args_taxi,NULL); 
				
				printf("Errore durante la creazione dei processi taxi\n");
				kill(pid, SIGTERM);
				break;
			default:
				break;
		}
	}
}
