NAME                 = muxd

BUILD_DIR            = ./build
BUILD_PROG           = $(BUILD_DIR)/$(NAME)
BUILD_FILES          = $(BUILD_DIR)/main.o \
                       $(BUILD_DIR)/magic.o \
                       $(BUILD_DIR)/config.o

INIT_DIR           = ./init
INIT_FILES         = $(INIT_DIR)/muxd

CONFIG_DIR           = ./conf
CONFIG_FILES         = $(CONFIG_DIR)/muxd.conf \
                       $(CONFIG_DIR)/muxd.magic


INSTALL_DIR          = /usr/bin
INSTALL_PROG         = $(INSTALL_DIR)/$(NAME)

INSTALL_CONFIG_DIR   = /etc/muxd
INSTALL_CONFIG_FILES = $(INSTALL_CONFIG_DIR)/muxd.conf \
                       $(INSTALL_CONFIG_DIR)/muxd.magic

INSTALL_INIT_DIR     = /etc/init.d
INSTALL_INIT_FILES   = $(INSTALL_INIT_DIR)/muxd

CC                 = gcc
CFLAGS             = -std=c99 -lconfig -lmagic -Wall -Wextra -g3

SUCCESS_MSG        = '  [\e[32m DONE \e[0m]'


all: build

build: $(BUILD_PROG)

$(BUILD_PROG): $(BUILD_FILES)
	@echo 'Building muxd:'
	@$(CC) $(CFLAGS) $^ -o $@
	@echo -e $(SUCCESS_MSG)

./build/main.o: ./src/main.c
	@mkdir -p build/
	@$(CC) $(CFLAGS) -c $< -o $@

./build/config.o: ./src/config.c ./src/config.h
	@mkdir -p build/
	@$(CC) $(CFLAGS) -c $< -o $@

./build/magic.o: ./src/magic.c ./src/magic.h
	@mkdir -p build/
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo 'Cleaning workspace:'
	@rm -rf build/
	@echo -e $(SUCCESS_MSG)

rebuild: clean build

install: build $(CONFIG_FILES)
	@echo 'Installing muxd:'
	@install -m 755 $(BUILD_PROG) $(INSTALL_PROG)
	@mkdir -p $(INSTALL_CONFIG_DIR)
	@install -m 644 $(CONFIG_FILES) $(INSTALL_CONFIG_DIR)
	@install -m 755 $(INIT_FILES) $(INSTALL_INIT_DIR)
	@echo -e $(SUCCESS_MSG)

uninstall:
	@echo 'Uninstalling muxd:'
	@rm -f $(INSTALL_PROG)
	@rm -f $(INSTALL_INIT_FILES)
	@rm -rf $(INSTALL_CONFIG_DIR)
	@echo -e $(SUCCESS_MSG)

