CC = g++
CXXFLAGS = -std=c++20 -I/usr/include -I/usr/local/include/glm -Iinclude -I$(VULKAN_SDK)/include
LDFLAGS = -L/usr/lib -L$(VULKAN_SDK)/lib -lvulkan -lglfw

DEBUG_FLAGS = -g -O0 -DDEBUG 
RELEASE_FLAGS = -O3 -Wall -Wshadow -DNDEBUG

SOURCES = main.cpp src/camera.cpp src/blas.cpp src/tlas.cpp src/renderer.cpp src/globals.cpp src/arena.cpp
OBJECTS = $(SOURCES:.cpp=.o)

EXECUTABLE = final

# Default to release mode if MODE is not passed
CXXFLAGS += $(if $(filter debug,$(MODE)),$(DEBUG_FLAGS),$(RELEASE_FLAGS))

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CXXFLAGS) $(OBJECTS) -o $(EXECUTABLE) $(LDFLAGS)
	chmod +x $(EXECUTABLE)

%.o: %.cpp
	$(CC) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJECTS) $(EXECUTABLE)

.PHONY: clean all debug release

debug:
	$(MAKE) MODE=debug clean all

release:
	$(MAKE) MODE=release clean all
