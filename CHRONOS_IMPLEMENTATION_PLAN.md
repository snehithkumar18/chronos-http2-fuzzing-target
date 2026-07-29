# CHRONOS HTTP/2 SERVER - COMPREHENSIVE IMPLEMENTATION PLAN

## PROJECT OVERVIEW

**Project Name:** Chronos HTTP/2 Server
**Language:** C (C11 standard)
**Target LOC:** 20,000+ lines of first-party source code
**Target Bugs:** 30 structural memory-safety bugs
**Difficulty Target:** 1-3/5 GPT-5.6 Terra solves (in-band)
**Platform:** AfterQuery Project Fenrir

**Critical Constraints:**
1. Repository LOCKED after submission - no updates possible
2. ALL 30 bugs MUST be triggered by fuzzing on AfterQuery platform
3. No patches during implementation - patches come later per bug
4. Local testing mandatory before private GitHub repo creation
5. Hermetic build (no network dependencies)
6. Private, original repository (AI-written-content check must pass)

## PROJECT ARCHITECTURE

```
chronos/
├── src/
│   ├── protocol/
│   │   ├── h2_frame.c/h           (3000 LOC)
│   │   ├── h2_stream.c/h          (2500 LOC)
│   │   ├── h2_connection.c/h      (2000 LOC)
│   │   ├── h2_settings.c/h        (1500 LOC)
│   │   └── h2_push.c/h            (1000 LOC)
│   ├── hpack/
│   │   ├── hpack_decoder.c/h      (2000 LOC)
│   │   ├── hpack_encoder.c/h      (1500 LOC)
│   │   ├── hpack_table.c/h        (1500 LOC)
│   │   └── huffman.c/h           (1500 LOC)
│   ├── tls/
│   │   ├── tls_record.c/h         (1500 LOC)
│   │   ├── tls_handshake.c/h      (2000 LOC)
│   │   └── tls_crypto.c/h         (1500 LOC)
│   ├── http/
│   │   ├── http_parser.c/h        (1500 LOC)
│   │   ├── http_request.c/h       (1500 LOC)
│   │   └── http_response.c/h      (1500 LOC)
│   ├── net/
│   │   ├── socket.c/h             (1000 LOC)
│   │   ├── epoll.c/h              (1500 LOC)
│   │   └── buffer.c/h             (1000 LOC)
│   └── utils/
│       ├── checksum.c/h           (800 LOC)
│       ├── base64.c/h             (800 LOC)
│       └── memory.c/h             (1200 LOC)
├── fuzz/
│   ├── h2_frame_fuzzer.c
│   ├── hpack_fuzzer.c
│   ├── tls_fuzzer.c
│   ├── http_fuzzer.c
│   ├── stream_fuzzer.c
│   └── corpus/
│       ├── h2_frame_fuzzer/
│       ├── hpack_fuzzer/
│       ├── tls_fuzzer/
│       ├── http_fuzzer/
│       └── stream_fuzzer/
├── .clusterfuzzlite/
│   ├── build.sh
│   └── project.yaml
├── CMakeLists.txt
├── README.md
└── .gitignore
```

## BUG DISTRIBUTION PLAN (30 BUGS TOTAL)

### LEVEL 1 BUGS (12 bugs - 40%)
**Characteristics:** Single-module scope, 1-2 fix locations, 10-30 minutes to solve

**Protocol Module (4 bugs):**
1. **h2_frame.c - Frame header parsing overflow**
   - Location: `parse_frame_header()` function
   - Type: Integer overflow in length calculation
   - Trigger: Malformed frame length field
   - Fuzzer path: h2_frame_fuzzer
   - Seed: Valid DATA frame with oversized length

2. **h2_stream.c - Missing null check in stream lookup**
   - Location: `get_stream_by_id()` function
   - Type: Null pointer dereference
   - Trigger: Invalid stream ID lookup
   - Fuzzer path: stream_fuzzer
   - Seed: Stream management sequence with invalid ID

3. **h2_settings.c - Settings parameter validation bypass**
   - Location: `apply_setting()` function
   - Type: Missing bounds check on settings value
   - Trigger: SETTINGS_MAX_FRAME_SIZE with invalid value
   - Fuzzer path: h2_frame_fuzzer
   - Seed: SETTINGS frame with oversized value

4. **h2_push.c - Push promise ID overflow**
   - Location: `handle_push_promise()` function
   - Type: Integer overflow in promised stream ID
   - Trigger: Push promise with max stream ID
   - Fuzzer path: h2_frame_fuzzer
   - Seed: PUSH_PROMISE frame with edge case ID

**HPACK Module (3 bugs):**
5. **hpack_decoder.c - Integer index overflow**
   - Location: `decode_integer()` function
   - Type: Integer overflow in index calculation
   - Trigger: Header with large index value
   - Fuzzer path: hpack_fuzzer
   - Seed: HPACK header block with large index

6. **hpack_table.c - Table size calculation overflow**
   - Location: `update_table_size()` function
   - Type: Integer overflow in size calculation
   - Trigger: Header with large size value
   - Fuzzer path: hpack_fuzzer
   - Seed: Dynamic table update with overflow

7. **huffman.c - Huffman code buffer overflow**
   - Location: `decode_huffman()` function
   - Type: Buffer overflow in Huffman decoding
   - Trigger: Huffman-encoded string with specific bit pattern
   - Fuzzer path: hpack_fuzzer
   - Seed: Huffman-encoded header value

**TLS Module (2 bugs):**
8. **tls_record.c - Record length validation missing**
   - Location: `parse_tls_record()` function
   - Type: Missing bounds check on record length
   - Trigger: TLS record with oversized length
   - Fuzzer path: tls_fuzzer
   - Seed: TLS record with max length

