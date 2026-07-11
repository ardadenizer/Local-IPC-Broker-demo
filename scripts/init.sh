#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
BIN_DIR="${BUILD_DIR}/bin"
LOG_DIR="${ROOT_DIR}/logs"
SOCKET_PATH="/tmp/ipc_broker.sock"

mkdir -p "${LOG_DIR}"

cleanup() {
  echo "[init] stopping services..."
  [[ -n "${CAPTURE_PID:-}" ]] && kill "${CAPTURE_PID}" 2>/dev/null || true
  [[ -n "${ANALYTICS_PID:-}" ]] && kill "${ANALYTICS_PID}" 2>/dev/null || true
  [[ -n "${BROKER_PID:-}" ]] && kill "${BROKER_PID}" 2>/dev/null || true
  wait 2>/dev/null || true
}

trap cleanup EXIT INT TERM

echo "[init] configuring/building..."
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}" -j

if [[ ! -x "${BIN_DIR}/broker" || ! -x "${BIN_DIR}/analytics" || ! -x "${BIN_DIR}/capture" ]]; then
  echo "[init] expected binaries are missing in ${BIN_DIR}"
  exit 1
fi

echo "[init] starting broker..."
"${BIN_DIR}/broker" > "${LOG_DIR}/broker.log" 2>&1 &
BROKER_PID=$!

echo "[init] waiting for broker socket..."
for _ in {1..50}; do
  if [[ -S "${SOCKET_PATH}" ]]; then
    break
  fi
  sleep 0.1
done

if [[ ! -S "${SOCKET_PATH}" ]]; then
  echo "[init] broker did not become ready in time"
  exit 1
fi

echo "[init] starting analytics..."
"${BIN_DIR}/analytics" > "${LOG_DIR}/analytics.log" 2>&1 &
ANALYTICS_PID=$!

echo "[init] starting capture..."
"${BIN_DIR}/capture" > "${LOG_DIR}/capture.log" 2>&1 &
CAPTURE_PID=$!

echo "[init] services started:"
echo "  broker pid=${BROKER_PID}"
echo "  analytics pid=${ANALYTICS_PID}"
echo "  capture pid=${CAPTURE_PID}"
echo "[init] logs at ${LOG_DIR}"

wait "${CAPTURE_PID}" "${ANALYTICS_PID}"