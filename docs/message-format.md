# Message Protocol Format

This document defines the JSON wire format used by all local IPC services.

## Transport

- Transport: Unix domain socket stream
- Framing: one JSON object per line (`\n` delimited)
- Encoding: UTF-8 JSON
- Protocol version: `1`

## Common Schema

Every message uses the same top-level schema.

```json
{
  "version": 1,
  "type": "publish",
  "message_id": "msg-001",
  "timestamp": 1783787005,
  "topic": "motion.events",
  "qos": 0,
  "client_id": "capture",
  "payload": {}
}
```

## Fields

| Field | Type | Required | Notes |
|---|---|---|---|
| `version` | integer | yes | Must be `1` |
| `type` | string | yes | One of `subscribe`, `publish`, `deliver`, `ack`, `error` |
| `message_id` | string | conditional | Required for `publish`, `deliver`, `ack` |
| `timestamp` | integer | conditional | Unix seconds, required for `publish`, `deliver` |
| `topic` | string | conditional | Required for `subscribe`, `publish`, `deliver`; optional for `ack` |
| `qos` | integer | optional | `0` (QoS 0) or `1` (QoS 1), defaults to `0` |
| `client_id` | string | yes | Sender identity |
| `payload` | object | optional | Application-specific body, defaults to `{}` |

## Message Types

### 1) Subscribe

Used by consumers to register topic interest.

```json
{
  "version": 1,
  "type": "subscribe",
  "topic": "motion.events",
  "client_id": "analytics",
  "payload": {}
}
```

### 2) Publish

Used by producers to publish a topic event.

```json
{
  "version": 1,
  "type": "publish",
  "message_id": "motion-42",
  "timestamp": 1783787005,
  "topic": "motion.events",
  "qos": 0,
  "client_id": "capture",
  "payload": {
    "event": "motion_detected",
    "sequence": 42
  }
}
```

### 3) Deliver

Broker-routed message to subscribers. Same payload and metadata as publish, with `type=deliver`.

```json
{
  "version": 1,
  "type": "deliver",
  "message_id": "analytics-8",
  "timestamp": 1783787007,
  "topic": "analytics.alerts",
  "qos": 1,
  "client_id": "analytics",
  "payload": {
    "decision": "alert",
    "event": "critical_message",
    "source_client": "capture",
    "source_message_id": "motion-15"
  }
}
```

### 4) Ack

Used by a consumer to acknowledge a received QoS 1 message.

```json
{
  "version": 1,
  "type": "ack",
  "message_id": "analytics-8",
  "topic": "analytics.alerts",
  "client_id": "uploader-go"
}
```

Broker correlation key for QoS 1 persistence: `topic|message_id`.

### 5) Error

Reserved for explicit protocol/application error reporting.

```json
{
  "version": 1,
  "type": "error",
  "client_id": "broker",
  "payload": {
    "code": "invalid_message",
    "detail": "message_id is required"
  }
}
```

## Topics In This Demo

| Topic | Producer | Consumer |
|---|---|---|
| `motion.events` | capture | analytics |
| `analytics.alerts` | analytics | uploader-go |

## QoS Behavior

### QoS 0 (`qos=0`)

- Best-effort delivery
- Queue overflow policy: drop oldest queued message for that consumer

### QoS 1 (`qos=1`)

- Stored on disk until ACK arrives
- Pending store file: `/tmp/ipc_broker_qos1_pending.jsonl`
- Each line is the original delivered message JSON
- Deletion occurs only after matching ACK by `topic|message_id`