9. **tls_crypto.c - MAC calculation buffer overflow**
   - Location: `calculate_mac()` function
   - Type: Buffer overflow in MAC computation
   - Trigger: Record with specific length pattern
   - Fuzzer path: tls_fuzzer
   - Seed: TLS record with edge case length

**HTTP Module (2 bugs):**
10. **http_parser.c - Header name length overflow**
    - Location: `parse_header()` function
    - Type: Integer overflow in header length
    - Trigger: Header with oversized name
    - Fuzzer path: http_fuzzer
    - Seed: HTTP header with large name

11. **http_request.c - Request URI buffer overflow**
    - Location: `parse_request_uri()` function
    - Type: Buffer overflow in URI parsing
    - Trigger: Request with oversized URI
    - Fuzzer path: http_fuzzer
    - Seed: HTTP request with long URI

**Utils Module (1 bug):**
12. **base64.c - Base64 decode buffer overflow**
    - Location: `base64_decode()` function
    - Type: Buffer overflow in base64 decoding
    - Trigger: Base64 string with specific padding
    - Fuzzer path: http_fuzzer
    - Seed: Base64-encoded header value

### LEVEL 2 BUGS (13 bugs - 43%)
**Characteristics:** Cross-module interactions, 2-4 fix locations, 30-60 minutes to solve

**Protocol Module (4 bugs):**
13. **h2_frame.c + h2_connection.c - Frame stream ID race condition**
    - Location: `process_frame()` and connection state
    - Type: Race condition in stream ID validation
    - Trigger: Concurrent frames with same stream ID
    - Fuzzer path: h2_frame_fuzzer
    - Seed: Multiple frames with overlapping stream IDs
    - Distractor: Redundant lock in unrelated function

14. **h2_stream.c + h2_connection.c - Stream state machine auto-transition**
    - Location: `transition_stream_state()` function
    - Type: Auto-transition on invalid event
    - Trigger: Invalid state transition sequence
    - Fuzzer path: stream_fuzzer
    - Seed: Stream state sequence with invalid transition
    - Distractor: Harmless state logging function

15. **h2_connection.c - Connection flow control window overflow**
    - Location: `update_flow_window()` function
    - Type: Integer overflow in window calculation
    - Trigger: WINDOW_UPDATE with large increment
    - Fuzzer path: h2_frame_fuzzer
    - Seed: WINDOW_UPDATE frame with max increment
    - Distractor: Unused window statistics tracking

16. **h2_settings.c + h2_stream.c - Settings propagation bug**
    - Location: `propagate_settings()` function
    - Type: Settings not propagated to existing streams
    - Trigger: Settings change after stream creation
    - Fuzzer path: stream_fuzzer
    - Seed: SETTINGS frame followed by stream operation
    - Distractor: Redundant settings validation

**HPACK Module (3 bugs):**
17. **hpack_decoder.c + hpack_table.c - Dynamic table eviction bug**
    - Location: `evict_from_table()` function
    - Type: Use-after-free in table eviction
    - Trigger: Table eviction during header decoding
    - Fuzzer path: hpack_fuzzer
    - Seed: Header block causing table eviction
    - Distractor: Harmless table size logging

18. **hpack_encoder.c + hpack_table.c - Reference counting bug**
    - Location: `encode_header()` and table reference
    - Type: Reference count mismatch
    - Trigger: Header encoding with table references
    - Fuzzer path: hpack_fuzzer
    - Seed: Header block with multiple references
    - Distractor: Unused reference tracking

19. **hpack_table.c + hpack_decoder.c - Table size validation ordering**
    - Location: Size validation before header decoding
    - Type: Validation bypass through check ordering
    - Trigger: Size change mid-decoding
    - Fuzzer path: hpack_fuzzer
    - Seed: Header block with size change
    - Distractor: Redundant size check

**TLS Module (2 bugs):**
20. **tls_handshake.c + tls_record.c - Handshake state machine bug**
    - Location: `process_handshake()` function
    - Type: State machine auto-transition on invalid message
    - Trigger: Invalid handshake message sequence
    - Fuzzer path: tls_fuzzer
    - Seed: Handshake messages out of order
    - Distractor: Harmless handshake logging

21. **tls_crypto.c + tls_record.c - Cipher suite type confusion**
    - Location: `select_cipher_suite()` function
    - Type: Type confusion in cipher selection
    - Trigger: Cipher suite with invalid parameters
    - Fuzzer path: tls_fuzzer
    - Seed: ClientHello with invalid cipher
    - Distractor: Unused cipher preference tracking

**HTTP Module (2 bugs):**
22. **http_parser.c + http_request.c - Header validation bypass**
    - Location: Header validation ordering
    - Type: Validation bypass through check ordering
    - Trigger: Headers with specific ordering
    - Fuzzer path: http_fuzzer
    - Seed: HTTP headers with bypass pattern
    - Distractor: Redundant header normalization

23. **http_response.c + http_parser.c - Response chunking bug**
    - Location: `encode_chunked()` function
    - Type: Integer overflow in chunk size
    - Trigger: Response with large chunk size
    - Fuzzer path: http_fuzzer
    - Seed: Chunked response with overflow
    - Distractor: Unused chunk statistics

**Network Module (2 bugs):**
24. **epoll.c + socket.c - Event callback UAF**
    - Location: Event handler callback
    - Type: Use-after-free in callback
    - Trigger: Socket closure during event processing
    - Fuzzer path: stream_fuzzer
    - Seed: Connection close during stream operation
    - Distractor: Harmless event logging

25. **buffer.c + epoll.c - Ring buffer overflow**
    - Location: `buffer_write()` function
    - Type: Buffer overflow in ring buffer
    - Trigger: Write beyond buffer capacity
    - Fuzzer path: h2_frame_fuzzer
    - Seed: Frame data exceeding buffer size
    - Distractor: Unused buffer statistics

