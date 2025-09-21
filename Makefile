PACKAGE = adan
CC = gcc

CFLAGS = -Wall -Wextra -Werror -O0 -ggdb -std=gnu23
LDFLAGS = -lcurl -ljansson

$(PACKAGE): $(PACKAGE).c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(PACKAGE) $<

clean:
	rm -rdvf $(PACKAGE)

.PHONY: clean
