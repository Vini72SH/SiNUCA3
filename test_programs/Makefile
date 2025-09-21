ifndef CC
  CC = clang
  ifeq "$(shell which clang)" ""
    CC = gcc
    ifeq "$(shell which gcc)" ""
      CC = cc
      ifeq "$(shell which cc)" ""
        $(error "Could not detect C compiler. Set one with CC or install clang, gcc or cc.")
      endif
    endif
  endif
endif

CFLAGS = -Wall -Wextra -std=c99 -O3 -I../x86_trace_generator/ -g
LOPENMP = -fopenmp
SRCS = $(shell find -name "*.c")
EXEC = $(SRCS:.c=)

all: $(EXEC)

%: %.c
	$(CC) $(CFLAGS) -o $@ $^ $(LOPENMP)

clean:
	rm -f $(EXEC)