### LEVEL 3 BUGS (5 bugs - 17%)
**Characteristics:** Complex state machines, 4-6 fix locations, 60-120 minutes to solve

**Protocol Module (2 bugs):**
26. **h2_stream.c + h2_connection.c + h2_frame.c - Complex stream lifecycle bug**
    - Location: Stream lifecycle management across multiple functions
    - Type: Multi-site use-after-free in stream cleanup
    - Trigger: Stream closure during frame processing
    - Fuzzer path: stream_fuzzer
    - Seed: Complex stream lifecycle sequence
    - Distractor: Multiple harmless stream tracking functions
    - Fix locations: 4-5 sites across stream/connection/frame

27. **h2_connection.c + h2_settings.c + h2_stream.c - Settings cascade bug**
    - Location: Settings propagation through connection hierarchy
    - Type: Silent invariant violation in settings cascade
    - Trigger: Settings change affecting multiple streams
    - Fuzzer path: stream_fuzzer
    - Seed: Settings change with active streams
    - Distractor: Redundant settings validation functions
    - Fix locations: 4-6 sites across modules

**HPACK Module (1 bug):**
28. **hpack_decoder.c + hpack_table.c + huffman.c - HPACK state machine bug**
    - Location: HPACK decoding state machine
    - Type: Complex state transition with auto-transition
    - Trigger: Invalid HPACK encoding sequence
    - Fuzzer path: hpack_fuzzer
    - Seed: Malformed HPACK header block
    - Distractor: Harmless HPACK statistics tracking
    - Fix locations: 4-5 sites in decoder/table/huffman

**TLS Module (1 bug):**
29. **tls_handshake.c + tls_record.c + tls_crypto.c - TLS renegotiation bug**
    - Location: TLS renegotiation state machine
    - Type: State corruption during renegotiation
    - Trigger: Renegotiation during active session
    - Fuzzer path: tls_fuzzer
    - Seed: Renegotiation request mid-session
    - Distractor: Unused session tracking
    - Fix locations: 5-6 sites across handshake/record/crypto

**HTTP Module (1 bug):**
30. **http_parser.c + http_request.c + http_response.c - HTTP/1.1 upgrade bug**
    - Location: HTTP/1.1 to HTTP/2 upgrade logic
    - Type: Protocol state corruption during upgrade
    - Trigger: Upgrade request with specific headers
    - Fuzzer path: http_fuzzer
    - Seed: HTTP/1.1 upgrade request
    - Distractor: Redundant upgrade validation
    - Fix locations: 4-5 sites across parser/request/response

## DETAILED IMPLEMENTATION STEPS

### PHASE 1: PROJECT SETUP (Day 1)

#### Step 1.1: Create Directory Structure
```bash
cd "c:/Users/NEHITH/Documents/fenrir project 4"
mkdir chronos
cd chronos
mkdir src
mkdir src/protocol
mkdir src/hpack
mkdir src/tls
mkdir src/http
mkdir src/net
mkdir src/utils
mkdir fuzz
mkdir fuzz/corpus
mkdir fuzz/corpus/h2_frame_fuzzer
mkdir fuzz/corpus/hpack_fuzzer
mkdir fuzz/corpus/tls_fuzzer
mkdir fuzz/corpus/http_fuzzer
mkdir fuzz/corpus/stream_fuzzer
mkdir .clusterfuzzlite
```

#### Step 1.2: Create Basic Build Files

**CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.10)
project(chronos C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2 -g")

# Source files
set(PROTOCOL_SOURCES
    src/protocol/h2_frame.c
    src/protocol/h2_stream.c
    src/protocol/h2_connection.c
    src/protocol/h2_settings.c
    src/protocol/h2_push.c
)

set(HPACK_SOURCES
    src/hpack/hpack_decoder.c
    src/hpack/hpack_encoder.c
    src/hpack/hpack_table.c
    src/hpack/huffman.c
)

set(TLS_SOURCES
    src/tls/tls_record.c
    src/tls/tls_handshake.c
    src/tls/tls_crypto.c
)

set(HTTP_SOURCES
    src/http/http_parser.c
    src/http/http_request.c
    src/http/http_response.c
)

set(NET_SOURCES
    src/net/socket.c
    src/net/epoll.c
    src/net/buffer.c
)

set(UTILS_SOURCES
    src/utils/checksum.c
    src/utils/base64.c
    src/utils/memory.c
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
add_executable(h2_frame_fuzzer fuzz/h2_frame_fuzzer.c)
target_link_libraries(h2_frame_fuzzer chronos ${LIB_FUZZING_ENGINE})

add_executable(hpack_fuzzer fuzz/hpack_fuzzer.c)
target_link_libraries(hpack_fuzzer chronos ${LIB_FUZZING_ENGINE})

add_executable(tls_fuzzer fuzz/tls_fuzzer.c)
target_link_libraries(tls_fuzzer chronos ${LIB_FUZZING_ENGINE})

add_executable(http_fuzzer fuzz/http_fuzzer.c)
target_link_libraries(http_fuzzer chronos ${LIB_FUZZING_ENGINE})

add_executable(stream_fuzzer fuzz/stream_fuzzer.c)
target_link_libraries(stream_fuzzer chronos ${LIB_FUZZING_ENGINE})
```

**.clusterfuzzlite/build.sh:**
```bash
#!/bin/bash -eu
set -e

# Build the project
cd /src
mkdir -p build
cd build

cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -- -j$(nproc)

# Copy fuzz targets to $OUT
cp h2_frame_fuzzer $OUT/
cp hpack_fuzzer $OUT/
cp tls_fuzzer $OUT/
cp http_fuzzer $OUT/
cp stream_fuzzer $OUT/
```

**.clusterfuzzlite/project.yaml:**
```yaml
fuzz_targets:
  - h2_frame_fuzzer
  - hpack_fuzzer
  - tls_fuzzer
  - http_fuzzer
  - stream_fuzzer
```

**.gitignore:**
```
build/
*.o
*.a
*.so
*.dSYM/
.DS_Store
.vscode/
.idea/
*.swp
*~
```

**README.md:**
```markdown
# Chronos HTTP/2 Server

Chronos is a lightweight HTTP/2 server implementation written in C. It implements the HTTP/2 protocol including frame parsing, stream multiplexing, header compression (HPACK), and basic TLS support.

## Features

- HTTP/2 frame parsing and generation
- Stream multiplexing and state management
- HPACK header compression and decompression
- Basic TLS record layer support
- HTTP/1.1 to HTTP/2 upgrade support
- Connection and flow control management

## Building

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

## Testing

The project includes fuzzing harnesses for testing protocol parsing and state management.

## License

MIT License
```

#### Step 1.3: Create Common Header Files

**src/common.h:**
```c
#ifndef CHRONOS_COMMON_H
#define CHRONOS_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#define CHRONOS_MAX_FRAME_SIZE 16384
#define CHRONOS_MAX_HEADER_SIZE 8192
#define CHRONOS_MAX_STREAMS 1000
#define CHRONOS_DEFAULT_WINDOW_SIZE 65535

typedef enum {
    CHRONOS_OK = 0,
    CHRONOS_ERROR_INVALID_INPUT = -1,
    CHRONOS_ERROR_BUFFER_TOO_SMALL = -2,
    CHRONOS_ERROR_INVALID_STATE = -3,
    CHRONOS_ERROR_OUT_OF_MEMORY = -4,
} chronos_error_t;

#endif // CHRONOS_COMMON_H
```

### PHASE 2: UTILS MODULE IMPLEMENTATION (Day 1-2)

#### Step 2.1: Implement memory.c with Bug #12 (Level 1)
- Create custom allocator with use-after-free vulnerability
- Implement `chronos_malloc()`, `chronos_free()`, `chronos_realloc()`
- Bug: Missing null check in realloc when size is 0
- Place in high-traffic path (used by all modules)
- Add distractor: Harmless memory statistics tracking

#### Step 2.2: Implement checksum.c with bitwise operations
- Implement custom checksum with potential overflow
- No critical bugs here (support module)
- Used for frame validation

#### Step 2.3: Implement base64.c with Bug #12 (Level 1)
- Implement base64 encode/decode
- Bug: Buffer overflow in decode when input has specific padding
- Trigger: Base64 string with padding pattern
- Add distractor: Unused base64 variant tracking

### PHASE 3: PROTOCOL MODULE IMPLEMENTATION (Day 3-5)

#### Step 3.1: Implement h2_frame.c with Bugs #1, #13, #25
- Implement frame header parsing
- Bug #1: Integer overflow in length calculation (Level 1)
- Bug #13: Race condition with connection state (Level 2)
- Bug #25: Ring buffer overflow (Level 2)
- Add distractor: Redundant frame type validation
- Ensure all bugs are in high-traffic parsing paths

#### Step 3.2: Implement h2_stream.c with Bugs #2, #14, #15, #26
- Implement stream state machine
- Bug #2: Missing null check in lookup (Level 1)
- Bug #14: Auto-transition on invalid event (Level 2)
- Bug #15: Flow control window overflow (Level 2)
- Bug #26: Complex lifecycle UAF (Level 3)
- Add distractor: Harmless stream state logging
- Ensure state machine is complex enough for Level 3

#### Step 3.3: Implement h2_connection.c with Bugs #13, #14, #15, #26, #27
- Implement connection management
- Shared state for race conditions
- Integration with stream and frame modules
- Ensure cross-module bugs are reachable

#### Step 3.4: Implement h2_settings.c with Bugs #3, #16, #27
- Implement settings handling
- Bug #3: Settings validation bypass (Level 1)
- Bug #16: Settings propagation bug (Level 2)
- Bug #27: Settings cascade bug (Level 3)
- Add distractor: Redundant settings validation

#### Step 3.5: Implement h2_push.c with Bug #4
- Implement server push logic
- Bug #4: Push promise ID overflow (Level 1)
- Simple but necessary for completeness

### PHASE 4: HPACK MODULE IMPLEMENTATION (Day 6-8)

#### Step 4.1: Implement hpack_decoder.c with Bugs #5, #17, #19, #28
- Implement HPACK decoder
- Bug #5: Integer index overflow (Level 1)
- Bug #17: Dynamic table eviction UAF (Level 2)
- Bug #19: Table size validation ordering (Level 2)
- Bug #28: Complex state machine bug (Level 3)
- Add distractor: Harmless HPACK statistics

#### Step 4.2: Implement hpack_encoder.c with Bug #18
- Implement HPACK encoder
- Bug #18: Reference counting bug (Level 2)
- Integration with table module

#### Step 4.3: Implement hpack_table.c with Bugs #6, #17, #18, #19, #28
- Implement dynamic table management
- Bug #6: Table size calculation overflow (Level 1)
- Shared bugs with decoder for cross-module interaction
- Ensure table operations are high-traffic

#### Step 4.4: Implement huffman.c with Bug #7
- Implement Huffman decoding
- Bug #7: Huffman code buffer overflow (Level 1)
- Complex bitwise operations for difficulty

### PHASE 5: TLS MODULE IMPLEMENTATION (Day 9-10)

#### Step 5.1: Implement tls_record.c with Bugs #8, #20, #21, #29
- Implement TLS record layer
- Bug #8: Record length validation missing (Level 1)
- Bug #20: Handshake state machine bug (Level 2)
- Bug #21: Cipher suite type confusion (Level 2)
- Bug #29: Renegotiation bug (Level 3)
- Add distractor: Unused cipher preference tracking

#### Step 5.2: Implement tls_handshake.c with Bugs #20, #29
- Implement handshake state machine
- Complex state transitions for Level 3 bug
- Integration with record layer

#### Step 5.3: Implement tls_crypto.c with Bugs #9, #21, #29
- Implement cryptographic operations
- Bug #9: MAC calculation buffer overflow (Level 1)
- Simplified crypto (not full TLS for complexity control)

### PHASE 6: HTTP MODULE IMPLEMENTATION (Day 11-12)

#### Step 6.1: Implement http_parser.c with Bugs #10, #22, #30
- Implement HTTP/1.1 parser
- Bug #10: Header name length overflow (Level 1)
- Bug #22: Header validation bypass (Level 2)
- Bug #30: HTTP/1.1 upgrade bug (Level 3)
- Add distractor: Redundant header normalization

#### Step 6.2: Implement http_request.c with Bugs #11, #22, #30
- Implement request handling
- Bug #11: Request URI buffer overflow (Level 1)
- Integration with parser for upgrade bug

#### Step 6.3: Implement http_response.c with Bugs #23, #30
- Implement response generation
- Bug #23: Response chunking bug (Level 2)
- Integration with upgrade logic

### PHASE 7: NETWORK MODULE IMPLEMENTATION (Day 13)

#### Step 7.1: Implement socket.c
- Basic socket operations
- No critical bugs (support module)
- Used by event loop

#### Step 7.2: Implement epoll.c with Bug #24
- Implement event loop
- Bug #24: Event callback UAF (Level 2)
- Reentrancy through callback mechanism
- Add distractor: Harmless event logging

#### Step 7.3: Implement buffer.c with Bug #25
- Implement ring buffer
- Bug #25: Ring buffer overflow (Level 2)
- Used by frame processing

### PHASE 8: FUZZ HARNESS IMPLEMENTATION (Day 14)

#### Step 8.1: Create h2_frame_fuzzer.c
```c
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "src/protocol/h2_frame.h"
#include "src/protocol/h2_connection.h"

void LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 9) return;
    
    // Create connection
    h2_connection_t *conn = h2_connection_create();
    if (!conn) return;
    
    // Parse frame
    h2_frame_t frame;
    if (h2_parse_frame(data, size, &frame) == CHRONOS_OK) {
        h2_process_frame(conn, &frame);
    }
    
    h2_connection_destroy(conn);
}
```

#### Step 8.2: Create hpack_fuzzer.c
```c
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "src/hpack/hpack_decoder.h"
#include "src/hpack/hpack_table.h"

void LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 1) return;
    
    // Create HPACK context
    hpack_context_t *ctx = hpack_context_create(4096);
    if (!ctx) return;
    
    // Decode headers
    hpack_header_t headers[100];
    int count = hpack_decode_headers(ctx, data, size, headers, 100);
    
    hpack_context_destroy(ctx);
}
```

#### Step 8.3: Create tls_fuzzer.c
```c
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "src/tls/tls_record.h"
#include "src/tls/tls_handshake.h"

void LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 5) return;
    
    // Create TLS context
    tls_context_t *ctx = tls_context_create();
    if (!ctx) return;
    
    // Parse record
    tls_record_t record;
    if (tls_parse_record(data, size, &record) == CHRONOS_OK) {
        tls_process_record(ctx, &record);
    }
    
    tls_context_destroy(ctx);
}
```

#### Step 8.4: Create http_fuzzer.c
```c
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "src/http/http_parser.h"
#include "src/http/http_request.h"

void LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 1) return;
    
    // Parse HTTP request
    http_request_t *req = http_request_create();
    if (!req) return;
    
    if (http_parse_request(data, size, req) == CHRONOS_OK) {
        http_process_request(req);
    }
    
    http_request_destroy(req);
}
```

#### Step 8.5: Create stream_fuzzer.c
```c
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "src/protocol/h2_stream.h"
#include "src/protocol/h2_connection.h"
#include "src/protocol/h2_frame.h"

void LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 9) return;
    
    // Create connection
    h2_connection_t *conn = h2_connection_create();
    if (!conn) return;
    
    // Create stream
    h2_stream_t *stream = h2_stream_create(conn, 1);
    if (!stream) {
        h2_connection_destroy(conn);
        return;
    }
    
    // Process multiple frames for state transitions
    size_t offset = 0;
    while (offset + 9 <= size) {
        h2_frame_t frame;
        if (h2_parse_frame(data + offset, size - offset, &frame) == CHRONOS_OK) {
            h2_stream_process_frame(stream, &frame);
            offset += 9 + frame.length;
        } else {
            break;
        }
    }
    
    h2_stream_destroy(stream);
    h2_connection_destroy(conn);
}
```

**CRITICAL:** All fuzzers must call ALL relevant modules to ensure bug reachability. No magic byte routing.

### PHASE 9: SEED CORPUS GENERATION (Day 15)

#### Step 9.1: Generate h2_frame_fuzzer seeds (15 files)
- Valid DATA frames with various sizes
- Valid HEADERS frames with different header counts
- SETTINGS frames with different parameter values
- WINDOW_UPDATE frames with edge case increments
- PUSH_PROMISE frames with various stream IDs
- Malformed frames for each specific bug trigger
- Frame sequences for state-dependent bugs

**Seed generation script approach:**
```python
# Generate valid HTTP/2 frames
# For each bug, create specific seed that triggers it
# Ensure seeds are valid enough to pass initial parsing
# But malformed enough to reach vulnerable code
```

#### Step 9.2: Generate hpack_fuzzer seeds (15 files)
- Valid HPACK header blocks with various encodings
- Headers using static table
- Headers using dynamic table
- Huffman-encoded values
- Integer-encoded values
- Specific patterns for each HPACK bug

#### Step 9.3: Generate tls_fuzzer seeds (15 files)
- Valid TLS records with different content types
- Handshake messages in various states
- Records with edge case lengths
- Specific cipher suite combinations
- Renegotiation sequences

#### Step 9.4: Generate http_fuzzer seeds (15 files)
- Valid HTTP/1.1 requests
- Requests with various headers
- Upgrade requests
- Chunked responses
- Specific header patterns for bypass bugs

#### Step 9.5: Generate stream_fuzzer seeds (15 files)
- Valid frame sequences for stream lifecycle
- Stream state transition sequences
- Concurrent stream operations
- Settings changes during stream operations
- Complex multi-stream scenarios

**CRITICAL:** Each bug must have at least 2-3 specific seed files that trigger it. Seeds should be valid enough to pass initial validation but reach the vulnerable code path.

### PHASE 10: BUILD VERIFICATION (Day 16)

#### Step 10.1: Test hermetic build
```bash
# Test build with networking disabled
docker build --network=none -t chronos-test .
# Or use offline build environment
```

#### Step 10.2: Verify all fuzzers build
```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
# Verify all 5 fuzzers are created
ls -la h2_frame_fuzzer hpack_fuzzer tls_fuzzer http_fuzzer stream_fuzzer
```

#### Step 10.3: Verify no network dependencies
- Check CMakeLists.txt for any fetch operations
- Verify all dependencies are vendored or standard library
- No external downloads during build

#### Step 10.4: Verify sanitizer compatibility
- Build with AddressSanitizer
- Build with UndefinedBehaviorSanitizer
- Ensure no sanitizer false positives in normal operation

### PHASE 11: LOCAL FUZZING VERIFICATION (Day 17-18)

#### Step 11.1: Run each fuzzer for extended period
```bash
# Run each fuzzer for at least 1 hour
./h2_frame_fuzzer -max_total_time=3600 corpus/h2_frame_fuzzer/
./hpack_fuzzer -max_total_time=3600 corpus/hpack_fuzzer/
./tls_fuzzer -max_total_time=3600 corpus/tls_fuzzer/
./http_fuzzer -max_total_time=3600 corpus/http_fuzzer/
./stream_fuzzer -max_total_time=3600 corpus/stream_fuzzer/
```

#### Step 11.2: Verify all 30 bugs are triggered
- Check crash outputs for each bug
- Map crashes to specific bug IDs
- Ensure no bug is missed
- If a bug is not triggered, adjust seed corpus or bug placement

#### Step 11.3: Document crash triggers
- For each bug, record the minimal input that triggers it
- Save these as reference for later patch development
- Ensure crashes are deterministic

#### Step 11.4: Verify crash quality
- All crashes must be memory-safety violations
- No crashes in fuzzer harness only
- All crashes must reach project code
- No symptom suppression (catch-all exceptions)

### PHASE 12: CODE QUALITY VERIFICATION (Day 19)

#### Step 12.1: Verify no bug-naming comments
```bash
grep -r "BUG\|TODO\|FIXME\|CVE\|vulnerability" src/
# Should return nothing
```

#### Step 12.2: Verify no knob bugs
- Check for adjustable thresholds
- Check for configurable parameters
- Ensure all bugs are structural

#### Step 12.3: Verify no magic byte routing
- Check fuzzers for conditional module selection
- Ensure all modules are called unconditionally
- No input-based routing

#### Step 12.4: Verify LOC count
```bash
# Count lines of code
find src -name "*.c" -o -name "*.h" | xargs wc -l
# Should be 20,000+ lines
```

#### Step 12.5: Verify AI-written-content check
- Ensure code is genuinely written
- No generated filler or duplicate logic
- Professional code style

### PHASE 13: FINAL PRE-SUBMISSION CHECKLIST (Day 20)

#### Step 13.1: Repository structure verification
- [ ] All required directories present
- [ ] All source files present
- [ ] All fuzzers present
- [ ] Seed corpus complete (75+ files)
- [ ] .clusterfuzzlite/build.sh present
- [ ] CMakeLists.txt present
- [ ] README.md present
- [ ] .gitignore present

#### Step 13.2: Build verification
- [ ] Hermetic build succeeds
- [ ] No network dependencies
- [ ] All fuzzers build to $OUT
- [ ] Build is deterministic

#### Step 13.3: Bug verification
- [ ] All 30 bugs present in code
- [ ] All bugs triggered by local fuzzing
- [ ] All bugs are memory-safety violations
- [ ] All bugs are structural (no knobs)
- [ ] All bugs have multiple unsafe paths
- [ ] Distractor code present near vulnerabilities

#### Step 13.4: Fuzzer verification
- [ ] 5 fuzzers present
- [ ] No magic byte routing
- [ ] All modules covered
- [ ] Seed corpus targets each bug
- [ ] Fuzzers run without crashes on valid input

#### Step 13.5: Quality verification
- [ ] No bug-naming comments
- [ ] Professional code style
- [ ] 20,000+ LOC
- [ ] Original work (not copied)
- [ ] Passes AI-content check

### PHASE 14: GITHUB REPOSITORY CREATION (Day 21)

#### Step 14.1: Create private GitHub repository
- Go to GitHub
- Create new private repository
- Name: "chronos-http2-server"
- Description: "Lightweight HTTP/2 server implementation in C"
- Initialize with README
- DO NOT add .gitignore (use custom one)
- DO NOT add license (will add later)

#### Step 14.2: Push code to repository
```bash
cd chronos
git init
git add .
git commit -m "Initial commit: Chronos HTTP/2 server implementation"
git remote add origin https://github.com/YOUR_USERNAME/chronos-http2-server.git
git branch -M main
git push -u origin main
```

#### Step 14.3: Verify repository is private
- Check GitHub repository settings
- Ensure repository is private
- Ensure it's not a fork
- Verify commit history is original

#### Step 14.4: Final verification
- Clone repository to fresh location
- Verify build succeeds
- Verify fuzzers run
- Verify bugs are still present

### PHASE 15: AFTERQUERY SUBMISSION (Day 22)

#### Step 15.1: Connect GitHub to AfterQuery
- Log in to AfterQuery platform
- Navigate to Project Fenrir
- Connect GitHub account
- Select chronos-http2-server repository
- Verify repository is detected as private

#### Step 15.2: Submit repository
- Paste repository URL
- Select branch: main
- Review submission details
- Submit for evaluation

#### Step 15.3: Monitor submission status
- Wait for eligibility checks
- Wait for fuzzing to start
- Monitor crash detection
- Wait for difficulty probe results

#### Step 15.4: Prepare for patch development
- Once bugs are detected, document each crash
- Prepare crash descriptions (1-3 sentences, no hints)
- Wait for approval to start patching

## BUG IMPLEMENTATION DETAILS

### BUG IMPLEMENTATION PRINCIPLES

1. **All bugs must be in high-traffic code paths**
   - Place bugs in functions called by fuzzers
   - Ensure bugs are not behind rare conditions
   - Use conditional gates but keep them reachable

2. **Each bug must have 2-3 unsafe paths**
   - Multiple ways to trigger the same bug
   - Different input sequences leading to same vulnerability
   - Ensures fuzzer will find it

3. **Add distractor code near each bug**
   - Harmless but suspicious-looking code
   - Redundant validations, unused tracking
   - Makes bug less obvious to AI analysis

4. **Use 3-5 conditional gates per bug**
   - Type checks, value ranges, state flags
   - Execution counts, accumulated state
   - Makes bug harder to trigger but still reachable

5. **Ensure temporal dimension where possible**
   - Bugs that manifest on Nth execution
   - State accumulation required
   - Pointer lifetime across calls

6. **Cross-module interaction for Level 2-3 bugs**
   - Shared state between modules
   - Callback mechanisms
   - Observer patterns

### SPECIFIC BUG IMPLEMENTATION PATTERNS

#### Pattern 1: Integer Overflow in Size Calculation
```c
// Vulnerable code
size_t calculate_total_size(size_t count, size_t item_size) {
    // Bug: No overflow check
    return count * item_size;
}

