# Make_file for StackAndOverlayHistograms

# Compiler
CC = g++
CFLAGS = -I. $(shell root-config --cflags)
LDFLAGS = $(shell root-config --libs)

SOURCES = StackAndOverlayHistograms.cpp
OBJECTS = $(SOURCES:.cpp=.o)
EXECUTABLE = StackAndOverlayHistograms

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)
