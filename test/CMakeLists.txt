project(drm_test)

set(DRM_TEST_NAME drm_test)

add_executable(${DRM_TEST_NAME}
    drm_test.cc
)

target_include_directories(${DRM_TEST_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../)

target_link_libraries(${DRM_TEST_NAME} drm)
target_link_libraries(${DRM_TEST_NAME} base)
target_link_libraries(${DRM_TEST_NAME} drm_lib)

install(TARGETS ${DRM_TEST_NAME} RUNTIME DESTINATION "bin")
