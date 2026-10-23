#!/bin/sh
set -e
here=$(cd "$(dirname "$0")" && pwd)
jar=${ANDROID_JAR:-${ANDROID_HOME:-$HOME/Android/Sdk}/platforms/android-23/android.jar}
work=$(mktemp -d)
javac -source 8 -target 8 -bootclasspath "$jar" -d "$work" "$here/fizmo/android/FizmoActivity.java"
if command -v d8 >/dev/null 2>&1; then
    d8 --min-api 26 --output "$work" $(find "$work" -name '*.class')
    cp "$work/classes.dex" "$here/classes.dex"
else
    dx --dex --min-sdk-version=26 --output="$here/classes.dex" "$work"
fi
rm -rf "$work"
echo "$here/classes.dex"
