#!/usr/bin/env bash
set -eo pipefail

# Configurable environment parameters
SERVER_JAR_PATH="${SERVER_JAR_PATH:-./android-server/build/libs/server.jar}"
DESKTOP_BIN="${DESKTOP_BIN:-./build/MirrorDesktop}"
MAX_SIZE="${MAX_SIZE:-1920}"
BIT_RATE="${BIT_RATE:-8000000}"

# Device Discovery
if [ -z "${SERIAL:-}" ]; then
    DEVICES_RAW=$(adb devices | grep -v "^List of devices attached" | grep -E "\bdevice$" || true)
    DEVICE_COUNT=$(echo "${DEVICES_RAW}" | grep -c -E "\bdevice$" || true)

    if [ "${DEVICE_COUNT}" -eq 0 ]; then
        echo "Error: No connected Android devices detected via adb." >&2
        exit 1
    elif [ "${DEVICE_COUNT}" -gt 1 ]; then
        echo "Error: Multiple devices connected and SERIAL is unset. Please specify SERIAL." >&2
        adb devices -l >&2
        exit 1
    else
        SERIAL=$(echo "${DEVICES_RAW}" | awk '{print $1}')
    fi
fi

echo "Selected device serial: ${SERIAL}"

# Pre-Cleanup on device
echo "Cleaning up any stale mirror processes on device..."
adb -s "${SERIAL}" shell pkill -f com.mirror.Server || true

# Dynamic Abstract Socket Generation
SOCKET_UUID=$(od -N 32 -x /dev/urandom | head -1 | awk '{OFS="-"; print $2$3,$4,$5,$6,$7$8$9}')
SOCKET_NAME="mirror_${SOCKET_UUID}"
echo "Generated isolated abstract socket name: ${SOCKET_NAME}"

# Trap & Cleanup Handler
SERVER_PID=""
cleanup() {
    local exit_code=$?
    trap - EXIT SIGINT SIGTERM
    echo "Tearing down mirror session..."
    if [ -n "${SERVER_PID}" ] && kill -0 "${SERVER_PID}" 2>/dev/null; then
        kill "${SERVER_PID}" 2>/dev/null || true
        wait "${SERVER_PID}" 2>/dev/null || true
    fi
    if [ -n "${SERIAL}" ]; then
        adb -s "${SERIAL}" shell pkill -f com.mirror.Server || true
    fi
    exit ${exit_code}
}
trap cleanup EXIT SIGINT SIGTERM

# Staging & Remote Spawn
if [ ! -f "${SERVER_JAR_PATH}" ]; then
    echo "Error: Server jar not found at ${SERVER_JAR_PATH}" >&2
    exit 1
fi

if [ ! -f "${DESKTOP_BIN}" ]; then
    echo "Error: Desktop binary not found at ${DESKTOP_BIN}" >&2
    exit 1
fi

echo "Staging ${SERVER_JAR_PATH} to /data/local/tmp/mirror-server.jar..."
adb -s "${SERIAL}" push "${SERVER_JAR_PATH}" /data/local/tmp/mirror-server.jar > /dev/null

echo "Launching Android server daemon in background..."
adb -s "${SERIAL}" shell CLASSPATH=/data/local/tmp/mirror-server.jar app_process / com.mirror.Server --socket-name="${SOCKET_NAME}" --max-size="${MAX_SIZE}" --bit-rate="${BIT_RATE}" --stay-awake=true &
SERVER_PID=$!

# Launch Client
sleep 0.5
echo "Launching desktop client..."
"${DESKTOP_BIN}" --serial="${SERIAL}" --socket-name="${SOCKET_NAME}"
