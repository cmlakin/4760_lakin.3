#Makefile/lakin/4760/Project2

GCC= gcc
CFLAGS= -g -Wall

all: runsim testsim

runsim: runsims.o license.o
	$(GCC) $(CFLAGS) runsims.o license.o -o runsim -lpthread

runsims.o: runsims.c
	$(GCC) $(CFLAGS) -c runsims.c

license.o: license.c
	$(GCC) $(CFLAGS) -c license.c

testsim: testsims.o
	$(GCC)  $(CFLAGS) testsims.o license.o -o testsim

testsims.o: testsims.c
	$(GCC) $(CFLAGS) -c testsims.c

clean:
	rm *.o runsim testsim
