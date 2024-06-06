set(BASE_LIBRARY_NAME "base")

aux_source_directory("." SRC_BASE)

add_library(${BASE_LIBRARY_NAME} STATIC ${SRC_BASE})