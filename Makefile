OUTFILES           = build/mmux.o build/config.o
TARGET             = build/mmux
INSTALL            = /usr/bin/mmux

SUCCESS_MSG        = '  [\e[32m DONE \e[0m]'

CC                 = gcc
CFLAGS             = -std=c99 -lmagic -Wall -Wextra -g3


all: build

build: $(TARGET)

$(TARGET): $(OUTFILES)
	@echo 'Building target:'
	@$(CC) $(CFLAGS) $^ -o $@
	@echo -e $(SUCCESS_MSG)

build/mmux.o: src/mmux.c
	@mkdir -p build/
	@$(CC) $(CFLAGS) -c $< -o $@

build/config.o: src/config.c src/config.h
	@mkdir -p build/
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo 'Cleaning workspace:'
	@rm -rf build/
	@echo -e $(SUCCESS_MSG)

rebuild: clean build

install: build
	@echo 'Installing target:'
	@cp -f $(TARGET) $(INSTALL)
	@echo -e $(SUCCESS_MSG)

uninstall:
	@echo 'Uninstalling target:'
	@rm -f $(INSTALL)
	@echo -e $(SUCCESS_MSG)

