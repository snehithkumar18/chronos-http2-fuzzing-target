#!/bin/bash -eu
set -e

# Build the project
cd /src
mkdir -p build
cd build

# Configure with fuzzing engine
cmake .. -DCMAKE_BUILD_TYPE=Release -DLIB_FUZZING_ENGINE=$LIB_FUZZING_ENGINE

# Build
cmake --build . -- -j$(nproc)

# Copy fuzz targets to $OUT
cp h2_frame_fuzzer $OUT/
cp hpack_fuzzer $OUT/
cp tls_fuzzer $OUT/
cp http_fuzzer $OUT/
cp stream_fuzzer $OUT/
