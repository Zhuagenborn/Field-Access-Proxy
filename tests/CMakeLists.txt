set(TEST_NAME ${LIB_NAME}_tests)

add_executable(${TEST_NAME})

target_sources(${TEST_NAME}
    PRIVATE
        endian.h
        c_style_tests.cpp
        macro_defined_tests.cpp
)

target_link_libraries(${TEST_NAME}
    PRIVATE
        ${LIB_NAME}
        ${GTEST_LIB}
)

gtest_discover_tests(${TEST_NAME})