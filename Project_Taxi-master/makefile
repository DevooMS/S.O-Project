#flag per la compilazione
CFLAGS = -std=c89 -pedantic


SO_TAXI = 95
SO_SOURCES = 190
SO_HOLES = 10
SO_TOP_CELLS = 40
SO_CAP_MIN = 1
SO_CAP_MAX = 1
SO_TIMENSEC_MIN = 100000000
SO_TIMENSEC_MAX = 300000000
SO_TIMEOUT = 1
SO_DURATION = 20

taxigame: master.o genera_mappa.o sources.o taxi.o
	gcc -o taxigame master.o genera_mappa.o -lrt
	
master.o: master.c taxi_game.h
	gcc $(CFLAGS) -c master.c

genera_mappa.o: genera_mappa.c taxi_game.h
	gcc $(CFLAGS) -c genera_mappa.c

sources.o: sources.c taxi_game.h
	gcc $(CFLAGS) sources.c -o sources
	
taxi.o: taxi.c taxi_game.h
	gcc $(CFLAGS) taxi.c -o taxi
	
clean:
	rm -f *.o sources taxi taxigame *~
	
run:
	./taxigame $(SO_TAXI) $(SO_SOURCES) $(SO_HOLES) $(SO_TOP_CELLS) $(SO_CAP_MIN) $(SO_CAP_MAX) $(SO_TIMENSEC_MIN) $(SO_TIMENSEC_MAX) $(SO_TIMEOUT) $(SO_DURATION)
