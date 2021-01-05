all: websnarf

websnarf: websnarf.o
	gcc -o websnarf -lnsl -Wall websnarf.o
websnarf.o: websnarf.c
	gcc -c -Wall websnarf.c
