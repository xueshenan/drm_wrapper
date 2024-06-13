
#include <drm_fourcc.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

struct buffer_object {
    uint32_t width;
    uint32_t height;
    uint32_t pitch[4];
    uint32_t handle[4];
    uint32_t size[4];
    uint8_t *vaddr[4];
    uint32_t fb_id;
};

static int modeset_create_fb(int fd, struct buffer_object *bo) {
    uint32_t pixel_format = DRM_FORMAT_NV12;  //DRM_FORMAT_NV12 DRM_FORMAT_XRGB8888

    struct drm_mode_create_dumb create = {};
    ///< Y buffer
    create.width = bo->width;
    create.height = bo->height;
    create.bpp = 8;
    /* handle, pitch, size will be returned */
    int ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);

    bo->pitch[0] = create.pitch;
    bo->size[0] = create.size;
    bo->handle[0] = create.handle;

    printf("drm create Y dump pitch:%d, size:%lld,handle:%d\n", create.pitch, create.size,
           create.handle);

    ///< UV buffer
    create.width = bo->width;
    create.height = bo->height / 2;
    create.bpp = 8;

    ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);
    printf("drm create UV dumb pitch:%d, size:%lld,handle:%d\n", create.pitch, create.size,
           create.handle);
    bo->pitch[1] = create.pitch;
    bo->size[1] = create.size;
    bo->handle[1] = create.handle;

    uint32_t bo_handles[4] = {
        0,
    };
    uint32_t pitches[4] = {
        0,
    };
    uint32_t offsets[4] = {
        0,
    };

    bo_handles[0] = bo->handle[0];
    bo_handles[1] = bo->handle[1];
    pitches[0] = bo->pitch[0];
    pitches[1] = bo->pitch[1];
    offsets[0] = 0;
    offsets[1] = 0;

    ret = drmModeAddFB2(fd, create.width, create.height, pixel_format, bo_handles, pitches, offsets,
                        &bo->fb_id, 0);

    // ret =
    //     drmModeAddFB(fd, create.width, create.height, 24, create.bpp, bo->pitch, bo->handle, &bo->fb_id);
    printf("addFB ret:%d, fb_id:%d\n", ret, bo->fb_id);

    FILE *fp = fopen("sample.yuv", "rb");
    if (fp == NULL) {
        printf("cannot open sample yuv\n");
        return 1;
    }
    int buffer_size = bo->width * bo->height * 3 / 2;
    uint8_t *mem_buffer = (uint8_t *)malloc(buffer_size);
    int read_size = fread(mem_buffer, buffer_size, 1, fp);
    fclose(fp);

    struct drm_mode_map_dumb map = {};
    map.handle = bo->handle[0];
    drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map);

    ///< Y buffer
    bo->vaddr[0] =
        (uint8_t *)mmap(0, bo->size[0], PROT_READ | PROT_WRITE, MAP_SHARED, fd, map.offset);
    printf("vaddress 0 fd : %d, handle : %d, vaddr: %p\n", fd, bo->handle[0], bo->vaddr[0]);

    memcpy(bo->vaddr[0], mem_buffer, bo->width * bo->height);

    struct drm_mode_map_dumb map2 = {};
    map2.handle = bo->handle[1];
    drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map2);
    bo->vaddr[1] =
        (uint8_t *)mmap(0, bo->size[1], PROT_READ | PROT_WRITE, MAP_SHARED, fd, map2.offset);
    printf("vaddress 0 fd : %d, handle : %d, vaddr: %p\n", fd, bo->handle[1], bo->vaddr[1]);
    memcpy(bo->vaddr[1], mem_buffer + bo->width * bo->height, bo->width * bo->height / 2);

    // uint8_t *addr = bo->vaddr;
    // for (uint32_t i = 0; i < bo->height; i++) {
    //     for (uint32_t j = 0; j < bo->pitch; j += 4) {
    //         addr[j + 0] = 0x33;  // B
    //         addr[j + 1] = 0xff;  // G
    //         addr[j + 2] = 0xff;  // R
    //         addr[j + 3] = 0xff;  // A
    //     }
    //     addr += bo->pitch;
    // }

    return 0;
}

static void modeset_destroy_fb(int fd, struct buffer_object *bo) {
    drmModeRmFB(fd, bo->fb_id);

    for (uint32_t i = 0; i < 4; i++) {
        if (bo->vaddr[i] != NULL && bo->size[i] > 0) {
            munmap(bo->vaddr[i], bo->size[i]);
        }
    }

    struct drm_mode_destroy_dumb destroy = {};
    for (uint32_t i = 0; i < 4; i++) {
        if (bo->handle[i] > 0) {
            destroy.handle = bo->handle[i];
            drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
        }
    }
}

static void log_drm_version(int fd) {
    drmVersion *version = NULL;

    version = drmGetVersion(fd);
    if (version != NULL) {
        printf("DRM v%d.%d.%d [%s — %s — %s]\n", version->version_major, version->version_minor,
               version->version_patchlevel, version->name, version->desc, version->date);
        drmFreeVersion(version);
    } else {
        printf("could not get driver information\n");
    }
}

int main(int argc, char **argv) {
    int fd;
    drmModeConnector *conn;
    drmModeRes *res;
    drmModePlaneRes *plane_res;
    uint32_t conn_id;
    uint32_t crtc_id;
    uint32_t plane_id;
    struct buffer_object buf;

    const char *driver_name = "msm_drm";
    memset(&buf, 0, sizeof(buffer_object));

    fd = drmOpen(driver_name, NULL);
    // fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        printf("open %s failed:%d\n", driver_name, errno);
        return 0;
    }
    printf("open %s result fd %d\n", driver_name, fd);
    // log_drm_version(fd);

    res = drmModeGetResources(fd);
    if (res == NULL) {
        printf("cannot get drm mode resource:%d\n", errno);
        return 0;
    }
    printf("drmModeGetResources success : %p\n", res);
    crtc_id = res->crtcs[0];
    conn_id = res->connectors[0];
    printf("drm get crtc id:%d, connector id:%d\n", crtc_id, conn_id);

    drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    plane_res = drmModeGetPlaneResources(fd);
    plane_id = plane_res->planes[0];
    printf("plane id is %d\n", plane_id);

    conn = drmModeGetConnector(fd, conn_id);
    buf.width = conn->modes[0].hdisplay;
    buf.height = conn->modes[0].vdisplay;

    modeset_create_fb(fd, &buf);

    drmModeSetCrtc(fd, crtc_id, buf.fb_id, 0, 0, &conn_id, 1, &conn->modes[0]);

    getchar();

    /* crop the rect from framebuffer(100, 150) to crtc(50, 50) */
    int crop_w = buf.width;
    int crop_h = buf.height / 2;
    drmModeSetPlane(fd, plane_id, crtc_id, buf.fb_id, 0, 0, 0, crop_w, crop_h, 0, 0, crop_w << 16,
                    crop_h << 16);

    getchar();

    modeset_destroy_fb(fd, &buf);

    drmModeFreeConnector(conn);
    drmModeFreePlaneResources(plane_res);
    drmModeFreeResources(res);

    close(fd);

    return 0;
}
