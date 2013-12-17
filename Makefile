rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))

CC=ccache gcc
CXX=ccache g++
LD=g++
RM=rm -f

EXCLUDE=src/deps/glm/core/dummy.cpp
C_SOURCES=$(filter-out $(EXCLUDE),$(call rwildcard,src,*.c))
CXX_SOURCES=$(filter-out $(EXCLUDE),$(call rwildcard,src,*.cpp))



CFLAGS=-c -Isrc -Isrc/deps
CXXFLAGS=-c -Isrc -Isrc/deps -std=c++0x -DBOOST_THREAD_USE_LIB
LDFLAGS= -lrt -lpthread -ldl \
	-lboost_system \
	-lboost_chrono \
	-lboost_unit_test_framework \
	-lboost_test_exec_monitor \
	-lboost_thread \
	-lSDL2 \
	-lSDLmain \
	-lSDL2_mixer \
	-lassimp \
	-lGL \
	-lGLEW	



OBJECTS=$(C_SOURCES:.c=.o) $(CXX_SOURCES:.cpp=.o)
EXECUTABLE=bin/space

all: $(C_SOURCES) $(CXX_SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(LD) $(OBJECTS) -o $@ $(LDFLAGS)

.cpp.o:
	$(CXX) $(CXXFLAGS) -o $@ $<

.c.o:
	$(CC) $(CFLAGS) -o $@ $<

.PHONY: clean

clean:
	find -name '*.o' | xargs $(RM)
	$(RM) $(EXECUTABLE)
	$(RM) $(EXECUTABLE).exe
