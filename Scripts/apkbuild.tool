#!/bin/bash

#
# apkbuild.tool
# ONScripter-RU
#
# Android APK generation script.
# Run with "build/dir" [--jsign] arguments.
#
# Consult LICENSE file for licensing terms and copyright holders.
#

pushd "$(dirname "$0")" &>/dev/null
SCRIPTS="$(pwd)"
popd &>/dev/null

if [ "$1" == "" ]; then
  WORK="$SCRIPTS/../DerivedData"
else
  WORK="$1"
fi

DROID_MIN_API="${DROID_MIN_API:-34}"
DROID_TARGET_API="${DROID_TARGET_API:-36}"
DROID_BUILD_TOOLS_VERSION="${DROID_BUILD_TOOLS_VERSION:-36.1.0}"

RECOMPILE=true
JAVASIGN=false

if [ "$2" == "--no-recompile" ] || [ "$3" == "--no-recompile" ]; then
  echo "Precompiled Android package resources are not supported after raising the floor to Android 14/API 34."
  echo "Recompile the package with Android SDK platform $DROID_TARGET_API and build-tools $DROID_BUILD_TOOLS_VERSION."
  exit 1
fi
if [ "$2" == "--jsign" ] || [ "$3" == "--jsign" ]; then
  JAVASIGN=true
fi

pushd "$WORK" &>/dev/null
WORK="$(pwd)"
popd &>/dev/null

if [ ! -d "$WORK" ] || [ ! -d "$SCRIPTS/../Resources/Droid" ]; then
  echo "Invalid launch directory!"
  exit 1
fi

PKGPATH="$WORK/Droid-package"
SDLACTPATH="$PKGPATH/src/SDLActivity.java"
ONSACTPATH="$PKGPATH/src/ONSActivity.java"
SDLJPATH="$PKGPATH/src/SDL.java"
SDLAUDIOPATH="$PKGPATH/src/SDLAudioManager.java"
SDLCTRLPATH="$PKGPATH/src/SDLControllerManager.java"
HIDDEVPATH="$PKGPATH/src/HIDDevice.java"
HIDBLEPATH="$PKGPATH/src/HIDDeviceBLESteamController.java"
HIDMGRPATH="$PKGPATH/src/HIDDeviceManager.java"
HIDUSBPATH="$PKGPATH/src/HIDDeviceUSB.java"
LIBPATH="$PKGPATH/lib"
BINPATH="$PKGPATH/bin"
RESPATH="$PKGPATH/res"
ARSCPATH="$PKGPATH/bin/resources.arsc"
MANPATH="$PKGPATH/bin/AndroidManifest.xml"
TXTMANPATH="$PKGPATH/AndroidManifest.xml"
APTPATH="$PKGPATH/apt"
UNSIGNED_APK="$PKGPATH/unsigned.apk"
ALIGNED_APK="$PKGPATH/aligned.apk"
SIGNED_APK="$PKGPATH/onscripter-ru.apk"

KEYSTORE="$PKGPATH/Test.keystore"
CERTPATH="$PKGPATH/cert.pem"
KEYPATH="$PKGPATH/key.pem"

echo "Working in $WORK"

rm -rf "$WORK/Droid-package" 
cp -r "$SCRIPTS/../Resources/Droid" "$WORK/Droid-package" || exit 1

if [ -f "$WORK/onscripter-ru" ]; then 
  echo "Proceeding with single arch mode..."
  ARCH="$(basename "$WORK")"
  if [ "$ARCH" == "Droid-aarch64" ] || [ "$ARCH" == "Droid-arm64" ]; then
    echo "Found arm64-v8a engine, copying..."
    mkdir -p "$PKGPATH/lib/arm64-v8a" || exit 1
    cp "$WORK/onscripter-ru" "$PKGPATH/lib/arm64-v8a/libmain.so" || exit 1
  elif [ "$ARCH" == "Droid-x86_64" ]; then
    echo "Found x86_64 engine, copying..."
    mkdir -p "$PKGPATH/lib/x86_64" || exit 1
    cp "$WORK/onscripter-ru" "$PKGPATH/lib/x86_64/libmain.so" || exit 1
  elif [ "$ARCH" == "Droid-arm" ] || [ "$ARCH" == "Droid-i386" ]; then
    echo "Unsupported Android architecture '$ARCH'. The supported package ABIs are arm64-v8a and x86_64."
    exit 1
  else
    echo "Unknown architecture: $ARCH, check your $WORK folder!"
    exit 1
  fi
