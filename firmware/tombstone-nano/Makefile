# Host build. The Arduino sketch is compiled by the IDE or arduino-cli;
# this Makefile does not need a core. `make test` runs the rules and
# typechecks the Nano adapter against a tiny Arduino stub.

CC ?= gcc
CXX ?= g++
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -I.
CXXFLAGS ?= -std=c++11 -Wall -Wextra -Werror -pedantic -I.
BUILD := build

.PHONY: test clean

test: $(BUILD)/test_rules $(BUILD)/nano_board.o $(BUILD)/sketch.o
	./$(BUILD)/test_rules

$(BUILD)/test_rules: game.c pins.c test/test_game.c game.h hw.h lamps.h sound.h operator.h pins.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ game.c pins.c test/test_game.c

$(BUILD)/nano_board.o: nano_board.c nano_board.h game.h hw.h lamps.h sound.h pins.h pins.c
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -Itest/stub -c nano_board.c -o $@

$(BUILD)/sketch.o: tombstone-nano.ino game.h nano_board.h hw.h lamps.h sound.h operator.h
	mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -Itest/stub -x c++ -c tombstone-nano.ino -o $@

clean:
	rm -rf $(BUILD)
