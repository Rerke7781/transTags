CC ?= gcc
PKG_CONFIG ?= pkg-config
TARGET := transTags_linux
SRC := transTags_linux.c

CFLAGS += -Wall -Wextra -O2 -std=c11 $(shell $(PKG_CONFIG) --cflags x11 xfixes xext)
LDLIBS += $(shell $(PKG_CONFIG) --libs x11 xfixes xext)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
