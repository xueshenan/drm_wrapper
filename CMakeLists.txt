project(qcomm-drm)

set(DRM_TEST_NAME drm_test)

add_subdirectory(base)

add_executable(${DRM_TEST_NAME}
    drm_wrapper.cc
    drm_test.cc
)

target_include_directories(${DRM_TEST_NAME} PUBLIC "/usr/include/drm")

target_link_libraries(${DRM_TEST_NAME} drm)
target_link_libraries(${DRM_TEST_NAME} base)

install(TARGETS ${DRM_TEST_NAME} RUNTIME DESTINATION "bin")