else
  echo "Proceeding with multi arch mode..."
  COPIED=false
  if [ -f "$WORK/Droid-aarch64/onscripter-ru" ]; then
    echo "Found arm64-v8a engine, copying..."
    mkdir -p "$PKGPATH/lib/arm64-v8a" || exit 1
    cp "$WORK/Droid-aarch64/onscripter-ru" "$PKGPATH/lib/arm64-v8a/libmain.so" || exit 1
    COPIED=true
  fi
  if [ -f "$WORK/Droid-arm64/onscripter-ru" ]; then
    echo "Found arm64-v8a engine, copying..."
    mkdir -p "$PKGPATH/lib/arm64-v8a" || exit 1
    cp "$WORK/Droid-arm64/onscripter-ru" "$PKGPATH/lib/arm64-v8a/libmain.so" || exit 1
    COPIED=true
  fi
  if [ -f "$WORK/Droid-x86_64/onscripter-ru" ]; then
    echo "Found x86_64 engine, copying..."
    mkdir -p "$PKGPATH/lib/x86_64" || exit 1
    cp "$WORK/Droid-x86_64/onscripter-ru" "$PKGPATH/lib/x86_64/libmain.so" || exit 1
    COPIED=true
  fi
  if ! $COPIED ; then
    echo "Failed to find any engine, aborting!"
    exit 1
  fi
fi

# Further code requires at least one of:
# $PKGPATH/lib/arm64-v8a/libmain.so
# $PKGPATH/lib/x86_64/libmain.so

compile_sources() {
  echo "Compiling sources..."
  
  OBJPATH="$PKGPATH/obj"
  rm -rf "$OBJPATH"
  
  mkdir -p "$OBJPATH"
  "$JAVAC" -source 1.8 -target 1.8 -classpath "$CLASSPATH" -Xlint:deprecation -d "$OBJPATH" "$SDLACTPATH" "$SDLJPATH" "$SDLAUDIOPATH" "$SDLCTRLPATH" "$ONSACTPATH" "$HIDDEVPATH" "$HIDBLEPATH" "$HIDMGRPATH" "$HIDUSBPATH"

  if (( $? )); then
    exit 1
  fi

  rm -rf "$BINPATH"
  mkdir -p "$BINPATH"
  classfiles=()
  while IFS= read -r classfile; do
    classfiles+=("$classfile")
  done < <(find "$OBJPATH" -type f -name "*.class")

  "$D8" --min-api "$DROID_MIN_API" --output "$BINPATH" "${classfiles[@]}"
  if (( $? )); then
    exit 1
  fi
  
  rm -rf "$OBJPATH"
  echo "Compiling sources successful!"
}

create_apk() {
  echo "Creating APK..."
  rm -f "$UNSIGNED_APK"
  rm -f "$SIGNED_APK"
  
  if [ "$RECOMPILE" != "true" ]; then
    rm -rf "$APTPATH"
    
    mkdir -p "$APTPATH"
    cp "$MANPATH" "$APTPATH/"
    cp "$BINPATH/classes.dex" "$APTPATH/"
    cp "$ARSCPATH" "$APTPATH/"
    cp -a "$LIBPATH" "$APTPATH/"
    cp -a "$RESPATH" "$APTPATH/"
    
    pushd "$APTPATH" &>/dev/null
    find . -type f -name ".*" | xargs rm -f
    find . -type f -name "Thumbs.db" | xargs rm -f
    
    zip -qry "$UNSIGNED_APK" *
    popd &>/dev/null
        
    rm -rf "$APTPATH"
  else
    "$AAPT" package -f -M "$TXTMANPATH" -S "$RESPATH" -I "$CLASSPATH" -F "$UNSIGNED_APK" "$BINPATH"
    if (( $? )); then
      exit 1
    fi

    pushd "$PKGPATH" &>/dev/null
    if [ -f "lib/arm64-v8a/libmain.so" ]; then
      "$AAPT" add "$UNSIGNED_APK" "lib/arm64-v8a/libmain.so"
    fi
    if [ -f "lib/x86_64/libmain.so" ]; then
      "$AAPT" add "$UNSIGNED_APK" "lib/x86_64/libmain.so"
    fi
    popd &>/dev/null
    
    # Now update binary resources and manifest
    rm -rf "$APTPATH"
    
    mkdir -p "$APTPATH"
    pushd "$APTPATH" &>/dev/null
    unzip -q "$UNSIGNED_APK"
    
    rm -f "$MANPATH"
    rm -f "$ARSCPATH"
    
    mv "AndroidManifest.xml" "$MANPATH"
    mv "resources.arsc" "$ARSCPATH"
    
    rm -rf "$APTPATH"
  fi
  
  echo "APK creation successful!"
}

