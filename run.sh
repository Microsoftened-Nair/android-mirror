#!/usr/bin/env bash
set -eo pipefail

# Configurable environment parameters
SERVER_JAR_PATH="${SERVER_JAR_PATH:-./android-server/build/libs/server.jar}"
DESKTOP_BIN="${DESKTOP_BIN:-./build/MirrorDesktop}"
MAX_SIZE="${MAX_SIZE:-1920}"
BIT_RATE="${BIT_RATE:-8000000}"

echo "=== Android Mirror Orchestrator ==="

# 1. Device Discovery
if [ -z "${SERIAL:-}" ]; then
    echo "[Setup] Scanning for connected Android devices via adb..."
    DEVICES_RAW=$(adb devices | grep -v "^List of devices attached" | grep -E "\bdevice$" || true)
    DEVICE_COUNT=$(echo "${DEVICES_RAW}" | grep -c -E "\bdevice$" || true)

    if [ "${DEVICE_COUNT}" -eq 0 ]; then
        echo "Error: No authorized Android devices detected via adb. Ensure USB debugging is enabled." >&2
        exit 1
    elif [ "${DEVICE_COUNT}" -gt 1 ]; then
        echo "Error: Multiple devices connected and SERIAL is unset. Please specify SERIAL:" >&2
        adb devices -l >&2
        exit 1
    else
        SERIAL=$(echo "${DEVICES_RAW}" | awk '{print $1}')
    fi
fi

DEVICE_MODEL=$(adb -s "${SERIAL}" shell getprop ro.product.model 2>/dev/null | tr -d '\r\n' || echo "Unknown")
ANDROID_VER=$(adb -s "${SERIAL}" shell getprop ro.build.version.release 2>/dev/null | tr -d '\r\n' || echo "?")
SDK_INT=$(adb -s "${SERIAL}" shell getprop ro.build.version.sdk 2>/dev/null | tr -d '\r\n' || echo "?")
echo "[Setup] Target Device: ${DEVICE_MODEL} (Serial: ${SERIAL}, Android ${ANDROID_VER}, API ${SDK_INT})"

# 2. Pre-Cleanup on device
echo "[Setup] Terminating any stale mirror daemon processes on device..."
adb -s "${SERIAL}" shell pkill -f com.mirror.Server || true

# 3. Dynamic Abstract Socket Generation
SOCKET_UUID=$(od -N 16 -x /dev/urandom | head -1 | awk '{OFS="-"; print $2$3,$4,$5,$6,$7$8$9}')
SOCKET_NAME="mirror_${SOCKET_UUID}"
echo "[Setup] Allocated isolated abstract Unix socket: ${SOCKET_NAME}"

# 4. Trap & Cleanup Handler
SERVER_PID=""
DAEMON_LOG="/tmp/mirror_server_${SERIAL}.log"
cleanup() {
    local exit_code=$?
    trap - EXIT SIGINT SIGTERM
    echo ""
    echo "[Teardown] Cleaning up session..."
    if [ -n "${SERVER_PID}" ] && kill -0 "${SERVER_PID}" 2>/dev/null; then
        kill "${SERVER_PID}" 2>/dev/null || true
        wait "${SERVER_PID}" 2>/dev/null || true
    fi
    if [ -n "${SERIAL}" ]; then
        adb -s "${SERIAL}" shell pkill -f com.mirror.Server 2>/dev/null || true
    fi
    if [ "${exit_code}" -ne 0 ] && [ -f "${DAEMON_LOG}" ]; then
        echo "[Teardown] Daemon captured logs (exit code ${exit_code}):"
        cat "${DAEMON_LOG}"
    fi
    echo "[Teardown] Session terminated cleanly."
    exit ${exit_code}
}
trap cleanup EXIT SIGINT SIGTERM

# 5. Staging Binary Artifacts
if [ ! -f "${SERVER_JAR_PATH}" ]; then
    echo "Error: Server jar not found at ${SERVER_JAR_PATH}. Run ./scripts/build_server.sh first." >&2
    exit 1
fi

if [ ! -f "${DESKTOP_BIN}" ]; then
    echo "Error: Desktop binary not found at ${DESKTOP_BIN}. Run cmake --build build first." >&2
    exit 1
fi

echo "[Deploy] Staging ${SERVER_JAR_PATH} to /data/local/tmp/mirror-server.jar..."
adb -s "${SERIAL}" push "${SERVER_JAR_PATH}" /data/local/tmp/mirror-server.jar

# 6. Remote Daemon Execution
echo "[Deploy] Spawning Android server daemon via app_process..."
echo "[Deploy] Options: max-size=${MAX_SIZE}, bit-rate=${BIT_RATE}, stay-awake=true"
adb -s "${SERIAL}" shell "CLASSPATH=/data/local/tmp/mirror-server.jar app_process / com.mirror.Server --socket-name=${SOCKET_NAME} --max-size=${MAX_SIZE} --bit-rate=${BIT_RATE} --stay-awake=true" > "${DAEMON_LOG}" 2>&1 &
SERVER_PID=$!
echo "[Deploy] Server process started (host adb PID: ${SERVER_PID})"

# 7. Wait for Server Socket Readiness
echo "[Deploy] Polling for daemon socket readiness..."
SOCKET_READY=0
for i in $(seq 1 30); do
    if adb -s "${SERIAL}" shell "grep -q '${SOCKET_NAME}' /proc/net/unix" 2>/dev/null; then
        SOCKET_READY=1
        echo "[Deploy] Daemon socket bound and listening! ($((i * 100))ms elapsed)"
        break
    fi
    if ! kill -0 "${SERVER_PID}" 2>/dev/null; then
        echo "Error: Daemon process died during initialization. Captured logs:" >&2
        cat "${DAEMON_LOG}" >&2
        exit 1
    fi
    sleep 0.1
done

if [ "${SOCKET_READY}" -ne 1 ]; then
    echo "[Deploy] Note: Socket polling timed out, attempting connection handshake..."
fi

# 8. Launch Desktop Client
echo "[Client] Launching desktop UI application..."
"${DESKTOP_BIN}" --serial="${SERIAL}" --socket-name="${SOCKET_NAME}"
