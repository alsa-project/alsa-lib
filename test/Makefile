CC     	= gcc
CFLAGS	= -static -O2 -g -Wall -pipe
TARGETS = control mixer
LIB	= -L../lib -lsound

all: $(TARGETS)

control: control.c
	$(CC) $(CFLAGS) $(LIB) -o control control.c

mixer: mixer.c
	$(CC) $(CFLAGS) $(LIB) -o mixer mixer.c

clean:
	rm -f *.o $(TARGETS) *~
