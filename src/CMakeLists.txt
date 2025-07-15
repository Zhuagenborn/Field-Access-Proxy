add_library(${LIB_NAME} INTERFACE)

set(HEADER_PATH ${PROJECT_SOURCE_DIR}/include/${LIB_NAME})

target_include_directories(${LIB_NAME}
    INTERFACE
        ${PROJECT_SOURCE_DIR}/include
)

target_sources(${LIB_NAME}
    INTERFACE
        ${HEADER_PATH}/${LIB_NAME}.h
)

target_link_libraries(${LIB_NAME}
    INTERFACE
        bit_manip
)