CC = gcc

CFLAGS = -std=gnu11 -g \
         -Wall -Wextra -Wpedantic \
         -Wshadow -Wpointer-arith -Wcast-qual \
         -Wstrict-prototypes -Wmissing-prototypes \
         -Wformat=2 -Wundef -Wconversion \
         -fno-common

OBJ = sop-backup.o pomocnicze.o zarzadzanie.o proces_potomny.o kopia_operacje.o

sop-backup: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o sop-backup

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o sop-backup
