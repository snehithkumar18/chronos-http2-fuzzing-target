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
