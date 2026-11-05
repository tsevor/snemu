CC = gcc
CFLAGS = -Wall -Werror -Wextra -O3 -MMD -MP -flto -Iinclude
LDFLAGS =

HOST = sdl3
TARGET = snemu

ifeq ($(HOST), gtk4)
	HOST_SRC = src/hosts/gtk4.c
	HOST_CFLAGS = $(shell pkg-config --cflags gtk4 libadwaita-1)
	LDFLAGS += $(shell pkg-config --libs gtk4 libadwaita-1)
else ifeq ($(HOST), sdl3)
	HOST_SRC = src/hosts/sdl3.c
	HOST_CFLAGS += $(shell pkg-config --cflags sdl3)
	LDFLAGS += $(shell pkg-config --libs sdl3)
else ifeq ($(HOST), sdl3-win)
	HOST_SRC = src/hosts/sdl3.c
	CC = x86_64-w64-mingw32-gcc
	HOST_CFLAGS += $(shell x86_64-w64-mingw32-pkg-config --cflags sdl3)
	LDFLAGS += $(shell x86_64-w64-mingw32-pkg-config --static --libs sdl3) -static
	TARGET := $(TARGET).exe
else
	$(error unknown HOST '$(HOST)')
endif

SRCS = $(wildcard src/*.c src/mappers/*.c)
OBJS = $(patsubst src/%.c,bin/obj/%.o,$(SRCS))
HOST_OBJ = $(patsubst src/%.c,bin/obj/%.o,$(HOST_SRC))
DEPS = $(OBJS:.o=.d) $(HOST_OBJ:.o=.d)

.PHONY: all clean test

$(HOST_OBJ): CFLAGS += $(HOST_CFLAGS)

$(TARGET): $(OBJS) $(HOST_OBJ)
	@mkdir -p $(dir $@)
	@echo -e '\tCC\t'$@
	@$(CC) $(CFLAGS) $(OBJS) $(HOST_OBJ) -o $(TARGET) $(LDFLAGS)

bin/obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	@echo -e '\tCC\t'$@
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf bin

-include $(DEPS)
