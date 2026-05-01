#include "renderer.h"

#include <iostream>

#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image/stb_image_write.h"

#include "gui.h"

int main() {
    auto rd = std::make_unique<Renderer>();

    try {
        rd->init();
        GUI gui(&*rd);
        rd->mainLoop();
        rd->cleanup();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
