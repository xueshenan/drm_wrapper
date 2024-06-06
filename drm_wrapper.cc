#include "drm_wrapper.h"

#include <drm.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <string>

#include "base/log.h"

bool DrmWrapper::open(const char *driver_name /*= nullptr*/) {
    std::string str_driver_name = "msm_drm";
    if (driver_name != nullptr) {
        str_driver_name = driver_name;
    }
    _fd = drmOpen(str_driver_name.c_str(), NULL);
    // fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (_fd < 0) {
        base::LogError() << "open " << str_driver_name << "failed:" << errno;
        return false;
    }
    log_drm_version();

    if (!get_drm_capability()) {
        return false;
    }
    return true;
}

void DrmWrapper::close() {
    ::close(_fd);
}

DrmWrapper::DrmWrapper() {}

DrmWrapper::~DrmWrapper() {}

void DrmWrapper::log_drm_version() {
    drmVersion *version = NULL;

    version = drmGetVersion(_fd);
    if (version != NULL) {
        base::LogInfo() << "DRM v" << version->version_major << "." << version->version_minor << "."
                        << version->version_patchlevel << " [" << version->name << " - "
                        << version->desc << " - " << version->date << "]";
        drmFreeVersion(version);
    } else {
        base::LogError() << "could not get driver information";
    }
}

bool DrmWrapper::get_drm_capability() {
    uint64_t has_dumb_buffer = 0;
    int ret = drmGetCap(_fd, DRM_CAP_DUMB_BUFFER, &has_dumb_buffer);
    if (ret != 0) {
        base::LogWarn() << "could not get dumb buffer capability";
    }
    if (has_dumb_buffer == 0) {
        base::LogError() << "driver cannot handle dumb buffers";
        return false;
    }

    uint64_t has_prime = 0;
    ret = drmGetCap(_fd, DRM_CAP_PRIME, &has_prime);
    if (ret != 0) {
        base::LogWarn() << "could not get prime capability";
    } else {
        _has_prime_import = (has_prime & DRM_PRIME_CAP_IMPORT);
        _has_prime_export = (has_prime & DRM_PRIME_CAP_EXPORT);
    }

    uint64_t has_async_page_flip = 0;
    ret = drmGetCap(_fd, DRM_CAP_ASYNC_PAGE_FLIP, &has_async_page_flip);
    if (ret != 0) {
        base::LogWarn() << "could not get async page flip capability";
    } else {
        _has_async_page_flip = has_async_page_flip;
    }

    // clang-format off
    base::LogDebug() << "prime import ("  << (_has_prime_import ? "✓" : "✗")
                     << ") / prime export (" << (_has_prime_export ? "✓": "✗")
                     << ") / async page flip (" << (_has_async_page_flip ? "✓" : "✗")
                     << ")";
    // clang-format on
    return true;
}
