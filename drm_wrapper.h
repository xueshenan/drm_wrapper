#pragma once

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
    bool _has_prime_import;
    bool _has_prime_export;
    bool _has_async_page_flip;
};