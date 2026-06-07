#!/bin/bash

#
# apkbuild.tool
# ONScripter-RU
#
# Android APK generation script.
# Run with "build/dir" [--release|--debug] arguments.
#
# Consult LICENSE file for licensing terms and copyright holders.
#

set -e

pushd "$(dirname "$0")" &>/dev/null
SCRIPTS="$(pwd)"
popd &>/dev/null

if [ "$1" == "" ]; then
  WORK="$SCRIPTS/../DerivedData"
else
  case "$1" in
    -h|--help)
      cat <<EOF
Usage: $0 [build-dir] [--release|--debug]

Builds an Android APK from an existing droid engine build directory.
The script generates a Gradle/AGP package under Droid-package and writes
the final APK to Droid-package/onscripter-new.apk.
EOF
      exit 0
      ;;
  esac
  WORK="$1"
fi

DROID_MIN_API="${DROID_MIN_API:-34}"
DROID_TARGET_API="${DROID_TARGET_API:-36}"
DROID_NDK_VERSION="${DROID_NDK_VERSION:-29.0.14206865}"
BUILD_TYPE="Release"

for arg in "${@:2}"; do
  case "$arg" in
    --release)
      BUILD_TYPE="Release"
      ;;
    --debug)
      BUILD_TYPE="Debug"
      ;;
    --no-recompile|--jsign)
      echo "$arg is obsolete. Android packaging now always uses Gradle/AGP, AAPT2, D8, zipalign, and apksigner."
      exit 1
      ;;
    *)
      echo "Unknown apkbuild option: $arg"
      exit 1
      ;;
  esac
done

pushd "$WORK" &>/dev/null
WORK="$(pwd)"
popd &>/dev/null

if [ ! -d "$WORK" ] || [ ! -d "$SCRIPTS/../Resources/Droid" ]; then
  echo "Invalid launch directory!"
  exit 1
fi

PKGPATH="$WORK/Droid-package"
LIBPATH="$PKGPATH/lib"
SIGNED_APK="$PKGPATH/onscripter-new.apk"

echo "Working in $WORK"

rm -rf "$PKGPATH"
cp -r "$SCRIPTS/../Resources/Droid" "$PKGPATH" || exit 1
rm -rf "$PKGPATH/bin" "$PKGPATH/build" "$PKGPATH/.gradle"
chmod +x "$PKGPATH/gradlew" 2>/dev/null || true

copy_engine() {
  local source="$1"
  local abi="$2"

  echo "Found $abi engine, copying..."
  mkdir -p "$LIBPATH/$abi" || exit 1
  cp "$source" "$LIBPATH/$abi/libmain.so" || exit 1
}

if [ -f "$WORK/onscripter-new" ] || [ -f "$WORK/onscripter-ru" ]; then
  echo "Proceeding with single arch mode..."
  ENGINE="$WORK/onscripter-new"
  if [ ! -f "$ENGINE" ]; then
    ENGINE="$WORK/onscripter-ru"
  fi
  ARCH="$(basename "$WORK")"
  case "$ARCH" in
    Droid-aarch64|Droid-arm64)
      copy_engine "$ENGINE" "arm64-v8a"
      ;;
    Droid-x86_64)
      copy_engine "$ENGINE" "x86_64"
      ;;
    Droid-arm|Droid-i386)
      echo "Unsupported Android architecture '$ARCH'. The supported package ABIs are arm64-v8a and x86_64."
      exit 1
      ;;
    *)
      echo "Unknown architecture: $ARCH, check your $WORK folder!"
      exit 1
      ;;
  esac
else
  echo "Proceeding with multi arch mode..."
  COPIED=false
  for dir in Droid-aarch64 Droid-arm64; do
    if [ -f "$WORK/$dir/onscripter-new" ]; then
      copy_engine "$WORK/$dir/onscripter-new" "arm64-v8a"
      COPIED=true
      break
    elif [ -f "$WORK/$dir/onscripter-ru" ]; then
      copy_engine "$WORK/$dir/onscripter-ru" "arm64-v8a"
      COPIED=true
      break
    fi
  done
  if [ -f "$WORK/Droid-x86_64/onscripter-new" ]; then
    copy_engine "$WORK/Droid-x86_64/onscripter-new" "x86_64"
    COPIED=true
  elif [ -f "$WORK/Droid-x86_64/onscripter-ru" ]; then
    copy_engine "$WORK/Droid-x86_64/onscripter-ru" "x86_64"
    COPIED=true
  fi
  if ! $COPIED ; then
    echo "Failed to find any engine, aborting!"
    exit 1
  fi
fi

resolve_tool() {
  local dir="$1"
  local tool="$2"
  local suffix

  for suffix in "" ".exe" ".bat" ".cmd" ".sh"; do
    if [ -f "$dir/$tool$suffix" ]; then
      echo "$dir/$tool$suffix"
      return 0
    fi
  done

  return 1
}

normalize_path() {
  if { [[ $(uname) == MINGW* ]] || [[ $(uname) == MSYS* ]] || [[ $(uname) == CYGWIN* ]]; } && command -v cygpath >/dev/null 2>&1; then
    cygpath -u "$1"
  else
    echo "$1"
  fi
}

gradle_path() {
  if { [[ $(uname) == MINGW* ]] || [[ $(uname) == MSYS* ]] || [[ $(uname) == CYGWIN* ]]; } && command -v cygpath >/dev/null 2>&1; then
    cygpath -m "$1"
  else
    echo "$1"
  fi
}

