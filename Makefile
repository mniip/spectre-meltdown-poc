CC= cc
CFLAGS += -O0
LDFLAGS += -lm
CPPFLAGS +=

EXE= poc

all: $(EXE)

clean:
	rm -f $(EXE)

$(EXE): poc.c
	$(CC) -o $@ $+ $(CFLAGS) $(LDFLAGS) $(CPPFLAGS)

.PHONY: all clean
