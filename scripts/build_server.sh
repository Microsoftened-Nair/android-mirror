#!/usr/bin/env bash
set -eo pipefail

if [ -z "${ANDROID_HOME}" ]; then
    if [ -d "${HOME}/android-sdk" ]; then
        export ANDROID_HOME="${HOME}/android-sdk"
    else
        echo "Error: ANDROID_HOME environment variable is not set." >&2
        exit 1
    fi
fi

ANDROID_JAR="${ANDROID_HOME}/platforms/android-34/android.jar"
D8="${ANDROID_HOME}/build-tools/34.0.0/d8"

if [ ! -f "${ANDROID_JAR}" ]; then
    echo "Error: Android framework jar not found at ${ANDROID_JAR}" >&2
    exit 1
fi

if [ ! -f "${D8}" ]; then
    echo "Error: d8 executable not found at ${D8}" >&2
    exit 1
fi

# Ensure output directories exist and are clean
mkdir -p bin/classes
mkdir -p bin/dist
mkdir -p build
rm -rf bin/classes/*
rm -rf bin/dist/server.jar
rm -f build/server.jar

# Compile Java sources cleanly without options warnings
echo "Compiling Java 11 sources with javac..."
javac -source 11 -target 11 -Xlint:-options -cp "${ANDROID_JAR}" -d bin/classes $(find android-server/src/main/java -name "*.java")

# Package into server.jar with d8
echo "Running d8 bytecode packaging..."
"${D8}" --output bin/dist/server.jar --lib "${ANDROID_JAR}" $(find bin/classes -name "*.class")

# Ensure build/server.jar exists at the canonical path expected by audit and run.sh
cp bin/dist/server.jar build/server.jar
mkdir -p android-server/build/libs
cp bin/dist/server.jar android-server/build/libs/server.jar

echo "Build successful: bin/dist/server.jar, build/server.jar, and android-server/build/libs/server.jar"
