CC = g++
CXXFLAGS = -std=c++20 -I/usr/include -I/usr/local/include/glm -Iinclude -Iinclude/tinygltf -Iinclude/stb_image -I$(VULKAN_SDK)/include -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends
LDFLAGS = -L/usr/lib -L$(VULKAN_SDK)/lib -lvulkan -lglfw

DEBUG_FLAGS = -g -O0 -DDEBUG 
RELEASE_FLAGS = -O3 -Wall -Wshadow -DNDEBUG

IMGUI_DIR = external/imgui

IMGUI_SRC = $(wildcard $(IMGUI_DIR)/*.cpp) \
			$(IMGUI_DIR)/backends/imgui_impl_vulkan.cpp \
			$(IMGUI_DIR)/backends/imgui_impl_glfw.cpp


#SOURCES = main.cpp src/camera.cpp src/blas.cpp src/tlas.cpp src/renderer.cpp src/globals.cpp src/arena.cpp
SOURCES = main.cpp $(wildcard src/*.cpp)
SOURCES += $(IMGUI_SRC)
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
