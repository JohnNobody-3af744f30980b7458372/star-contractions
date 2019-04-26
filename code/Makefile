BUILD=debug

ADDITIONAL_CFLAGS=

CC=g++
cflags.common=-std=c++14 -O3 -Wall -Wextra -Wno-unused-result -Wfatal-errors
cflags.debug=-march=native -ggdb
cflags.release=-march=native -DNDEBUG

CFLAGS=${cflags.common} ${cflags.${BUILD}} ${ADDITIONAL_CFLAGS}
LDFLAGS=

MAIN=star_contractions_test
 

HEADERS=$(wildcard src/*.hpp)


# clear implicit rules
.SUFFIXES: 

default:  bin/star_contractions_test


bin/%: src/%.cpp ./version.sh $(HEADERS) | bin
	$(CC) $(CFLAGS) $(LDFLAGS) $(shell ./version.sh) -o $@ $<

bin:
	mkdir -p bin

.PHONY: clean
clean:
	rm -rf bin/

data:
	@if [ ! -d directory ]; then\
		wget http://www.lamsade.dauphine.fr/~sikora/pace18/heuristic.zip ;\
		unzip heuristic.zip ;\
		rm heuristic.zip ;\
		mv public/ data/ ;\
	fi

