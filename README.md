# Project Description:
A simple Local (IPC Based) broker implementation that is intended to be deployed strictly on Linux for resource constrained embedded devices.

## Processes:
- broker: routes messages
- capture: publishes motion events
- analytics: consumes motion, publishes alerts
- uploader: consumes alert and simulates cloud upload

## Message Format

Communication between processes is performed using newline-delimited JSON messages.

The full protocol specification is documented in:
- [docs/message-format.md](docs/message-format.md)

Quick schema reminder:

```json
{
    "version": 1,
    "type": "publish|deliver|subscribe|ack|error",
    "message_id": "msg-001",
    "timestamp": 1783787005,
    "topic": "motion.events",
    "qos": 0,
    "client_id": "capture",
    "payload": {}
}
```

## Topics

| Topic | Producer | Consumer |
|--------|----------|----------|
| motion.events | Capture | Analytics |
| analytics.alerts | Analytics | Uploader |

## Quality of Service

Two QoS levels are implemented to distinguish importance and persistence of event messages.

### QoS 0

- Lightweight
- Best-effort delivery
- Memory-only queue
- If queue is full, broker drops oldest queued message for that consumer
- Suitable for high-frequency motion events

### QoS 1

- At-least-once delivery
- Persisted to disk until acknowledged
- Pending entries are stored in `/tmp/ipc_broker_qos1_pending.jsonl`
- ACK correlation key is `topic|message_id`
- Suitable for important alert messages


## System Architecture:
Relevant design documents can be found in the  `docs/` directory

![System architecture](docs/system-architecture-pic.png)

## High Level Sequence Diagram:
``` Capture -> Broker -> Analytics -> Broker -> Uploader -> Cloud```


# Build and Run
### Requirements:
- Linux ( I use WSL Ubuntu image)
- CMake 3.16+
- C++ compiler with C++20 support (GCC or Clang)
- Go 

### Build:
From the project root:

```cmake -S . -B build```

```cmake --build build -j```

This produces binaries in:
- build/bin/broker
- build/bin/capture
- build/bin/analytics

### Demo:
To build and run all services with one script for the demo, use:

```chmod +x scripts/init.sh```

```./scripts/init.sh```

Stop the services:
``` CRTL + C ```