CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic -O3 -g -fopenmp
LDFLAGS = $(CFLAGS)
LDLIBS  = -lm

all: partdiff

partdiff: partdiff.o

clean:
	$(RM) partdiff.o partdiff
