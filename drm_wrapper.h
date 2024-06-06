#pragma once

class DrmWrapper {
public:
    /**
     * @brief open drm device
     * @param driver_name drm driver name
     */
    bool open(const char *driver_name = nullptr);
public:
    DrmWrapper();
    ~DrmWrapper();
private:
    /**
     *  @brief log drm version info
     */
    void log_drm_version();
private:
    int _fd;
};