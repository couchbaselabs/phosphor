CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
PROJECT(Phosphor)

INCLUDE(CTest)

IF (${CMAKE_MAJOR_VERSION} GREATER 2)
    CMAKE_POLICY(SET CMP0042 NEW)
ENDIF ()

IF(NOT DEFINED COUCHBASE_SERVER_BUILD)
    INCLUDE(NonServerBuild.cmake)
ENDIF ()

INCLUDE_DIRECTORIES(BEFORE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/phosphor
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

SET(SOURCE_FILES
        src/sentinel.cc
        src/string_utils.cc
        src/trace_buffer.cc
        src/trace_event.cc
        src/trace_log.cc)

ADD_LIBRARY(phosphor SHARED ${SOURCE_FILES})

IF(DEFINED gtest_SOURCE_DIR)
    INCLUDE_DIRECTORIES(AFTER
            ${gtest_SOURCE_DIR}/include
            ${gmock_SOURCE_DIR}/include)

    ADD_SUBDIRECTORY(tests)
ENDIF ()

ENABLE_CODE_COVERAGE_REPORT()