// Distractor code nearby
void log_size_calculation(size_t count, size_t item_size) {
    // Harmless logging, looks suspicious
    printf("Calculating size: %zu * %zu\n", count, item_size);
}
```

#### Pattern 2: Use-After-Free with Callback
```c
// Vulnerable code
typedef void (*callback_t)(void*);

void register_callback(callback_t cb, void* data) {
    global_callback = cb;
    global_data = data;
}

void trigger_callback() {
    // Bug: global_data might be freed
    global_callback(global_data);
}

// Distractor code
void validate_callback(callback_t cb) {
    // Harmless validation
    if (cb == NULL) {
        printf("Warning: NULL callback\n");
    }
}
```

#### Pattern 3: State Machine Auto-Transition
```c
// Vulnerable code
typedef enum {
    STATE_IDLE,
    STATE_ACTIVE,
    STATE_ERROR
} state_t;

state_t transition(state_t current, event_t event) {
    state_t next = lookup_transition(current, event);
    
    if (next == NULL) {
        // Bug: Auto-transition to ERROR instead of rejecting
        next = STATE_ERROR;
    }
    
    return next;
}

// Distractor code
void log_transition(state_t from, state_t to) {
    // Harmless logging
    printf("Transition: %d -> %d\n", from, to);
}
```

#### Pattern 4: Validation Ordering Bypass
```c
// Vulnerable code
bool process_message(message_t* msg) {
    // Bug: Size check before checksum allows bypass
    if (msg->size > MAX_SIZE) return false;
    if (!validate_checksum(msg)) return false;
    if (msg->type == INVALID_TYPE) return false;
    
    return true;
}

// Distractor code
void log_message(message_t* msg) {
    // Harmless logging
    printf("Processing message type %d\n", msg->type);
}
```

#### Pattern 5: Type Erasure with void*
```c
// Vulnerable code
typedef struct {
    void* data;
    size_t size;
    int type;
} variant_t;

void process_variant(variant_t* v) {
    // Bug: Assumes type is correct without verification
    if (v->type == TYPE_STRING) {
        char* str = (char*)v->data;
        printf("%s\n", str);  // UAF if type is wrong
    }
}

