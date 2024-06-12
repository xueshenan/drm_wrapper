#include "drm_wrapper.h"

#include <drm.h>
#include <string.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <string>

#include "base/log.h"

static drmModeConnector *find_main_monitor(int fd, drmModeRes *res);
static drmModeConnector *find_used_connector_by_type(int fd, drmModeRes *res, int type);
static drmModeConnector *find_first_used_connector(int fd, drmModeRes *res);
static drmModeCrtc *find_crtc_for_connector(int fd, drmModeRes *res, drmModeConnector *conn,
                                            uint32_t *pipe);
static drmModePlane *find_plane_for_crtc(int fd, drmModeRes *res, drmModePlaneRes *pres,
                                         int crtc_id);

bool DrmWrapper::open(const char *driver_name /*= nullptr*/) {
    bool ret = true;
    std::string str_driver_name = "msm_drm";

    drmModeRes *res = NULL;
    drmModeConnector *conn = NULL;
    drmModeCrtc *crtc = NULL;
    drmModePlaneRes *plane_res = NULL;
    drmModePlane *plane = NULL;

    bool universal_planes = false;

    if (driver_name != nullptr) {
        str_driver_name = driver_name;
    }
    _fd = drmOpen(str_driver_name.c_str(), NULL);
    // fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (_fd < 0) {
        base::LogError() << "Could not open DRM module " << str_driver_name << "reason: ",
            strerror(errno);
        ret = false;
        goto bail;
    }

    log_drm_version();
    if (!get_drm_capability()) {
        ret = false;
        goto bail;
    }

    res = drmModeGetResources(_fd);
    if (res == NULL) {
        base::LogError() << "drmModeGetResources failed:" << strerror(errno);
        ret = false;
        goto bail;
    }

    if (_conn_id == -1) {
        conn = find_main_monitor(_fd, res);
    } else {
        conn = drmModeGetConnector(_fd, _conn_id);
    }
    if (conn == NULL) {
        ret = false;
        base::LogError() << "Could not find a valid monitor connector";
        goto bail;
    }

    crtc = find_crtc_for_connector(_fd, res, conn, &_pipe);
    if (crtc == NULL) {
        ret = false;
        base::LogError() << "Could not find a crtc for connector";
        goto bail;
    }

    if (!crtc->mode_valid || _modesetting_enabled) {
        base::LogDebug() << "enabling modesetting";
        _modesetting_enabled = true;
        universal_planes = true;
    }

    if (crtc->mode_valid && _modesetting_enabled && _restore_crtc) {
        _saved_crtc = (drmModeCrtc *)crtc;
    }

retry_find_plane:
    if (universal_planes && drmSetClientCap(_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1)) {
        base::LogError() << "Could not set universal planes capability bit";
        ret = false;
        goto bail;
    }

    plane_res = drmModeGetPlaneResources(_fd);
    if (plane_res == NULL) {
        //TODO(anxs) need or not need set ret?
        ret = false;
        base::LogError() << "drmModeGetPlaneResources failed reason:" << strerror(errno);
        goto bail;
    }

    if (_plane_id == -1) {
        plane = find_plane_for_crtc(_fd, res, plane_res, crtc->crtc_id);
    } else {
        plane = drmModeGetPlane(_fd, _plane_id);
    }
    if (plane == NULL) {
        ret = false;
        if (universal_planes) {
            base::LogError() << "Could not find a plane for crtc";
            goto bail;
        } else {
            universal_planes = true;
            goto retry_find_plane;
        }
    }

    _conn_id = conn->connector_id;
    _crtc_id = crtc->crtc_id;
    _plane_id = plane->plane_id;

    base::LogDebug() << "connector id = " << _conn_id << " / crtc id = " << _crtc_id
                     << " / plane id = " << _plane_id;

    _hdisplay = crtc->mode.hdisplay;
    _vdisplay = crtc->mode.vdisplay;

    _buffer_id = crtc->buffer_id;

    _mm_width = conn->mmWidth;
    _mm_height = conn->mmHeight;

    base::LogDebug() << "display size: pixels = " << _hdisplay << "x" << _vdisplay
                     << " / millimeters = " << _mm_width << "x" << _mm_height;
    ret = true;
bail:
    if (plane != NULL) {
        drmModeFreePlane(plane);
    }
    if (plane_res != NULL) {
        drmModeFreePlaneResources(plane_res);
    }
    if (crtc != _saved_crtc) {
        drmModeFreeCrtc(crtc);
    }
    if (conn != NULL) {
        drmModeFreeConnector(conn);
    }
    if (res != NULL) {
        drmModeFreeResources(res);
    }

    if (!ret && _fd >= 0) {
        drmClose(_fd);
        _fd = -1;
    }

    return ret;
}

