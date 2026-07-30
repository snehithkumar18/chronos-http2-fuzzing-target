cmake_minimum_required(VERSION 3.10)
project(chronos C)

set(CMAKE_C_STANDARD 11)

# Enable parallel builds and unity build for faster compilation
set(CMAKE_BUILD_PARALLEL_LEVEL 8)
set(CMAKE_UNITY_BUILD ON)
set(CMAKE_UNITY_BUILD_BATCH_SIZE 8)

# Platform-specific compiler flags
if(MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4 /O2 /MP")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2 -g")
endif()

# Use ccache if available for faster rebuilds
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

# Source files
set(PROTOCOL_SOURCES
    src/protocol/h2_frame.c
    src/protocol/h2_stream.c
    src/protocol/h2_connection.c
    src/protocol/h2_settings.c
    src/protocol/h2_push.c
    src/protocol/frame_reassembly.c
    src/protocol/stream_priority.c
    src/protocol/connection_state.c
    src/protocol/flow_control.c
    src/protocol/stream_dependency.c
)

set(HPACK_SOURCES
    src/hpack/hpack_decoder.c
    src/hpack/hpack_encoder.c
    src/hpack/hpack_table.c
    src/hpack/huffman.c
    src/hpack/dynamic_table_ref.c
)

set(TLS_SOURCES
    src/tls/tls_record.c
    src/tls/tls_handshake.c
    src/tls/tls_crypto.c
    src/tls/session_cache.c
)

set(HTTP_SOURCES
    src/http/http_parser.c
    src/http/http_request.c
    src/http/http_response.c
    src/http/header_validator.c
)

set(NET_SOURCES
    src/net/socket.c
    src/net/epoll.c
    src/net/buffer.c
    src/net/connection_pool.c
)

set(UTILS_SOURCES
    src/utils/checksum.c
    src/utils/base64.c
    src/utils/memory.c
    src/utils/memory_pool.c
    src/global_state.c
)

add_library(chronos STATIC
    ${PROTOCOL_SOURCES}
    ${HPACK_SOURCES}
    ${TLS_SOURCES}
    ${HTTP_SOURCES}
    ${NET_SOURCES}
    ${UTILS_SOURCES}
)

# Fuzz targets
add_executable(h2_frame_fuzzer ../fuzz/h2_frame_fuzzer.c)
target_link_libraries(h2_frame_fuzzer chronos ${LIB_FUZZING_ENGINE})

add_executable(hpack_fuzzer ../fuzz/hpack_fuzzer.c)
target_link_libraries(hpack_fuzzer chronos ${LIB_FUZZING_ENGINE})

add_executable(tls_fuzzer ../fuzz/tls_fuzzer.c)
target_link_libraries(tls_fuzzer chronos ${LIB_FUZZING_ENGINE})

add_executable(http_fuzzer ../fuzz/http_fuzzer.c)
target_link_libraries(http_fuzzer chronos ${LIB_FUZZING_ENGINE})

add_executable(stream_fuzzer ../fuzz/stream_fuzzer.c)
target_link_libraries(stream_fuzzer chronos ${LIB_FUZZING_ENGINE})

add_executable(memory_pool_fuzzer ../fuzz/memory_pool_fuzzer.c)
target_link_libraries(memory_pool_fuzzer chronos ${LIB_FUZZING_ENGINE})

add_executable(frame_reassembly_fuzzer ../fuzz/frame_reassembly_fuzzer.c)
target_link_libraries(frame_reassembly_fuzzer chronos ${LIB_FUZZING_ENGINE})

add_executable(stream_priority_fuzzer ../fuzz/stream_priority_fuzzer.c)
target_link_libraries(stream_priority_fuzzer chronos ${LIB_FUZZING_ENGINE})

add_executable(session_cache_fuzzer ../fuzz/session_cache_fuzzer.c)
target_link_libraries(session_cache_fuzzer chronos ${LIB_FUZZING_ENGINE})

add_executable(connection_state_fuzzer ../fuzz/connection_state_fuzzer.c)
target_link_libraries(connection_state_fuzzer chronos ${LIB_FUZZING_ENGINE})

add_executable(dynamic_table_ref_fuzzer ../fuzz/dynamic_table_ref_fuzzer.c)
target_link_libraries(dynamic_table_ref_fuzzer chronos ${LIB_FUZZING_ENGINE})

add_executable(header_validator_fuzzer ../fuzz/header_validator_fuzzer.c)
target_link_libraries(header_validator_fuzzer chronos ${LIB_FUZZING_ENGINE})

add_executable(connection_pool_fuzzer ../fuzz/connection_pool_fuzzer.c)
target_link_libraries(connection_pool_fuzzer chronos ${LIB_FUZZING_ENGINE})

add_executable(flow_control_fuzzer ../fuzz/flow_control_fuzzer.c)
target_link_libraries(flow_control_fuzzer chronos ${LIB_FUZZING_ENGINE})

add_executable(stream_dependency_fuzzer ../fuzz/stream_dependency_fuzzer.c)
target_link_libraries(stream_dependency_fuzzer chronos ${LIB_FUZZING_ENGINE})