// Distractor code
void validate_variant(variant_t* v) {
    // Harmless validation
    if (v->data == NULL) {
        printf("Warning: NULL data\n");
    }
}
```

## SEED CORPUS STRATEGY

### SEED GENERATION GUIDELINES

1. **Each bug needs 2-3 specific seeds**
   - Direct trigger seed
   - Edge case trigger seed
   - Complex sequence trigger seed

2. **Seeds must pass initial validation**
   - Valid enough to reach parsing logic
   - Not so malformed they're rejected immediately
   - Structured according to protocol

3. **Seeds should be minimal**
   - Smallest input that triggers bug
   - Reduces fuzzer exploration time
   - Easier to debug

4. **Seeds should be diverse**
   - Different input patterns for same bug
   - Ensures bug is reachable through multiple paths
   - Covers edge cases

### SEED FILE NAMING CONVENTION

```
corpus/<fuzzer>/<bug_id>_<description>_<variant>.bin
```

Example:
```
corpus/h2_frame_fuzzer/bug1_frame_length_overflow_direct.bin
corpus/h2_frame_fuzzer/bug1_frame_length_overflow_edge.bin
corpus/h2_frame_fuzzer/bug1_frame_length_overflow_complex.bin
```

### SEED GENERATION APPROACH

For each bug:
1. Identify the exact trigger condition
2. Create minimal input that satisfies all conditional gates
3. Test locally to confirm crash
4. Create variants with different edge cases
5. Add to corpus

## TESTING AND VALIDATION PROCEDURES

### LOCAL TESTING CHECKLIST

Before submission, perform these tests:

#### Test 1: Build Verification
```bash
# Clean build
rm -rf build
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
# Should complete without errors
```

#### Test 2: Fuzzer Execution
```bash
# Run each fuzzer for 60 seconds
./h2_frame_fuzzer -max_total_time=60 corpus/h2_frame_fuzzer/
./hpack_fuzzer -max_total_time=60 corpus/hpack_fuzzer/
./tls_fuzzer -max_total_time=60 corpus/tls_fuzzer/
./http_fuzzer -max_total_time=60 corpus/http_fuzzer/
./stream_fuzzer -max_total_time=60 corpus/stream_fuzzer/
# Should run without immediate crashes on valid input
```

#### Test 3: Bug Trigger Verification
```bash
# Run each fuzzer with specific seed for each bug
./h2_frame_fuzzer corpus/h2_frame_fuzzer/bug1_*.bin
# Should crash for each bug-specific seed
```

#### Test 4: Sanitizer Verification
```bash
# Build with sanitizers
cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_SANITIZERS=ON
cmake --build .
# Run fuzzers
# Should detect memory safety violations
```

#### Test 5: Hermetic Build Verification
```bash
# Test build without network
docker build --network=none -t chronos-test .
# Should complete without network access
```

## CRASH DESCRIPTION GUIDELINES

When bugs are detected and need descriptions:

### Format Requirements
- 1-3 sentences maximum
- ~80 words maximum
- Describe bug class, file/function, observable symptom
- NO trigger mechanism
- NO remediation hints
- NO patch strategy

### Example Good Description
"An out-of-bounds read occurs in h2_parse_frame when processing a DATA frame with a malformed length field."

### Example Bad Description
"An out-of-bounds read occurs in h2_parse_frame when the length field is greater than 16384 bytes. Add a bounds check before accessing the frame data."

### Description Template
"[Bug class] occurs in [file/function] when [observable symptom]."

## POST-SUBMISSION PROCESS

### AFTER BUG DETECTION

1. **Document each crash**
   - Save crash output
   - Identify which bug it corresponds to
   - Verify crash is deterministic

2. **Write crash descriptions**
   - Follow format guidelines
   - No hints or remediation
   - Keep descriptions concise

3. **Develop patches**
   - Fix root cause, not symptom
   - Preserve intended behavior
   - Keep tests passing
   - Leave harness operational

4. **Submit patches**
   - One patch per bug
   - Wait for approval
   - Address any revision requests

### MONITORING TASK STATUS

- Check status: In Review, Quality Flagged, Needs Bundle, Too Easy, Never Solved
- If "Too Easy": Bug was too simple, need harder bugs next time
- If "Never Solved": Bug was unreachable, need better reachability
- If "In Review": Wait for human reviewer decision

## TROUBLESHOOTING GUIDE

### IF A BUG IS NOT TRIGGERED BY FUZZING

1. **Check bug placement**
   - Is bug in high-traffic code path?
   - Are conditional gates too restrictive?
   - Is bug behind rare validation?

2. **Check seed corpus**
   - Are seeds targeting the specific bug?
   - Are seeds valid enough to pass initial parsing?
   - Are seeds minimal and direct?

3. **Check fuzzer coverage**
   - Does fuzzer call the module with the bug?
   - Is there magic byte routing preventing reach?
   - Are all modules called unconditionally?

4. **Adjust bug if needed**
   - Move bug to higher-traffic path
   - Relax conditional gates slightly
   - Add more specific seed files

### IF BUILD FAILS

1. **Check dependencies**
   - Are all source files present?
   - Are all header files present?
   - Is CMakeLists.txt correct?

2. **Check compilation errors**
   - Fix syntax errors
   - Fix missing includes
   - Fix type mismatches

3. **Check linker errors**
   - Are all object files linked?
   - Are library dependencies correct?
   - Are circular dependencies resolved?

### IF REPOSITORY IS REJECTED

1. **Check rejection reason**
   - Public repository? → Make private
   - Copied from public? → Ensure original work
   - Network dependency? → Make build hermetic
   - Missing harness? → Add fuzz harness
   - Too easy? → Add complexity to bugs
   - Never solved? → Improve bug reachability

2. **Address the issue**
   - Fix the specific problem
   - Re-test locally
   - Resubmit if possible

## SUCCESS METRICS

### BEFORE SUBMISSION
- [ ] 20,000+ lines of code
- [ ] 30 structural bugs implemented
- [ ] All 30 bugs triggered by local fuzzing
- [ ] 5 fuzzers with comprehensive coverage
- [ ] 75+ seed corpus files
- [ ] Hermetic build verified
- [ ] No bug-naming comments
- [ ] Professional code quality

### AFTER SUBMISSION
- [ ] Repository accepted by platform
- [ ] Fuzzing finds crashes
- [ ] Crashes correspond to implemented bugs
- [ ] Difficulty probe: 1-3/5 solves
- [ ] Tasks approved and in-band
- [ ] Patches accepted
- [ ] Repository bonus achieved (5+ tasks)

## CONCLUSION

This implementation plan provides a complete roadmap from initial setup to AfterQuery submission. Follow each phase carefully, verify each step, and ensure all 30 bugs are guaranteed to be triggered by fuzzing. The repository will be locked after submission, so thorough local testing is critical.

Remember the three critical constraints:
1. Repository locked after submission
2. All 30 bugs MUST be triggered by fuzzing
3. Target 1-3/5 solve rate (not 0, not 4-5)

Good luck with the Chronos HTTP/2 server implementation!
