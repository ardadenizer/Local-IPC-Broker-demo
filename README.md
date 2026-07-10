### Project Description:
A simple Local (IPC Based) broker implementation that is intended to be deployed strictly on Linux.

### Processes:
- broker: routes messages
- capture: publishes motion events
- analytics: consumes motion, publishes alerts
- uploader: consumes alert and simulates cloud upload


## Build and Run
### Requirements:
- Linux ( I use WSL Ubuntu image)
- CMake 3.16+
- C++ compiler with C++20 support (GCC or Clang)

### Build:
From the project root:

```cmake -S . -B build```

```cmake --build build -j```

This produces binaries in:
- build/bin/broker
- build/bin/capture
- build/bin/analytics

### Run:
To run all services with one script:

```chmod +x init.sh```

```./init.sh```

## IPC Mechanism: Unix Domain Socket(UDS)



### Message format: JSON
