#!/bin/bash
set -e

# Build directory
BUILD_DIR="$SRC/build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake for fuzzing
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_C_FLAGS="-g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined" \
      -DLIB_FUZZING_ENGINE="-fsanitize=fuzzer" \
      "$SRC/chronos"

# Build all fuzz targets
cmake --build . -- -j$(nproc)

# Copy fuzz binaries to $OUT
for fuzzer in h2_frame_fuzzer hpack_fuzzer tls_fuzzer http_fuzzer stream_fuzzer \
             memory_pool_fuzzer frame_reassembly_fuzzer stream_priority_fuzzer \
             session_cache_fuzzer connection_state_fuzzer dynamic_table_ref_fuzzer \
             header_validator_fuzzer connection_pool_fuzzer flow_control_fuzzer \
             stream_dependency_fuzzer; do
    if [ -f "$fuzzer" ]; then
        cp "$fuzzer" "$OUT/"
    fi
done
