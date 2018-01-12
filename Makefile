CC= cc
CFLAGS += -O0 -std=gnu99 -g
LDFLAGS += -lm
CPPFLAGS +=

EXES= scan

all: $(EXES)

clean:
	rm -f $(EXES)

scan: scan.c
	$(CC) -o $@ $+ $(CFLAGS) $(LDFLAGS) $(CPPFLAGS)
