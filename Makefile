CC=gcc
CFLAGS=-Wall -Wextra -Iinclude

SRC = src/*.c

grapeusb:
	$(CC) $(CFLAGS) $(SRC) -o grapeusb

clean:
	rm -f grapeusb