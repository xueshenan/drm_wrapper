#pragma once

#include <stdint.h>

class DrmWrapper {
public:
    /**
     * @brief open drm device
     * @param driver_name drm driver name
     */
    bool open(const char *driver_name = nullptr);
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
private:
    int _fd;
    int _conn_id;
    int _crtc_id;
    int _plane_id;
    uint32_t _pipe;
    void *_saved_crtc;
    bool _has_prime_import;
    bool _has_prime_export;
    bool _has_async_page_flip;
    bool _modesetting_enabled;
    bool _restore_crtc;

    uint16_t _hdisplay;
    uint16_t _vdisplay;

    uint32_t _buffer_id;

    uint32_t _mm_width;
    uint32_t _mm_height;
};