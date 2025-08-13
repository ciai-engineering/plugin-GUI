# Redis DataThread Tests
cmake_minimum_required(VERSION 3.15)
project(RedisDataThreadTests)

# Find required packages
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(HIREDIS hiredis)
endif()

# If pkg-config didn't find it, try manual search
if(NOT HIREDIS_FOUND)
    find_path(HIREDIS_INCLUDE_DIR hiredis/hiredis.h
        PATHS /usr/include /usr/local/include /opt/homebrew/include)
    find_library(HIREDIS_LIBRARY hiredis
        PATHS /usr/lib /usr/local/lib /opt/homebrew/lib)

    if(HIREDIS_INCLUDE_DIR AND HIREDIS_LIBRARY)
        set(HIREDIS_FOUND TRUE)
        set(HIREDIS_INCLUDE_DIRS ${HIREDIS_INCLUDE_DIR})
        set(HIREDIS_LIBRARIES ${HIREDIS_LIBRARY})
    endif()
endif()

if(HIREDIS_FOUND)
    # Simple unit tests that don't require full JUCE framework
    add_executable(test_redis_simple
        simple_redis_test.cpp
    )

    target_include_directories(test_redis_simple PRIVATE
        ${HIREDIS_INCLUDE_DIRS}
        ..
    )

    target_link_libraries(test_redis_simple
        ${HIREDIS_LIBRARIES}
    )

    target_compile_definitions(test_redis_simple PRIVATE
        REDIS_ENABLED=1
    )

    # Standalone integration test
    add_executable(test_integration_standalone
        test_integration.cpp
    )

    target_include_directories(test_integration_standalone PRIVATE
        ${HIREDIS_INCLUDE_DIRS}
        ..
    )

    target_link_libraries(test_integration_standalone
        ${HIREDIS_LIBRARIES}
    )

    target_compile_definitions(test_integration_standalone PRIVATE
        REDIS_ENABLED=1
        INTEGRATION_TESTING=1
    )

    # Add tests to CTest
    enable_testing()
    add_test(NAME RedisSimpleTests COMMAND test_redis_simple)
    add_test(NAME IntegrationStandaloneTests COMMAND test_integration_standalone)

else()
    message(WARNING "hiredis not found - skipping Redis tests")
endif()
