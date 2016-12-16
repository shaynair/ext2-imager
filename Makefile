CC = gcc
CFLAGS = -Wall -Werror -Wextra -g
PROGS = ext2_ls ext2_cp ext2_mkdir ext2_ln ext2_rm ext2_rm_bonus

all : $(PROGS)
	rm -f *.o

$(PROGS) : % : %.o ext2_imager.o
	$(CC) $(CFLAGS) -o $@ $^

%.o : %.c ext2_imager.h ext2.h
	$(CC) $(CFLAGS) -o $@ -c $<
	
.PHONY: clean
clean : 
	rm -f $(PROGS) *.o *~
