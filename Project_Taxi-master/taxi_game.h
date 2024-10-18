#define SO_HEIGHT 10	/*altezza*/
#define SO_WIDTH 20	/*larghezza*/
#define NOT_HOLES 0
#define HOLES 1
#define LIBERO 0
#define OCCUPATO 1
#define SOURCES 2
#define MSG_LEN sizeof(int[4])	/* grandezza dei messaggi */

/* numero dei semafori*/
#define NUM_SEM 	   (SO_HEIGHT*SO_WIDTH)+6
#define SEM_MASTER		SO_HEIGHT*SO_WIDTH
#define SEM_SOURCES 	SO_HEIGHT*SO_WIDTH+1
#define SEM_TAXI 		SO_HEIGHT*SO_WIDTH+2
#define SEM_MEM 		SO_HEIGHT*SO_WIDTH+3
#define SEM_STATISTICHE SO_HEIGHT*SO_WIDTH+4
#define SEM_STAMPA 		SO_HEIGHT*SO_WIDTH+5

#define LOCK						\
	sops.sem_num = SEM_MEM;			\
	sops.sem_op = -1;				\
	sops.sem_flg = 0;				\
	semop(sid, &sops, 1);
	
#define UNLOCK						\
	sops.sem_num = SEM_MEM;				\
	sops.sem_op = 1;				\
	sops.sem_flg = 0;				\
	semop(sid, &sops, 1);
	
#define LOCK_STATISTICHE				\
	sops_stat.sem_num = SEM_STATISTICHE;		\
	sops_stat.sem_op = -1;				\
	sops_stat.sem_flg = 0;				\
	semop(sid, &sops_stat, 1);
	
#define UNLOCK_STATISTICHE				\
	sops_stat.sem_num = SEM_STATISTICHE;		\
	sops_stat.sem_op = 1;				\
	sops_stat.sem_flg = 0;				\
	semop(sid, &sops_stat, 1);

/* struttura celle mappa */
typedef struct{
	int type;
	long tempo_attr;
	int capacita;
	int id_sem;
	long type_msg;
	long count;
	int stampa;
}point;

struct shared{
	point mappa[SO_HEIGHT][SO_WIDTH];
	 
};

/* struttura statistiche */
struct stat{
	long tot;
	long successo;
	long inevasi;
	long abortiti;
	pid_t max_c;
	long int max_numc;
	pid_t max_r;
	long int max_numr;
	pid_t max_pidt;
	float max_temp;
};

/* struttura messaggi */
struct msg_ric {
	long mtype;       
	int msg[4];
};
