#pragma once

#include <stdint.h>

/**
 * @brief calc drm bpp from drm pixformat
*/
uint32_t drm_bpp_from_drm_format(uint32_t drm_format);

uint32_t drm_height_from_drm_format(uint32_t drm_format, uint32_t height);