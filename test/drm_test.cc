#include <iostream>

#include "src/drm_wrapper.h"

int main() {
    DrmWrapper drm_wrapper;
    bool ret = drm_wrapper.open();
    if (!ret) {
        return 1;
    }

    int width = 1920;
    int height = 1080;
    FILE *fp = fopen("sample.yuv", "rb");
    if (fp == NULL) {
        printf("cannot open sample yuv\n");
        return 1;
    }
    int buffer_size = width * height * 3 / 2;
    uint8_t *mem_buffer = (uint8_t *)malloc(buffer_size);
    int read_size = fread(mem_buffer, buffer_size, 1, fp);
    fclose(fp);

    drm_wrapper.draw_nv12_frame(mem_buffer, width, height, width);
    getchar();

    drm_wrapper.close();
    return 0;
}