PREFIX   ?= /system/xbin

CXXFLAGS += -std=c++11 -Wall -Wextra -pedantic

elf-cleaner: 		elf-cleaner.cpp

clean:
			  rm -f elf-cleaner

install: 			elf-cleaner
			install elf-cleaner $(PREFIX)/elf-cleaner

uninstall:
	rm -f $(PREFIX)/elf-cleaner

