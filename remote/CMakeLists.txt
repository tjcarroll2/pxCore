cmake_minimum_required(VERSION 2.8)
project(rtremote)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

option(BUILD_RTREMOTE_SHARED_LIB "BUILD_RTREMOTE_SHARED_LIB" ON)
option(BUILD_RTREMOTE_STATIC_LIB "BUILD_RTREMOTE_STATIC_LIB" ON)
option(BUILD_RTREMOTE_SAMPLE_APP_SHARED "BUILD_RTREMOTE_SAMPLE_APP_SHARED" OFF)
option(BUILD_RTREMOTE_SAMPLE_APP_STATIC "BUILD_RTREMOTE_SAMPLE_APP_STATIC" OFF)
option(BUILD_RTREMOTE_SAMPLE_APP_SIMPLE "BUILD_RTREMOTE_SAMPLE_APP_SIMPLE" OFF)
option(ENABLE_RTREMOTE_DEBUG "ENABLE_RTREMOTE_DEBUG" OFF)
option(ENABLE_RTREMOTE_PROFILE "ENABLE_RTREMOTE_PROFILE" OFF)

set(RTREMOTE_SOURCE_FILES rtremote.conf.gen rtRemoteConfig.h rtRemoteServer.cpp rtRemoteObject.cpp
        rtRemoteFunction.cpp rtRemoteMessage.cpp rtRemoteClient.cpp rtRemoteValueReader.cpp
        rtRemoteValueWriter.cpp rtRemoteSocketUtils.cpp rtRemoteStream.cpp
        rtRemoteObjectCache.cpp rtRemote.cpp rtRemoteConfig.cpp rtRemoteFactory.cpp
        rtRemoteFileResolver.cpp rtRemoteMulticastResolver.cpp rtRemoteNsResolver.cpp
        rtRemoteNameService.cpp rtRemoteConfigBuilder.cpp rtRemoteAsyncHandle.cpp
        rtRemoteUtils.cpp rtRemoteEnvironment.cpp rtRemoteStreamSelector.cpp rtGuid.cpp
        rtRemoteMapperMap.cpp rtRemoteMapperFile.cpp rtRemoteMapperSql.cpp
        rtRemoteEndpointHandleStreamServer.cpp)