sign_apk() {
  echo "Signing APK..."
  rm -f "$ALIGNED_APK"
  if [ "$JAVASIGN" != "true" ] && [ "$APKSIGNER" != "" ]; then
    rm -f "$KEYSTORE"
    "$JAVAKEY" -genkeypair -validity 10000 \
      -dname "CN=GB, OU=Selfsign, O=Xtova Corporation, L=Unknown, S=Unknown, C=Unknown" \
      -keystore "$KEYSTORE" -storepass password -keypass password -alias Test -keyalg RSA -v
    if (( $? )); then
      exit 1
    fi

    "$ZIPALIGN" -f 4 "$UNSIGNED_APK" "$ALIGNED_APK"
    if (( $? )); then
      exit 1
    fi

    "$APKSIGNER" sign --ks "$KEYSTORE" --ks-pass pass:password --key-pass pass:password --min-sdk-version "$DROID_MIN_API" --out "$SIGNED_APK" "$ALIGNED_APK"
    if (( $? )); then
      exit 1
    fi

    rm -f "$KEYSTORE" "$ALIGNED_APK" "$UNSIGNED_APK"
    echo "APK signing successful!"
    return
  fi

  if [ "$JAVASIGN" != "true" ]; then
    rm -rf "$APTPATH"
    
    mkdir -p "$APTPATH"
    pushd "$APTPATH" &>/dev/null
    unzip -q "$UNSIGNED_APK"
    
    files=( $(find * -type f) )
    
    mkdir META-INF
    
    # MANIFEST.MF
    printf "Manifest-Version: 1.0\r\n" > META-INF/MANIFEST.MF
    printf "Created-By: 9.6.96 (Xtova Corporation)\r\n\r\n" >> META-INF/MANIFEST.MF
    
    digests=()
    
    for f in "${files[@]}"; do
      hash=$(openssl sha1 -binary "$f" | openssl base64)
      hash256=$(openssl sha256 -binary "$f" | openssl base64)
      digest="Name: $f\r\nSHA-256-Digest: $hash256\r\nSHA1-Digest: $hash\r\n\r\n"
      digests+=("$digest")
      printf "$digest" >> META-INF/MANIFEST.MF
    done
    
    # SELFSIGN.SF
    printf "Signature-Version: 1.0\r\n" > META-INF/SELFSIGN.SF
    hash=$(openssl sha1 -binary "META-INF/MANIFEST.MF" | openssl base64)
    hash256=$(openssl sha256 -binary "META-INF/MANIFEST.MF" | openssl base64)
    printf "SHA-256-Digest-Manifest: $hash256\r\n" >> META-INF/SELFSIGN.SF
    printf "SHA1-Digest-Manifest: $hash\r\n" >> META-INF/SELFSIGN.SF
    printf "Created-By: 9.6.96 (Xtova Corporation)\r\n\r\n" >> META-INF/SELFSIGN.SF
    
    dignum="${#digests[@]}"
    
    for ((i=0; $i<$dignum; i++)); do
      digest="${digests[$i]}"
      file="${files[$i]}"
      hash=$(printf "$digest" | openssl sha1 -binary | openssl base64)
      hash256=$(printf "$digest" | openssl sha256 -binary | openssl base64)
      printf "Name: $file\r\nSHA-256-Digest: $hash256\r\nSHA1-Digest: $hash\r\n\r\n" >> META-INF/SELFSIGN.SF
    done
    
    # SELFSIGN.RSA
    rm -f "$KEYPATH"
    rm -f "$CERTPATH"
    case $(uname) in
      MINGW32*)
        openssl req -x509 -newkey rsa:2048 -keyout "$KEYPATH" -out "$CERTPATH" -days 3650 -nodes -subj "//C=GB\ST=Unknown\L=Unknown\O=Xtova Corporation\OU=Selfsign\CN=Unknown"
        ;;
      *)
        openssl req -x509 -newkey rsa:2048 -keyout "$KEYPATH" -out "$CERTPATH" -days 3650 -nodes -subj "/C=GB/ST=Unknown/L=Unknown/O=Xtova Corporation/OU=Selfsign/CN=Unknown"
        ;;
    esac
    openssl smime -sign -noattr -in META-INF/SELFSIGN.SF -outform der -out META-INF/SELFSIGN.RSA -inkey "$KEYPATH" -signer "$CERTPATH" -md sha1
    if (( $? )); then
      exit 1
    fi
    
    rm -f "$KEYPATH"
    rm -f "$CERTPATH"
    
    zip -qry "$SIGNED_APK" *
    popd &>/dev/null

    rm -rf "$APTPATH"
  else
    rm -f "$KEYSTORE"
    "$JAVAKEY" -genkeypair -validity 10000 \
      -dname "CN=GB, OU=Selfsign, O=Xtova Corporation, L=Unknown, S=Unknown, C=Unknown" \
      -keystore "$KEYSTORE" -storepass password -keypass password -alias Test -keyalg RSA -v
    if (( $? )); then
      exit 1
    fi

    "$JAVASIGNER" -digestalg SHA1 -sigalg SHA1withRSA -keystore "$KEYSTORE" -storepass password -keypass password -signedjar "$SIGNED_APK" "$UNSIGNED_APK" Test
    if (( $? )); then
      exit 1
    fi
    
    rm -f "$KEYSTORE"
  fi
  
  rm -f "$UNSIGNED_APK"
  echo "APK signing successful!"
}

