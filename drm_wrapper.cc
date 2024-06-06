#include "drm_wrapper.h"

#include <drm.h>
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
    return true;
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
