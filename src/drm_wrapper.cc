#include "drm_wrapper.h"

#include <drm.h>
#include <drm_fourcc.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>

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

    _mode_res = drmModeGetResources(_fd);
    if (_mode_res == NULL) {
        base::LogError() << "drmModeGetResources failed:" << strerror(errno);
        ret = false;
        goto bail;
    }

    if (_conn_id == -1) {
        _conn = find_main_monitor(_fd, _mode_res);
    } else {
        _conn = drmModeGetConnector(_fd, _conn_id);
    }
    if (_conn == NULL) {
        ret = false;
        base::LogError() << "Could not find a valid monitor connector";
        goto bail;
    }

    _mode_crtc = find_crtc_for_connector(_fd, _mode_res, _conn, &_pipe);
    if (_mode_crtc == NULL) {
        ret = false;
        base::LogError() << "Could not find a crtc for connector";
        goto bail;
    }

    if (!_mode_crtc->mode_valid || _modesetting_enabled) {
        base::LogDebug() << "enabling modesetting";
        _modesetting_enabled = true;
        universal_planes = true;
    }

retry_find_plane:
    if (universal_planes && drmSetClientCap(_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1)) {
        base::LogError() << "Could not set universal planes capability bit";
        ret = false;
        goto bail;
    }

    _mode_plane_res = drmModeGetPlaneResources(_fd);
    if (_mode_plane_res == NULL) {
        //TODO(anxs) need or not need set ret?
        ret = false;
        base::LogError() << "drmModeGetPlaneResources failed reason:" << strerror(errno);
        goto bail;
    }

    if (_plane_id == -1) {
        _mode_plane = find_plane_for_crtc(_fd, _mode_res, _mode_plane_res, _mode_crtc->crtc_id);
    } else {
        _mode_plane = drmModeGetPlane(_fd, _plane_id);
    }
    if (_mode_plane == NULL) {
        ret = false;
        if (universal_planes) {
            base::LogError() << "Could not find a plane for crtc";
            goto bail;
        } else {
            universal_planes = true;
            goto retry_find_plane;
        }
    }

    _conn_id = _conn->connector_id;
    _crtc_id = _mode_crtc->crtc_id;
    _plane_id = _mode_plane->plane_id;

    base::LogDebug() << "connector id = " << _conn_id << " / crtc id = " << _crtc_id
                     << " / plane id = " << _plane_id;

    _hdisplay = _mode_crtc->mode.hdisplay;
    _vdisplay = _mode_crtc->mode.vdisplay;

    _buffer_id = _mode_crtc->buffer_id;

    _mm_width = _conn->mmWidth;
    _mm_height = _conn->mmHeight;

    base::LogDebug() << "display size: pixels = " << _hdisplay << "x" << _vdisplay
                     << " / millimeters = " << _mm_width << "x" << _mm_height;
    ret = true;
    return ret;
bail:
    if (_mode_plane != NULL) {
        drmModeFreePlane(_mode_plane);
    }
    if (_mode_plane_res != NULL) {
        drmModeFreePlaneResources(_mode_plane_res);
    }
    if (_mode_crtc != NULL) {
        drmModeFreeCrtc(_mode_crtc);
    }
    if (_conn != NULL) {
        drmModeFreeConnector(_conn);
    }
    if (_mode_res != NULL) {
        drmModeFreeResources(_mode_res);
    }

    if (!ret && _fd >= 0) {
        drmClose(_fd);
        _fd = -1;
    }

    return ret;
}

bool DrmWrapper::draw_nv12_frame(uint8_t *address, int32_t width, int32_t height, int32_t stride) {
    if (!_init_nv12_frame_buffer_object) {
        bool ret = create_nv12_frame_buffer_object(width, height);
        if (!ret) {
            return false;
        }
    }

    if (width == stride) {
        memcpy(_buffer_object.vaddr[0], address, _buffer_object.width * _buffer_object.height);
        memcpy(_buffer_object.vaddr[1], address + _buffer_object.width * _buffer_object.height,
               _buffer_object.width * _buffer_object.height / 2);
    } else {
        // copy Y buffer
        uint8_t *vaddr = _buffer_object.vaddr[0];
        for (uint32_t i = 0; i < _buffer_object.height; i++) {
            memcpy(vaddr, address + i * stride, _buffer_object.width);
            vaddr += _buffer_object.pitch[0];
        }
        // copy uv buffer
        vaddr = _buffer_object.vaddr[1];
        address = address + _buffer_object.width * _buffer_object.height;
        for (uint32_t i = 0; i < _buffer_object.height / 2; i++) {
            memcpy(vaddr, address + i * stride, _buffer_object.width);
            vaddr += _buffer_object.pitch[1];
        }
    }

    drmModeSetCrtc(_fd, _crtc_id, _buffer_object.fb_id, 0, 0, &_conn_id, 1, &_conn->modes[0]);

    return true;
}

