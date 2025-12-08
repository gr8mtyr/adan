PACKAGE = adan

RM = rm -rdvf

CC = gcc
## Debug
# CFLAGS = -Wall -Wextra -Werror -Wpedantic -O0 -ggdb -std=gnu11 -fsanitize=address,undefined
# Valgrind
# CFLAGS = -Wall -Wextra -Werror -Wpedantic -O0 -ggdb -std=gnu11
## Release
CFLAGS = -Os -s
CPPFLAGS = -lcurl -ljansson

all: $(PACKAGE)

$(PACKAGE): main.c
	$(CC) $(CFLAGS) -o $(PACKAGE) main.c $(CPPFLAGS)

clean:
	$(RM) $(PACKAGE)

.PHONY: all clean
