#If you use threads, add -pthread here.
COMPILERFLAGS = -g -Wall -Wextra -Wno-sign-compare

#Any libraries you might need linked in.
#LINKLIBS = -lm

SENDEROBJECTS = obj/sender_main.o
RECEIVEROBJECTS = obj/receiver_main.o

.PHONY: all clean

all : obj sender_main receiver_main

sender_main: $(SENDEROBJECTS)
	$(CC) $(COMPILERFLAGS) $^ -o $@ $(LINKLIBS)

receiver_main: $(RECEIVEROBJECTS)
	$(CC) $(COMPILERFLAGS) $^ -o $@ $(LINKLIBS)

clean :
	$(RM) -r obj/ sender_main receiver_main output
obj/%.o: src/%.c
	$(CC) $(COMPILERFLAGS) -c -o $@ $<
obj:
	mkdir -p obj
