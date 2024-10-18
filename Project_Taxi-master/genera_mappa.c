#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/sem.h>  
#include "taxi_game.h"

	/* inizializzazione della mappa */
void inizializza_mappa(struct shared * map, struct stat * statistiche,int sid){
	int i, j;
	long k=1;
	
	statistiche->tot = 0;
	statistiche->successo = 0;
	statistiche->inevasi = 0;
	statistiche->abortiti = 0;
	statistiche->max_numc = 0;
	statistiche->max_numr = 0;
	
	for(i=0; i<SO_HEIGHT;i++){
		for(j=0;j<SO_WIDTH;j++){
			map->mappa[i][j].type = NOT_HOLES;
			map->mappa[i][j].type_msg = k;
			map->mappa[i][j].id_sem = k-1;	
			map->mappa[i][j].count = 0;	
			map->mappa[i][j].stampa = 0;
			k++;
		}
	}
}

/* generazione delle holes */
void genera_holes(struct shared * map, int SO_HOLES){
	int i, j, k=0,riga=0, col=0, cond;
	
	srand(time(NULL));
	
	while(k<SO_HOLES){
		do{
			cond=0;
			riga = rand()%SO_HEIGHT;
			col = rand()%SO_WIDTH;
			
			if(map->mappa[riga][col].type==NOT_HOLES){
				for(i=riga-1;i<=riga+1 && !cond;i++){
					for(j=col-1;j<=col+1 && !cond;j++){
						if(i>=0 && i<SO_HEIGHT && j>=0 && j<SO_WIDTH && map->mappa[i][j].type==HOLES)
							cond=1;
					}
				}
				if(!cond) 
					map->mappa[riga][col].type = HOLES;
			}else
				cond = 1;
			
		}while(cond==1);
		
		k++;
	}
	/*map->mappa[1][0].type = HOLES;*/
}

/* definizione dei tempi di percorrenza delle celle */
void definisci_tempi(struct shared * map, int SO_TIMENSEC_MIN, int SO_TIMENSEC_MAX){
	int i, j;
	
	srand(time(NULL));
	
	for(i=0;i<SO_HEIGHT;i++){
		for(j=0;j<SO_WIDTH;j++){
			if(map->mappa[i][j].type != HOLES)
				map->mappa[i][j].tempo_attr = (rand() % (SO_TIMENSEC_MAX - SO_TIMENSEC_MIN + 1)) + SO_TIMENSEC_MIN;
			else
				map->mappa[i][j].tempo_attr = 0;
		}
	}
}

/* definizione della capienza delle celle */
void definisci_capienza(struct shared * map, int SO_CAP_MIN, int SO_CAP_MAX, int sid){
	int i, j;
	
	srand(time(NULL));
	
	for(i=0;i<SO_HEIGHT;i++){
		for(j=0;j<SO_WIDTH;j++){
			if(map->mappa[i][j].type != HOLES){
				map->mappa[i][j].capacita = (rand() % (SO_CAP_MAX - SO_CAP_MIN + 1)) + SO_CAP_MIN;
			}else{
				map->mappa[i][j].capacita = 0;
			}
			/* inizializzazione semaforo per cella [i][j] */
			semctl(sid, map->mappa[i][j].id_sem, SETVAL, map->mappa[i][j].capacita); 
		}
	}
}

/* calcolo delle SO_TOP_CELLS */
void calcola_top(struct shared * map, int SO_TOP_CELLS){
	int i, j, i_min, j_min,  j_temp, k, k_temp;

	i_min = 0;
	j_min = 0;
	
	for(i=0; i<SO_HEIGHT;i++){
		for(j=0; j<SO_WIDTH;j++){
			if(map->mappa[i][j].type != HOLES && map->mappa[i][j].count <= map->mappa[i_min][j_min].count ){
				i_min = i;
				j_min = j;
			} 
		}
	}
	
	for(i=0; i<SO_TOP_CELLS;i++){
		j_temp = i_min;
		k_temp = j_min;
		
		for(j=0; j<SO_HEIGHT;j++){
			for(k=0; k<SO_WIDTH;k++){
				if(map->mappa[j][k].type != HOLES && map->mappa[j][k].stampa == 0 && map->mappa[j][k].count >= map->mappa[j_temp][k_temp].count){
					j_temp = j;
					k_temp = k;
				}
			}
		}
		map->mappa[j_temp][k_temp].stampa = 1;
	}
}

