CC     	= gcc
CFLAGS	= -static -O2 -g -Wall -pipe
TARGETS = control mixer switches pause pcm
LIB	= -L../lib -lasound

all: $(TARGETS)

control: control.c
	$(CC) $(CFLAGS) $(LIB) -o control control.c

mixer: mixer.c
	$(CC) $(CFLAGS) $(LIB) -o mixer mixer.c

switches: switches.c
	$(CC) $(CFLAGS) $(LIB) -o switches switches.c

pause: pause.c
	$(CC) $(CFLAGS) $(LIB) -o pause pause.c

pcm: pcm.c
	$(CC) $(CFLAGS) $(LIB) -o pcm pcm.c

clean:
	rm -f *.o $(TARGETS) *~
