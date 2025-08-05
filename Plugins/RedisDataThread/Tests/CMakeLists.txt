# Redis DataThread Tests
cmake_minimum_required(VERSION 3.15)

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
    # Test executable for Redis stream functionality
    add_executable(test_redis_streams
        test_redis_streams.cpp
        test_brandbci_parser.cpp
        test_stream_manager.cpp
        ../RedisDataThread.cpp
        ../RedisDataThread.h
    )

    target_include_directories(test_redis_streams PRIVATE
        ${HIREDIS_INCLUDE_DIRS}
        ..
        ../../Headers
        ../../../Source
    )

    target_link_libraries(test_redis_streams
        ${HIREDIS_LIBRARIES}
    )

    target_compile_definitions(test_redis_streams PRIVATE
        REDIS_ENABLED=1
        TESTABLE=
        UNIT_TESTING=1
    )

    # Integration test executable
    add_executable(test_brandbci_integration
        test_integration.cpp
        ../RedisDataThread.cpp
        ../RedisDataThread.h
    )

    target_include_directories(test_brandbci_integration PRIVATE
        ${HIREDIS_INCLUDE_DIRS}
        ..
        ../../Headers
        ../../../Source
    )

    target_link_libraries(test_brandbci_integration
        ${HIREDIS_LIBRARIES}
    )

    target_compile_definitions(test_brandbci_integration PRIVATE
        REDIS_ENABLED=1
        TESTABLE=
        INTEGRATION_TESTING=1
    )

    # Add tests to CTest
    enable_testing()
    add_test(NAME RedisStreamTests COMMAND test_redis_streams)
    add_test(NAME BrandBCIIntegrationTests COMMAND test_brandbci_integration)

else()
    message(WARNING "hiredis not found - skipping Redis tests")
endif()
