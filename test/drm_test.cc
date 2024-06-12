#include <iostream>

#include "src/drm_wrapper.h"

int main() {
    DrmWrapper drm_wrapper;
    bool ret = drm_wrapper.open();
}