detect_android_sdk() {
  local candidates=()
  local candidate

  for envname in DROID_SDK_ROOT ANDROID_SDK_ROOT ANDROID_HOME; do
    if [ "${!envname}" != "" ]; then
      candidates+=("$(normalize_path "${!envname}")")
    fi
  done
  if [ "$DROID_PLATFORM" != "" ]; then
    candidates+=("$(dirname "$(dirname "$(normalize_path "$DROID_PLATFORM")")")")
  fi
  if [ "$DROID_TOOLS" != "" ]; then
    candidates+=("$(dirname "$(dirname "$(normalize_path "$DROID_TOOLS")")")")
  fi

  candidates+=(
    "/c/droid"
    "$HOME/Android/Sdk"
  )

  for candidate in "${candidates[@]}"; do
    if [ -f "$candidate/platforms/android-$DROID_TARGET_API/android.jar" ] &&
       [ -d "$candidate/ndk/$DROID_NDK_VERSION" ]; then
      echo "$candidate"
      return 0
    fi
  done

  return 1
}

find_jdk_bin() {
  local candidates=()
  local candidate javac keytool

  if [ "$JAVA_PATH" != "" ]; then
    candidates+=("$(normalize_path "$JAVA_PATH")")
  fi
  if [ "$JAVA_HOME" != "" ]; then
    candidates+=("$(normalize_path "$JAVA_HOME")/bin")
  fi
  if command -v javac >/dev/null 2>&1; then
    candidates+=("$(dirname "$(which javac)")")
  fi

  candidates+=(
    "/c/Program Files/Common Files/Oracle/Java/javapath"
    "/C/Program Files/Common Files/Oracle/Java/javapath"
    "/C/Program Files/Java/"jdk*/bin
    "/c/Program Files/Java/"jdk*/bin
    "/C/Program Files (x86)/Java/"jdk*/bin
    "/c/Program Files (x86)/Java/"jdk*/bin
  )

  for candidate in "${candidates[@]}"; do
    javac="$(resolve_tool "$candidate" javac || true)"
    keytool="$(resolve_tool "$candidate" keytool || true)"
    if [ "$javac" != "" ] && [ "$keytool" != "" ]; then
      echo "$candidate"
      return 0
    fi
  done

  return 1
}

SDK_ROOT="$(detect_android_sdk || true)"
if [ "$SDK_ROOT" == "" ]; then
  echo "Unable to find Android SDK platform $DROID_TARGET_API and NDK $DROID_NDK_VERSION."
  echo "Set DROID_SDK_ROOT, ANDROID_SDK_ROOT, ANDROID_HOME, DROID_PLATFORM, or DROID_TOOLS."
  exit 1
fi

echo "Using Android SDK from $SDK_ROOT"
printf "sdk.dir=%s\n" "$(gradle_path "$SDK_ROOT")" > "$PKGPATH/local.properties"

JDK_BIN="$(find_jdk_bin || true)"
if [ "$JDK_BIN" != "" ]; then
  export PATH="$JDK_BIN:$PATH"
fi

if [ "$BUILD_TYPE" == "Release" ] && [ "$ONS_ANDROID_KEYSTORE" == "" ]; then
  if [ "$JDK_BIN" == "" ]; then
    echo "Unable to find a complete JDK for release key generation, please provide JAVA_PATH or JAVA_HOME!"
    exit 1
  fi
  KEYTOOL="$(resolve_tool "$JDK_BIN" keytool)"
  KEYSTORE="$PKGPATH/Test.keystore"
  rm -f "$KEYSTORE"
  "$KEYTOOL" -genkeypair -validity 10000 \
    -dname "CN=GB, OU=Selfsign, O=Xtova Corporation, L=Unknown, S=Unknown, C=Unknown" \
    -keystore "$KEYSTORE" -storepass password -keypass password -alias Test -keyalg RSA -v
fi

if [ "$GRADLE" != "" ]; then
  GRADLE_CMD=("$GRADLE")
elif command -v gradle >/dev/null 2>&1; then
  GRADLE_CMD=(gradle)
else
  GRADLE_CMD=("./gradlew")
fi

TASK="assemble$BUILD_TYPE"
echo "Building Android package with Gradle task $TASK..."
pushd "$PKGPATH" &>/dev/null
"${GRADLE_CMD[@]}" --no-daemon "$TASK"
popd &>/dev/null

VARIANT_DIR="$(echo "$BUILD_TYPE" | tr '[:upper:]' '[:lower:]')"
APK_OUTPUT="$PKGPATH/build/outputs/apk/$VARIANT_DIR/onscripter-new-$VARIANT_DIR.apk"
if [ ! -f "$APK_OUTPUT" ]; then
  APK_OUTPUT="$(find "$PKGPATH/build/outputs/apk/$VARIANT_DIR" -type f -name "*.apk" | head -n 1)"
fi
if [ "$APK_OUTPUT" == "" ] || [ ! -f "$APK_OUTPUT" ]; then
  echo "Gradle completed, but no APK was found under $PKGPATH/build/outputs/apk/$VARIANT_DIR."
  exit 1
fi

cp "$APK_OUTPUT" "$SIGNED_APK"
rm -f "$PKGPATH/Test.keystore"

echo "Please grab your apk at $SIGNED_APK"

exit 0