/* stampa della mappa e delle statistiche */
void stampa_finale(struct shared * map, struct stat * statistiche, int SO_TOP_CELLS){
	int i, j, k, i_temp, j_temp;
	long  count;
	
	printf("\nStatistiche finali\n");
	
	calcola_top(map, SO_TOP_CELLS);
	printf("\nRichieste totali: %ld\n",statistiche->tot);
	printf("\nSuccesso: %ld\n",statistiche->successo);
	printf("Inevasi: %ld\n",statistiche->inevasi);
	printf("Abortiti: %ld\n",statistiche->abortiti);
	printf("\nIl processo taxi %d ha percorso il max num. di celle: %ld\n",statistiche->max_c,statistiche->max_numc);
	printf("Il processo taxi %d ha fatto il viaggio più lungo: %.4f secondi\n",statistiche->max_pidt,statistiche->max_temp);
	printf("Il processo taxi %d ha raccolto più richieste: %ld\n",statistiche->max_r,statistiche->max_numr);
	
	printf("\n");
	
	for(j=0; j<SO_WIDTH; j++)
	{
	    printf("--------");
	}
	printf("-\n");

    	for(i=0; i<SO_HEIGHT; i++)
    	{
    		for(j=0; j<SO_WIDTH; j++)
		{
				if(map->mappa[i][j].type == HOLES)
					printf("|   * \t");
				else if(map->mappa[i][j].type == SOURCES && map->mappa[i][j].stampa == 1)
					printf("|  S/T \t");
				else if(map->mappa[i][j].type == SOURCES)
					printf("|   S \t");
				else if(map->mappa[i][j].stampa == 1)
					printf("|   T \t");
				else
					printf("| \t");
		}
  		printf("|\n");
  		if(i != SO_HEIGHT-1)
  		{
    			for(j=0; j<SO_WIDTH; j++)
			{
		    		printf("|-------");
			}
  			printf("|\n");
		}		
	}	
	
	for(j=0; j<SO_WIDTH; j++)
	{
	    printf("--------");
	}
	printf("-\n");
	
	printf("Numero di accessi celle TOP CELLS\n");
	
	count = 0;
	
	for(i=0; i<SO_HEIGHT;i++){
		for(j=0; j<SO_WIDTH;j++){
			if(map->mappa[i][j].count > count){
				count = map->mappa[i][j].count;
			} 
		}
	}
	
	for(i=0; i<SO_TOP_CELLS;i++){
		for(j=0; j<SO_HEIGHT;j++){
			for(k=0; k<SO_WIDTH;k++){
				if(map->mappa[j][k].count == count && map->mappa[j][k].stampa == 1){
					printf("\n(%d %d): %ld \n",j,k,map->mappa[j][k].count);
					map->mappa[j][k].stampa = 0;
				}
			}
		}
		count--;
	}
}

/* stampa occupazione celle */
void stampa_dati(struct shared * map, int sid){
	int i, j, num_taxi;
	struct sembuf sops;
			
	printf("\nStato occupazione celle: ""(cella): num.taxi""\n");
	
	for(i=0; i < SO_HEIGHT; i++){
		for(j=0; j < SO_WIDTH; j++){
			num_taxi = map->mappa[i][j].capacita-semctl(sid, map->mappa[i][j].id_sem, GETVAL);
			printf("(%d,%d): %d ", i, j, num_taxi);
		}
		printf("\n");
	}
}
