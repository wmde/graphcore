CCFLAGS=-Wall -Wstrict-overflow=3 -Wno-unused-variable -std=c++0x -ggdb -DSYSTEMPAGESIZE=$(shell getconf PAGESIZE)
LDFLAGS=-lpthread -lreadline

ifeq ($(STDERR_DEBUGGING),1)
	CCFLAGS+=-DSTDERR_DEBUGGING
endif

ifeq ($(USE_MMAP_POOL),1)
	CCFLAGS+=-DUSE_MMAP_POOL
endif

ifeq ($(DEBUG_COMMANDS),1)
	CCFLAGS+=-DDEBUG_COMMANDS
endif

all: 		Release Debug

Release:	graphcore
Debug:		graphcore.dbg

graphcore:	src/main.cpp src/*.h
		g++ $(CCFLAGS) -O3 -march=native src/main.cpp $(LDFLAGS) -ographcore

graphcore.dbg:	src/main.cpp src/*.h
		g++ $(CCFLAGS) -O0 -DDEBUG_COMMANDS -ggdb src/main.cpp $(LDFLAGS) -ographcore.dbg

# updatelang: update the language files
# running this will generate changes in the repository
updatelang:	#
		./update-lang.sh

test:	Debug
		python test/talkback.py test/graphcore.tb ./graphcore.dbg
