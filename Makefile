#If you use threads, add -pthread here.
COMPILERFLAGS = -g -Wall -Wextra -Wno-sign-compare

#Any libraries you might need linked in.
#LINKLIBS = -lm

SENDEROBJECTS = obj/sender_main.o
RECEIVEROBJECTS = obj/receiver_main.o

.PHONY: all clean

all : obj reliable_sender reliable_receiver

reliable_sender: $(SENDEROBJECTS)
	$(CC) $(COMPILERFLAGS) $^ -o $@ $(LINKLIBS)

reliable_receiver: $(RECEIVEROBJECTS)
	$(CC) $(COMPILERFLAGS) $^ -o $@ $(LINKLIBS)

clean :
	$(RM) -r obj/ reliable_sender reliable_receiver output
obj/%.o: src/%.c
	$(CC) $(COMPILERFLAGS) -c -o $@ $<
obj:
	mkdir -p obj
