cmake_minimum_required(VERSION 2.6)

project(open62541)

set(open62541_VERSION_MAJOR 0)
set(open62541_VERSION_MINOR 1)

# set(CMAKE_VERBOSE_MAKEFILE on )

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/CMakeModules")

# compiler options
if(CMAKE_COMPILER_IS_GNUCC)
add_definitions(-std=c99 -pedantic -pipe -fstack-protector -Wall -Wextra
                 -Wno-unused-parameter -Wno-unused-function -Wno-unused-label
                 -Wpointer-arith -Wformat -Wreturn-type -Wsign-compare -Wmultichar
                 -Wformat-nonliteral -Winit-self -Wuninitialized -Wno-deprecated
                 -Wformat-security -Werror -ffunction-sections -fdata-sections
                 -Wl,--gc-sections)
endif(CMAKE_COMPILER_IS_GNUCC)

# multithreading
set(MULTITHREADING OFF CACHE BOOL "Enable multithreading")
if(MULTITHREADING)
    find_package(Threads REQUIRED)
endif(MULTITHREADING)

# encodings
set(UA_ENCODING_AMOUNT 1) # binary encoding

set(UA_ENCODING_XML OFF CACHE BOOL "Enable XML-encoding of the UA types")
if(UA_ENCODING_XML)
    MATH(EXPR UA_ENCODING_AMOUNT "${UA_ENCODING_AMOUNT}+1")
    find_package(EXPAT REQUIRED)
    if(EXPAT_FOUND)
        include_directories(${EXPAT_INCLUDE_DIRS})
    else(EXPAT_FOUND)
        message(FATAL_ERROR "Expat library not found.")
    endif(EXPAT_FOUND)
endif(UA_ENCODING_XML)

set(UA_ENCODING_JSON OFF CACHE BOOL "Enable JSON-encoding of the UA types")
if(UA_ENCODING_JSON)
    MATH(EXPR UA_ENCODING_AMOUNT "${UA_ENCODING_AMOUNT}+1")
endif(UA_ENCODING_JSON)

# gcov
option(USE_GCOV "Enable gcov support" OFF)
if(USE_GCOV)
    message(STATUS "Enabling gcov support")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --coverage")
endif()

# directory for generated source files
file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/src_generated")
include_directories("${PROJECT_BINARY_DIR}/src_generated") 

# build the library
configure_file("src/ua_config.h.in" "${PROJECT_BINARY_DIR}/src_generated/ua_config.h")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src/util")

set(lib_sources src/ua_types.c
                src/ua_types_encoding_binary.c
                src/ua_application.c
                src/ua_transport.c
                src/ua_transport_binary.c
                src/ua_transport_binary_secure.c
                src/ua_services_attribute.c
                src/ua_services_session.c
                src/ua_services_discovery.c
                src/ua_services_securechannel.c
                src/ua_services_nodemanagement.c
                src/ua_services_view.c
                src/ua_services_subscription.c
                src/ua_services_monitoreditems.c
                src/ua_stack_channel.c
                src/ua_stack_channel_manager.c
                src/ua_stack_session_manager.c
                src/ua_stack_session.c
                src/ua_transport_connection.c
                src/ua_transport_connection_manager.c
                src/util/ua_util.c
                src/util/ua_list.c
                src/util/ua_indexedList.c
                src/util/ua_base64.c
                ${PROJECT_BINARY_DIR}/src_generated/ua_types_generated.c
                ${PROJECT_BINARY_DIR}/src_generated/ua_namespace_0.c)

if(MULTITHREADING)
    list(APPEND lib_sources src/ua_namespace_concurrent.c)
else()
    list(APPEND lib_sources src/ua_namespace.c)
endif(MULTITHREADING)

set(generate_src_options "")

if(UA_ENCODING_XML)
    list(APPEND lib_sources src/ua_types_encoding_xml.c
                            src/ua_namespace_xml.c
                            src/ua_xml.c)
    set(generate_src_options "${generate_src_options}--with-xml")
endif(UA_ENCODING_XML)

if(UA_ENCODING_JSON)
    list(APPEND lib_sources src/ua_types_encoding_json.c)
    set(generate_src_options "${generate_src_options}--with-json")
endif(UA_ENCODING_JSON)

add_library(open62541 ${lib_sources})

# generate data structures
add_custom_command(OUTPUT ${PROJECT_BINARY_DIR}/src_generated/ua_types_generated.c
                          ${PROJECT_BINARY_DIR}/src_generated/ua_types_generated.h
                   COMMAND python ${PROJECT_SOURCE_DIR}/tools/generate_builtin.py ${generate_src_options} ${PROJECT_SOURCE_DIR}/schema/Opc.Ua.Types.bsd ${PROJECT_BINARY_DIR}/src_generated/ua_types_generated
                   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_builtin.py
                           ${CMAKE_CURRENT_SOURCE_DIR}/schema/Opc.Ua.Types.bsd)

add_custom_command(OUTPUT ${PROJECT_BINARY_DIR}/src_generated/ua_namespace_0.c
                          ${PROJECT_BINARY_DIR}/src_generated/ua_namespace_0.h
                   COMMAND python ${PROJECT_SOURCE_DIR}/tools/generate_namespace.py ${generate_src_options} ${PROJECT_SOURCE_DIR}/schema/NodeIds.csv ${PROJECT_BINARY_DIR}/src_generated/ua_namespace_0
                   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_namespace.py
                           ${CMAKE_CURRENT_SOURCE_DIR}/schema/NodeIds.csv)

# download queue.h if required
if(WIN32)
    if(NOT EXISTS "${PROJECT_BINARY_DIR}/src_generated/queue.h")
        file(DOWNLOAD "http://openbsd.cs.toronto.edu/cgi-bin/cvsweb/~checkout~/src/sys/sys/queue.h" "${PROJECT_BINARY_DIR}/src_generated/queue.h" STATUS result)
        list(GET result 0 download_ok)
        if(NOT ${download_ok} MATCHES 0)
            file(REMOVE "${PROJECT_BINARY_DIR}/src_generated/queue.h") # remove empty file if created
            message(FATAL_ERROR "queue.h could not be downloaded")
        endif()
    endif()
endif(WIN32)

# build example implementations
add_executable(exampleServer examples/src/opcuaServer.c
                              examples/src/networklayer.c)
target_link_libraries(exampleServer open62541) 

# generate documentation
option(GENERATE_DOCUMENTATION "Generate doxygen documentation" OFF)
if(GENERATE_DOCUMENTATION)
    find_package(Doxygen)
        if(NOT DOXYGEN_FOUND)
            message(FATAL_ERROR "Doxygen is not installed or not properly configured")
        endif(NOT DOXYGEN_FOUND)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(doc
                      ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
                      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                      COMMENT "Generating API documentation with Doxygen")
endif(GENERATE_DOCUMENTATION)

# run unit tests
option(UNIT_TESTS "Run unit tests after building" OFF)
if(UNIT_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif(UNIT_TESTS)