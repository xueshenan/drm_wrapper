project(drm_lib)

set(DRM_LIB_NAME drm_lib)

add_library(${DRM_LIB_NAME} STATIC
    drm_utils.cc
    drm_wrapper.cc
)

target_include_directories(${DRM_LIB_NAME} PUBLIC "/usr/include/drm")
target_include_directories(${DRM_LIB_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../)

target_link_libraries(${DRM_LIB_NAME} drm)
target_link_libraries(${DRM_LIB_NAME} base)

