#define STB_IMAGE_IMPLEMENTATION
#include "external/stb_image.h"

// -----------------------------------------------------------------------------
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "external/stb_image_write.h"

// -----------------------------------------------------------------------------
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "external/stb_image_resize2.h"

// -----------------------------------------------------------------------------

void stbi_write_to_FILE_func(void* file, void* data, int size)
{
    fwrite(data, size, 1, (FILE*)file);
}