void DrmWrapper::close() {
    ::close(_fd);
}

DrmWrapper::DrmWrapper() {
    _conn_id = -1;
    _plane_id = 49;  //TODO(anxs) temp value
    _restore_crtc = false;
}

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

static drmModeConnector *find_main_monitor(int fd, drmModeRes *res) {
    /* Find the LVDS and eDP connectors: those are the main screens. */
    constexpr int priority_count = 2;
    static const int priority[priority_count] = {DRM_MODE_CONNECTOR_LVDS, DRM_MODE_CONNECTOR_eDP};

    drmModeConnector *connector = NULL;
    for (int i = 0; connector == NULL && i < priority_count; i++) {
        connector = find_used_connector_by_type(fd, res, priority[i]);
    }

    /* if we didn't find a connector, grab the first one in use */
    if (connector == NULL) {
        connector = find_first_used_connector(fd, res);
    }

    /* if no connector is used, grab the first one */
    if (connector != NULL) {
        connector = drmModeGetConnector(fd, res->connectors[0]);
    }

    return connector;
}

static drmModeCrtc *find_crtc_for_connector(int fd, drmModeRes *res, drmModeConnector *conn,
                                            uint32_t *pipe) {
    int crtc_id = -1;
    drmModeEncoder *encoder = NULL;
    drmModeCrtc *crtc = NULL;
    uint32_t crtcs_for_connector = 0;

    for (int i = 0; i < res->count_encoders; i++) {
        encoder = drmModeGetEncoder(fd, res->encoders[i]);
        if (encoder != NULL) {
            if (encoder->encoder_id == conn->encoder_id) {
                crtc_id = encoder->crtc_id;
                drmModeFreeEncoder(encoder);
                break;
            }
            drmModeFreeEncoder(encoder);
        }
    }

    /* If no active crtc was found, pick the first possible crtc */
    if (crtc_id == -1) {
        for (int i = 0; i < conn->count_encoders; i++) {
            encoder = drmModeGetEncoder(fd, conn->encoders[i]);
            crtcs_for_connector |= encoder->possible_crtcs;
            drmModeFreeEncoder(encoder);
        }

        if (crtcs_for_connector != 0) crtc_id = res->crtcs[ffs(crtcs_for_connector) - 1];
    }

    if (crtc_id == -1) {
        return NULL;
    }

    for (int i = 0; i < res->count_crtcs; i++) {
        crtc = drmModeGetCrtc(fd, res->crtcs[i]);
        if (crtc != NULL) {
            if (crtc_id == crtc->crtc_id) {
                if (pipe != NULL) {
                    *pipe = i;
                }
                return crtc;
            }
            drmModeFreeCrtc(crtc);
        }
    }

    return NULL;
}

static bool connector_is_used(int fd, drmModeRes *res, drmModeConnector *conn) {
    bool result = false;
    drmModeCrtc *crtc = find_crtc_for_connector(fd, res, conn, NULL);
    if (crtc != NULL) {
        result = crtc->buffer_id != 0;
        drmModeFreeCrtc(crtc);
    }

    return result;
}

static drmModeConnector *find_first_used_connector(int fd, drmModeRes *res) {
    drmModeConnector *connector = NULL;
    for (int i = 0; i < res->count_connectors; i++) {
        connector = drmModeGetConnector(fd, res->connectors[i]);
        if (connector != NULL) {
            if (connector_is_used(fd, res, connector)) {
                return connector;
            }
            drmModeFreeConnector(connector);
        }
    }

    return NULL;
}

static drmModeConnector *find_used_connector_by_type(int fd, drmModeRes *res, int type) {
    drmModeConnector *connector = NULL;
    for (int i = 0; i < res->count_connectors; i++) {
        connector = drmModeGetConnector(fd, res->connectors[i]);
        if (connector != NULL) {
            if ((connector->connector_type == type) && connector_is_used(fd, res, connector)) {
                return connector;
            }
            drmModeFreeConnector(connector);
        }
    }

    return NULL;
}

static drmModePlane *find_plane_for_crtc(int fd, drmModeRes *res, drmModePlaneRes *pres,
                                         int crtc_id) {
    drmModePlane *plane = NULL;
    int pipe = -1;

    for (int i = 0; i < res->count_crtcs; i++) {
        if (crtc_id == res->crtcs[i]) {
            pipe = i;
            break;
        }
    }

    if (pipe == -1) {
        return NULL;
    }

    for (int i = 0; i < pres->count_planes; i++) {
        plane = drmModeGetPlane(fd, pres->planes[i]);
        if (plane->possible_crtcs & (1 << pipe)) {
            return plane;
        }
        drmModeFreePlane(plane);
    }

    return NULL;
}