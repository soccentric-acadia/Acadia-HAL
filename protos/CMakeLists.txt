# protos/CMakeLists.txt
#
# Protobuf + gRPC code generation and hal_protos static library.
#
# Produced target : hal_protos (STATIC)
# Consumers       : target_link_libraries(<target> hal_protos)
# Include paths   : automatically propagated via target properties
#
# Generated header locations (relative to the build-time include root):
#   common.pb.h
#   hal_service.pb.h          hal_service.grpc.pb.h
#   modules/audio_interfaces.pb.h
#   modules/cpu.pb.h
#   ... (one .pb.h per module proto)
#
# Example client usage after linking hal_protos:
#   #include "common.pb.h"
#   #include "hal_service.grpc.pb.h"
#   #include "modules/cpu.pb.h"

cmake_minimum_required(VERSION 3.10)

# ============================================================
# 1. Locate Protobuf
# ============================================================
find_package(Protobuf REQUIRED)
message(STATUS "[hal_protos] protobuf ${Protobuf_VERSION}  (${Protobuf_INCLUDE_DIRS})")
message(STATUS "[hal_protos] protoc   ${Protobuf_PROTOC_EXECUTABLE}")

# ============================================================
# 2. Locate gRPC (CMake config preferred; pkg-config fallback)
# ============================================================
find_package(gRPC CONFIG QUIET)
if(gRPC_FOUND)
    message(STATUS "[hal_protos] gRPC ${gRPC_VERSION} found via CMake config")
    set(HAL_GRPC_LIBRARIES gRPC::grpc++)
    get_target_property(GRPC_CPP_PLUGIN gRPC::grpc_cpp_plugin LOCATION)
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(GRPCPP QUIET grpc++)
    endif()
    if(GRPCPP_FOUND)
        message(STATUS "[hal_protos] gRPC found via pkg-config")
        set(HAL_GRPC_LIBRARIES ${GRPCPP_LIBRARIES})
        find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin
            HINTS /usr/bin /usr/local/bin
            DOC   "Path to the grpc_cpp_plugin protoc plugin")
    else()
        message(WARNING
            "[hal_protos] gRPC not found. "
            "hal_protos will be built without gRPC stubs. "
            "Install libgrpc++-dev and protobuf-compiler-grpc, then re-run cmake.")
        set(HAL_GRPC_LIBRARIES "")
        set(GRPC_CPP_PLUGIN    "")
    endif()
endif()

# ============================================================
# 3. Output directory for generated sources
# ============================================================
set(PROTO_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${PROTO_BINARY_DIR}")
file(MAKE_DIRECTORY "${PROTO_BINARY_DIR}/modules")

# ============================================================
# 4. Collect proto files
# ============================================================
set(COMMON_PROTO  "${CMAKE_CURRENT_SOURCE_DIR}/common.proto")
set(SERVICE_PROTO "${CMAKE_CURRENT_SOURCE_DIR}/hal_service.proto")
file(GLOB MODULE_PROTOS "${CMAKE_CURRENT_SOURCE_DIR}/modules/*.proto")

# All protos that need plain protobuf C++ generation
set(ALL_PROTOS ${COMMON_PROTO} ${SERVICE_PROTO} ${MODULE_PROTOS})

# ============================================================
# 5. Generate protobuf C++ sources for every proto file
# ============================================================
set(PROTO_SRCS "")
set(PROTO_HDRS "")

foreach(PROTO_FILE IN LISTS ALL_PROTOS)
    # Derive output paths mirroring the source tree layout under PROTO_BINARY_DIR
    file(RELATIVE_PATH PROTO_REL "${CMAKE_CURRENT_SOURCE_DIR}" "${PROTO_FILE}")
    get_filename_component(PROTO_REL_DIR  "${PROTO_REL}" DIRECTORY)
    get_filename_component(PROTO_NAME_WE  "${PROTO_FILE}" NAME_WE)

    if(PROTO_REL_DIR)
        set(OUT_SUBDIR "${PROTO_BINARY_DIR}/${PROTO_REL_DIR}")
    else()
        set(OUT_SUBDIR "${PROTO_BINARY_DIR}")
    endif()
    file(MAKE_DIRECTORY "${OUT_SUBDIR}")

    set(PROTO_SRC "${OUT_SUBDIR}/${PROTO_NAME_WE}.pb.cc")
    set(PROTO_HDR "${OUT_SUBDIR}/${PROTO_NAME_WE}.pb.h")

    add_custom_command(
        OUTPUT  "${PROTO_SRC}" "${PROTO_HDR}"
        COMMAND "${Protobuf_PROTOC_EXECUTABLE}"
                "--proto_path=${CMAKE_CURRENT_SOURCE_DIR}"
                "--proto_path=${Protobuf_INCLUDE_DIRS}"
                "--cpp_out=${PROTO_BINARY_DIR}"
                "${PROTO_FILE}"
        DEPENDS "${PROTO_FILE}" "${COMMON_PROTO}"
        COMMENT "[protoc] generating C++ for ${PROTO_REL}"
        VERBATIM
    )

    list(APPEND PROTO_SRCS "${PROTO_SRC}")
    list(APPEND PROTO_HDRS "${PROTO_HDR}")
endforeach()

# ============================================================
# 6. Generate gRPC stubs for hal_service.proto only
# ============================================================
set(GRPC_SRC "")
set(GRPC_HDR "")

if(GRPC_CPP_PLUGIN)
    set(GRPC_SRC "${PROTO_BINARY_DIR}/hal_service.grpc.pb.cc")
    set(GRPC_HDR "${PROTO_BINARY_DIR}/hal_service.grpc.pb.h")

    add_custom_command(
        OUTPUT  "${GRPC_SRC}" "${GRPC_HDR}"
        COMMAND "${Protobuf_PROTOC_EXECUTABLE}"
                "--proto_path=${CMAKE_CURRENT_SOURCE_DIR}"
                "--proto_path=${Protobuf_INCLUDE_DIRS}"
                "--grpc_out=${PROTO_BINARY_DIR}"
                "--plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}"
                "${SERVICE_PROTO}"
        DEPENDS "${SERVICE_PROTO}" "${COMMON_PROTO}"
        COMMENT "[protoc] generating gRPC stubs for hal_service.proto"
        VERBATIM
    )
endif()

# ============================================================
# 7. Build the hal_protos static library
# ============================================================
add_library(hal_protos STATIC
    ${PROTO_SRCS}
    ${GRPC_SRC}
)

# Suppress warnings in protoc-generated code (they are not our responsibility)
target_compile_options(hal_protos PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:   -w>
    $<$<CXX_COMPILER_ID:Clang>: -w>
)

target_include_directories(hal_protos
    PUBLIC
        # Generated headers live here; consumers can #include "modules/cpu.pb.h"
        "${PROTO_BINARY_DIR}"
        # Well-known protos (google/protobuf/*.proto)
        "${Protobuf_INCLUDE_DIRS}"
)

target_link_libraries(hal_protos
    PUBLIC
        protobuf::libprotobuf
        ${HAL_GRPC_LIBRARIES}
)

# ============================================================
# 8. Install rules (optional — integrate with the repo install target)
# ============================================================
install(TARGETS hal_protos
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Install generated headers, preserving the modules/ subdirectory structure
install(
    DIRECTORY   "${PROTO_BINARY_DIR}/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/hal_protos"
    FILES_MATCHING
        PATTERN "*.pb.h"
)
