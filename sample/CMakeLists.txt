project(drm_sample)

set(DRM_FPS_SAMPLE drm_fps)

add_executable(${DRM_FPS_SAMPLE}
    drm_fps.cc
)

target_include_directories(${DRM_FPS_SAMPLE} PUBLIC "/usr/include/drm")
target_link_libraries(${DRM_FPS_SAMPLE} drm)

install(TARGETS ${DRM_FPS_SAMPLE} RUNTIME DESTINATION "bin")


set(DRM_HELLO_SAMPLE drm_hello)

add_executable(${DRM_HELLO_SAMPLE}
    drm_hello.cc
)

target_include_directories(${DRM_HELLO_SAMPLE} PUBLIC "/usr/include/drm")
target_link_libraries(${DRM_HELLO_SAMPLE} drm)

install(TARGETS ${DRM_HELLO_SAMPLE} RUNTIME DESTINATION "bin")