add_definitions(-DRAPIDJSON_HAS_STDSTRING -DRT_PLATFORM_LINUX -DRT_REMOTE_LOOPBACK_ONLY)
include_directories(AFTER ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../src ${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")

set(LIBRARY_LINKER_OPTIONS -pthread -ldl -luuid -Wl,-rpath=../../,--enable-new-dtags)
set(RTREMOTE_LINK_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../src ${CMAKE_CURRENT_SOURCE_DIR}/../build/glut ${CMAKE_CURRENT_SOURCE_DIR}/../build/egl)

add_executable(rtRemoteConfigGen rtRemoteConfigGen.cpp)
set_target_properties(rtRemoteConfigGen
                      PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

add_custom_command(OUTPUT rtremote.conf.gen
                   DEPENDS rtRemoteConfigGen rtremote.conf.ac
                   COMMENT "Generating rtremote.conf.gen"
                   COMMAND rtRemoteConfigGen -i ${CMAKE_CURRENT_SOURCE_DIR}/rtremote.conf.ac -c -o ${CMAKE_CURRENT_BINARY_DIR}/rtremote.conf.gen)

add_custom_command(OUTPUT rtRemoteConfig.h
                   DEPENDS rtRemoteConfigGen rtremote.conf.ac
                   COMMENT "Generating rtRemoteConfig.h"
                   COMMAND rtRemoteConfigGen -i ${CMAKE_CURRENT_SOURCE_DIR}/rtremote.conf.ac -h -o ${CMAKE_CURRENT_BINARY_DIR}/rtRemoteConfig.h)

add_custom_command(OUTPUT rtRemoteConfigBuilder.cpp
                   DEPENDS rtRemoteConfigGen rtremote.conf.ac
                   COMMENT "Generating rtRemoteConfigBuilder.cpp"
                   COMMAND rtRemoteConfigGen -i ${CMAKE_CURRENT_SOURCE_DIR}/rtremote.conf.ac -s -o ${CMAKE_CURRENT_BINARY_DIR}/rtRemoteConfigBuilder.cpp)

if (ENABLE_RTREMOTE_DEBUG)
    message("Enabling rtRemote debug")
    add_definitions(-DRT_RPC_DEBUG -DRT_DEBUG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -fno-inline")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
endif (ENABLE_RTREMOTE_DEBUG)

if (ENABLE_RTREMOTE_PROFILE)
    message("Enabling rtRemote profile")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
endif (ENABLE_RTREMOTE_PROFILE)

if (BUILD_RTREMOTE_SHARED_LIB)
    message("Building rtRemote shared lib")
    add_library(rtremote_shared SHARED ${RTREMOTE_SOURCE_FILES})
    set_target_properties(rtremote_shared PROPERTIES OUTPUT_NAME "rtRemote")
    target_link_libraries(rtremote_shared ${LIBRARY_LINKER_OPTIONS})
endif (BUILD_RTREMOTE_SHARED_LIB)

if (BUILD_RTREMOTE_STATIC_LIB)
    message("Building rtRemote static lib")
    add_library(rtremote_static STATIC ${RTREMOTE_SOURCE_FILES})
    set_target_properties(rtremote_static PROPERTIES OUTPUT_NAME "rtRemote_s")
endif (BUILD_RTREMOTE_STATIC_LIB)

if (BUILD_RTREMOTE_SAMPLE_APP_SHARED)
    message ("Building rtRemote sample app using shared library")
    link_directories(${RTREMOTE_LINK_DIRECTORIES})
    add_executable(rtremote_sample_app_shared rtRemoteConfig.h rpc_main.cpp)
    set_target_properties(rtremote_sample_app_shared PROPERTIES OUTPUT_NAME "rpcSampleApp")
    target_link_libraries(rtremote_sample_app_shared ${LIBRARY_LINKER_OPTIONS} -lrtCore rtremote_shared -luuid)
    target_compile_definitions(rtremote_sample_app_shared PRIVATE RT_PLATFORM_LINUX RAPIDJSON_HAS_STDSTRING)
endif (BUILD_RTREMOTE_SAMPLE_APP_SHARED)

if (BUILD_RTREMOTE_SAMPLE_APP_STATIC)
    message ("Building rtRemote sample app using static library")
    link_directories(${RTREMOTE_LINK_DIRECTORIES})
    add_executable(rtremote_sample_app_static rtRemoteConfig.h rpc_main.cpp)
    set_target_properties(rtremote_sample_app_static PROPERTIES OUTPUT_NAME "rpcSampleApp_s")
    target_link_libraries(rtremote_sample_app_static ${LIBRARY_LINKER_OPTIONS} -lrtCore_s rtremote_static -luuid)
endif (BUILD_RTREMOTE_SAMPLE_APP_STATIC)

if (BUILD_RTREMOTE_SAMPLE_APP_SIMPLE)
    message ("Building rtRemote sample app with client and server")
    set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS} -std=c++11")
    link_directories(${RTREMOTE_LINK_DIRECTORIES})
    #client
    add_executable(rtremote_sample_app_client rtSampleClient.cpp)
    set_target_properties(rtremote_sample_app_client PROPERTIES OUTPUT_NAME "rtSampleClient")
    target_link_libraries(rtremote_sample_app_client ${LIBRARY_LINKER_OPTIONS} -lrtCore rtremote_shared -luuid)
    target_compile_definitions(rtremote_sample_app_client PRIVATE RT_PLATFORM_LINUX RAPIDJSON_HAS_STDSTRING)
    #server
    add_executable(rtremote_sample_app_server rtSampleServer.cpp)
    set_target_properties(rtremote_sample_app_server PROPERTIES OUTPUT_NAME "rtSampleServer")
    target_link_libraries(rtremote_sample_app_server ${LIBRARY_LINKER_OPTIONS} -lrtCore rtremote_shared -luuid)
    target_compile_definitions(rtremote_sample_app_server PRIVATE RT_PLATFORM_LINUX RAPIDJSON_HAS_STDSTRING)
endif (BUILD_RTREMOTE_SAMPLE_APP_SIMPLE)