if [ "$RECOMPILE" != "true" ]; then
  command -v zip >/dev/null 2>&1 
  if (( $? )); then
    echo "Unable to find zip, which is required to create apk!"
    exit 1
  fi
fi

if [ "$JAVASIGN" != "true" ]; then
  command -v openssl >/dev/null 2>&1 
  if (( $? )); then
    echo "Unable to find openssl, which is required to sign apk!"
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

if [ "$JAVASIGN" == "true" ] || [ "$RECOMPILE" == "true" ]; then
  if [ -z "$JAVA_PATH" ]; then
    java_candidates=()
    if [ "$JAVA_HOME" != "" ]; then
      java_candidates+=("$JAVA_HOME/bin")
    fi
    if command -v javac >/dev/null 2>&1; then
      java_candidates+=("$(dirname "$(which javac)")")
    fi
    java_candidates+=(
      "/C/Program Files/Java/"jdk*/bin
      "/C/Program Files (x86)/Java/"jdk*/bin
    )

    for candidate in "${java_candidates[@]}"; do
      JAVAC="$(resolve_tool "$candidate" javac)"
      JAVAKEY="$(resolve_tool "$candidate" keytool)"
      JAVASIGNER="$(resolve_tool "$candidate" jarsigner)"
      if [ "$JAVAC" != "" ] && [ "$JAVAKEY" != "" ] && [ "$JAVASIGNER" != "" ]; then
        JAVA_PATH="$candidate"
        break
      fi
    done

    if [ "$JAVA_PATH" == "" ]; then
      echo "Unable to find a complete JDK, please provide JAVA_PATH or JAVA_HOME!"
      exit 1
    fi
  fi
  
  echo "Using jdk from $JAVA_PATH"
  
  JAVAC="$(resolve_tool "$JAVA_PATH" javac)"
  JAVAKEY="$(resolve_tool "$JAVA_PATH" keytool)"
  JAVASIGNER="$(resolve_tool "$JAVA_PATH" jarsigner)"

  if [ "$RECOMPILE" == "true" ]; then

    if [ -z "$DROID_TOOLS" ]; then
      for candidate in "/C/droid/build-tools/$DROID_BUILD_TOOLS_VERSION" "/c/droid/build-tools/$DROID_BUILD_TOOLS_VERSION"; do
        if [ -d "$candidate" ]; then
          DROID_TOOLS="$candidate"
          break
        fi
      done
    fi

    if [ -z "$DROID_TOOLS" ]; then
      echo "Unable to find Android build-tools $DROID_BUILD_TOOLS_VERSION, please provide DROID_TOOLS variable!"
      exit 1
    fi

    echo "Using droid build-tools from $DROID_TOOLS"
    
    D8="$(resolve_tool "$DROID_TOOLS" d8)"
    AAPT="$(resolve_tool "$DROID_TOOLS" aapt)"
    APKSIGNER="$(resolve_tool "$DROID_TOOLS" apksigner)"
    ZIPALIGN="$(resolve_tool "$DROID_TOOLS" zipalign)"

    if [ -z "$DROID_PLATFORM" ]; then
      for candidate in "/C/droid/platforms/android-$DROID_TARGET_API" "/c/droid/platforms/android-$DROID_TARGET_API"; do
        if [ -d "$candidate" ]; then
          DROID_PLATFORM="$candidate"
          break
        fi
      done
    fi

    if [ -z "$DROID_PLATFORM" ]; then
      echo "Unable to find Android SDK platform $DROID_TARGET_API, please provide DROID_PLATFORM variable!"
      exit 1
    fi

    echo "Using droid platform from $DROID_PLATFORM"
    
    if [ "$D8" == "" ] || [ "$AAPT" == "" ] || [ "$APKSIGNER" == "" ] || [ "$ZIPALIGN" == "" ]; then
      echo "Unable to find d8, aapt, apksigner, or zipalign in Android build-tools $DROID_TOOLS!"
      exit 1
    fi

    CLASSPATH="$DROID_PLATFORM/android.jar"

    compile_sources
  fi
fi

create_apk
sign_apk

echo "Please grab your apk at $SIGNED_APK"

exit 0
