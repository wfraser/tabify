CFLAGS = -std=c1x -pedantic
CC     = gcc

OBJS   = tabify.o growbuf.o

all: tabify

.SUFFIXES:

%.o: %.c
	@echo "    CC  $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

tabify: $(OBJS)
	@echo "  LINK  $<"
	@$(CC) $(LDFLAGS) -o $@ $(OBJS)

clean:
	@echo " CLEAN"
	@rm -f *.o tabify

