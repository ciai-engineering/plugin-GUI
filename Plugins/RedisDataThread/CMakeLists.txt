# Redis DataThread plugin build file
cmake_minimum_required(VERSION 3.15)

# include common rules
include(../PluginRules.cmake)

# Find hiredis library
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

# Always add the main sources
add_sources(${PLUGIN_NAME} RedisDataThread.cpp RedisDataThread.h
            RedisDataThreadEditor.cpp RedisDataThreadEditor.h
            RedisDataDisplayPopup.cpp RedisDataDisplayPopup.h
            RedisConnectionTestPopup.cpp RedisConnectionTestPopup.h
            RedisHelpPopup.cpp RedisHelpPopup.h
            RedisConfigurationPanel.cpp RedisConfigurationPanel.h
            RedisConfigurationPopup.cpp RedisConfigurationPopup.h
            CustomTooltipComponent.cpp CustomTooltipComponent.h)

if(NOT HIREDIS_FOUND)
    message(WARNING "hiredis library not found. RedisDataThread plugin will be built without Redis support.")
    message(WARNING "Install hiredis with: sudo apt-get install libhiredis-dev (Ubuntu/Debian)")
    message(WARNING "                     brew install hiredis (macOS)")

    # Add stub sources as well
    add_sources(${PLUGIN_NAME} RedisDataThreadStub.cpp RedisDataThreadStub.h)
else()
    message(STATUS "Found hiredis: ${HIREDIS_LIBRARIES}")

    # Link hiredis library
    target_link_libraries(${PLUGIN_NAME} ${HIREDIS_LIBRARIES})
    target_include_directories(${PLUGIN_NAME} PRIVATE ${HIREDIS_INCLUDE_DIRS})

    # Add compile definition to enable Redis support
    target_compile_definitions(${PLUGIN_NAME} PRIVATE REDIS_ENABLED=1)
endif()

if(APPLE)
    set_target_properties(
        ${PLUGIN_NAME} PROPERTIES XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER
                                  "org.open-ephys.plugin.RedisDataThread")
endif()

if(BUILD_TESTS)
    add_subdirectory(Tests)
endif()
