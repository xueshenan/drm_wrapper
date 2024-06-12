#include "drm_utils.h"

#include <drm_fourcc.h>

uint32_t drm_bpp_from_drm_format(uint32_t drm_format) {
    uint32_t bpp = 0;
    switch (drm_format) {
        case DRM_FORMAT_YUV420:
        case DRM_FORMAT_YVU420:
        case DRM_FORMAT_YUV422:
        case DRM_FORMAT_NV12:
        case DRM_FORMAT_NV21:
        case DRM_FORMAT_NV16:
        case DRM_FORMAT_NV61:
        case DRM_FORMAT_NV24:
            bpp = 8;
            break;
        case DRM_FORMAT_P010:
            bpp = 10;
            break;
        case DRM_FORMAT_UYVY:
        case DRM_FORMAT_YUYV:
        case DRM_FORMAT_YVYU:
        case DRM_FORMAT_P016:
        case DRM_FORMAT_RGB565:
        case DRM_FORMAT_BGR565:
            bpp = 16;
            break;
        case DRM_FORMAT_BGR888:
        case DRM_FORMAT_RGB888:
            bpp = 24;
            break;
        case DRM_FORMAT_ARGB8888:
        case DRM_FORMAT_ABGR8888:
            bpp = 32;
            break;
        default:
            bpp = 32;
    }
    return bpp;
}

uint32_t drm_height_from_drm_format(uint32_t drm_format, uint32_t height) {
    uint32_t ret = 0;
    switch (drm_format) {
        case DRM_FORMAT_YUV420:
        case DRM_FORMAT_YVU420:
        case DRM_FORMAT_YUV422:
        case DRM_FORMAT_NV12: /* 2x2 subsampled Cr:Cb plane */
        case DRM_FORMAT_NV21:
        case DRM_FORMAT_P010:
        case DRM_FORMAT_P016:
            ret = height * 3 / 2;
            break;
        case DRM_FORMAT_NV16: /* 2x1 subsampled Cr:Cb plane */
        case DRM_FORMAT_NV61:
            ret = height * 2;
            break;
        case DRM_FORMAT_NV24: /* non-subsampled Cr:Cb plane */
            ret = height * 3;
            break;
        default:
            ret = height;
            break;
    }

    return ret;
}
