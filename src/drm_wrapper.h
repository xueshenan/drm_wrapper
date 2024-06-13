#pragma once

#include <stdint.h>
#include <xf86drmMode.h>

constexpr int32_t kBufferObjectSize = 4;

struct frame_buffer_object {
    uint32_t width;
    uint32_t height;
    uint32_t pitch[kBufferObjectSize];
    uint32_t handle[kBufferObjectSize];
    uint32_t size[kBufferObjectSize];
    uint8_t *vaddr[kBufferObjectSize];
    uint32_t fb_id;
};

class DrmWrapper {
public:
    /**
     * @brief open drm device
     * @param driver_name drm driver name
     */
    bool open(const char *driver_name = nullptr);
    /**
     * @brief draw nv 12 frame
     * @param width frame width
     * @param height frame height
     * @param stride line stride
    */
    bool draw_nv12_frame(uint8_t *address, int32_t width, int32_t height, int32_t stride);
    /**
     * @brief close drm device
    */
    void close();
public:
    DrmWrapper();
    ~DrmWrapper();
private:
    /**
     *  @brief log drm version info
     */
    void log_drm_version();
    /**
     * @brief get drm capability
    */
    bool get_drm_capability();
    /**
     * create nv12 frame buffer object
    */
    bool create_nv12_frame_buffer_object(int32_t width, int32_t height);
    /**
     * free nv12 frame buffer object
    */
    void free_frame_buffer_object();
private:
    int _fd;
    drmModeRes *_mode_res;
    drmModeCrtc *_mode_crtc;
    drmModePlaneRes *_mode_plane_res;
    drmModePlane *_mode_plane;
    uint32_t _conn_id;
    drmModeConnector *_conn;
    int _crtc_id;
    int _plane_id;
    uint32_t _pipe;
    bool _has_prime_import;
    bool _has_prime_export;
    bool _has_async_page_flip;
    bool _modesetting_enabled;

    uint16_t _hdisplay;
    uint16_t _vdisplay;

    uint32_t _buffer_id;

    uint32_t _mm_width;
    uint32_t _mm_height;

    bool _init_nv12_frame_buffer_object;
    frame_buffer_object _buffer_object;
};