void DrmWrapper::close() {
    if (_fd < 0) {
        return;
    }
    if (_mode_plane != NULL) {
        drmModeFreePlane(_mode_plane);
    }
    if (_mode_plane_res != NULL) {
        drmModeFreePlaneResources(_mode_plane_res);
    }
    if (_mode_crtc != NULL) {
        drmModeFreeCrtc(_mode_crtc);
    }
    if (_conn != NULL) {
        drmModeFreeConnector(_conn);
    }
    if (_mode_res != NULL) {
        drmModeFreeResources(_mode_res);
    }
    ::close(_fd);
    _fd = -1;
}

DrmWrapper::DrmWrapper() {
    _fd = -1;
    _mode_res = NULL;
    _conn_id = -1;
    _conn = NULL;
    _mode_crtc = NULL;
    _mode_plane_res = NULL;
    _mode_plane = NULL;

    _plane_id = -1;

    _init_nv12_frame_buffer_object = false;
}

DrmWrapper::~DrmWrapper() {
    close();
}

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

bool DrmWrapper::create_nv12_frame_buffer_object(int32_t width, int32_t height) {
    uint32_t pixel_format = DRM_FORMAT_NV12;

    _buffer_object.width = width;
    _buffer_object.height = height;

    struct drm_mode_create_dumb create = {};
    ///< Y buffer
    create.width = width;
    create.height = height;
    create.bpp = 8;
    /* handle, pitch, size will be returned */
    int ret = drmIoctl(_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);
    if (ret != 0) {
        base::LogError() << "drmIoctl DRM_IOCTL_MODE_CREATE_DUMB create Y dumb failed " << ret;
        return false;
    }

    _buffer_object.pitch[0] = create.pitch;
    _buffer_object.size[0] = create.size;
    _buffer_object.handle[0] = create.handle;
    base::LogDebug() << "drm ioctl create Y dump pitch:" << create.pitch << ", size:" << create.size
                     << ",handle:" << create.handle;

    ///< UV buffer
    create.width = width;
    create.height = height;
    create.bpp = 8;

    ret = drmIoctl(_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);
    if (ret != 0) {
        base::LogError() << "drmIoctl DRM_IOCTL_MODE_CREATE_DUMB create UV dumb failed " << ret;
        return false;
    }

    _buffer_object.pitch[1] = create.pitch;
    _buffer_object.size[1] = create.size;
    _buffer_object.handle[1] = create.handle;

    uint32_t bo_handles[4] = {
        0,
    };
    uint32_t pitches[4] = {
        0,
    };
    uint32_t offsets[4] = {
        0,
    };

    bo_handles[0] = _buffer_object.handle[0];
    bo_handles[1] = _buffer_object.handle[1];
    pitches[0] = _buffer_object.pitch[0];
    pitches[1] = _buffer_object.pitch[1];
    offsets[0] = 0;
    offsets[1] = 0;

    ret = drmModeAddFB2(_fd, create.width, create.height, pixel_format, bo_handles, pitches,
                        offsets, &_buffer_object.fb_id, 0);

    if (ret) {
        base::LogError() << "drmModeAddFB2 failed " << ret;
        return false;
    }
    base::LogDebug() << "success add fb, fb_id:" << _buffer_object.fb_id;

    struct drm_mode_map_dumb map = {};
    map.handle = _buffer_object.handle[0];
    drmIoctl(_fd, DRM_IOCTL_MODE_MAP_DUMB, &map);

    ///< Y buffer
    _buffer_object.vaddr[0] = (uint8_t *)mmap(0, _buffer_object.size[0], PROT_READ | PROT_WRITE,
                                              MAP_SHARED, _fd, map.offset);

    ///< UV buffer
    map.handle = _buffer_object.handle[1];
    drmIoctl(_fd, DRM_IOCTL_MODE_MAP_DUMB, &map);
    _buffer_object.vaddr[1] = (uint8_t *)mmap(0, _buffer_object.size[1], PROT_READ | PROT_WRITE,
                                              MAP_SHARED, _fd, map.offset);

    _init_nv12_frame_buffer_object = true;
    return true;
}

void DrmWrapper::free_frame_buffer_object() {
    if (!_init_nv12_frame_buffer_object) {
        base::LogDebug() << "not need free frame buffer object";
        return;
    }

    drmModeRmFB(_fd, _buffer_object.fb_id);

    for (uint32_t i = 0; i < kBufferObjectSize; i++) {
        if (_buffer_object.vaddr[i] != NULL && _buffer_object.size[i] > 0) {
            munmap(_buffer_object.vaddr[i], _buffer_object.size[i]);
        }
    }

    struct drm_mode_destroy_dumb destroy = {};
    for (uint32_t i = 0; i < 4; i++) {
        if (_buffer_object.handle[i] > 0) {
            destroy.handle = _buffer_object.handle[i];
            drmIoctl(_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
        }
    }

    _init_nv12_frame_buffer_object = false;
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