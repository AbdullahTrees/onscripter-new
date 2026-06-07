#!/bin/bash

#
# ndktoolchain.sh
# ONScripter-RU
#
# Android NDK toolchain setup script.
#
# Consult LICENSE file for licensing terms and copyright holders.
#

srcdir=$(dirname "$0")
source "$srcdir/../Dependencies/common.sh"

if [ "$#" != "1" ]; then
    error "usage: ./ndktoolchain.sh <path>"
    exit 1
fi

dstdir="$1"
ndkpath="$dstdir/ndk"

ndkver="r29"
ndkrel="1"
ndkfullver="29.0.14206865"
ndkgood=true
droid_min_api="34"

# Keep these in sync with the supported Android package ABIs.
ndkarch=(
    "arm64"
    "x86_64"
)
ndkabi=(
    "aarch64-linux-android"
    "x86_64-linux-android"
)
ndkapi=(
    "$droid_min_api"
    "$droid_min_api"
)
archnum="${#ndkarch[@]}"

is_windows_host() {
    case $(uname) in
        MINGW*|MSYS*|CYGWIN*) return 0 ;;
        *) return 1 ;;
    esac
}

for ((i=0; $i<$archnum; i++)); do
    arch="${ndkarch[$i]}"
    api="${ndkapi[$i]}"
    abi="${ndkabi[$i]}"
    toolchain="$dstdir/ndk/toolchain-$arch"

    if [ ! -f "$toolchain/triple" ] || [ "$(cat "$toolchain/triple")" != "$abi" ]; then
        ndkgood=false
        break
    fi

    if [ ! -f "$toolchain/api" ] || [ "$(cat "$toolchain/api")" != "$api" ]; then
        ndkgood=false
        break
    fi

    if [ ! -f "$toolchain/version" ] || [ "$(cat "$toolchain/version")" != "${ndkver}-${ndkrel}" ]; then
        ndkgood=false
        break
    fi

    if is_windows_host && { [ ! -f "$toolchain/bin/clang.cmd" ] || [ ! -f "$toolchain/bin/${abi}-ar.cmd" ]; }; then
        ndkgood=false
        break
    fi
done

if $ndkgood; then
    exit 0
fi

normalize_path() {
    if { [[ $(uname) == MINGW* ]] || [[ $(uname) == MSYS* ]]; } && command -v cygpath >/dev/null 2>&1; then
        cygpath -u "$1"
    else
        echo "$1"
    fi
}

find_existing_ndk() {
    local candidates=()

    if [ "$ANDROID_NDK_HOME" != "" ]; then
        candidates+=("$(normalize_path "$ANDROID_NDK_HOME")")
    fi
    if [ "$ANDROID_NDK_ROOT" != "" ]; then
        candidates+=("$(normalize_path "$ANDROID_NDK_ROOT")")
    fi
    if [ "$ANDROID_SDK_ROOT" != "" ]; then
        candidates+=("$(normalize_path "$ANDROID_SDK_ROOT")/ndk/$ndkfullver")
    fi
    if [ "$ANDROID_HOME" != "" ]; then
        candidates+=("$(normalize_path "$ANDROID_HOME")/ndk/$ndkfullver")
    fi

    candidates+=(
        "/c/droid/ndk/$ndkfullver"
        "$HOME/Android/Sdk/ndk/$ndkfullver"
        "$ndkpath/android-ndk-$ndkver"
    )

    local candidate
    for candidate in "${candidates[@]}"; do
        if [ -x "$candidate/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe" ] ||
           [ -x "$candidate/toolchains/llvm/prebuilt/linux-x86_64/bin/clang" ] ||
           [ -x "$candidate/toolchains/llvm/prebuilt/darwin-x86_64/bin/clang" ]; then
            echo "$candidate"
            return 0
        fi
    done

    return 1
}

host_tag() {
    case $(uname) in
        Darwin*) echo "darwin-x86_64" ;;
        MINGW*|MSYS*) echo "windows-x86_64" ;;
        Linux*)  echo "linux-x86_64" ;;
        *)
            error "Unsupported NDK host: %s" "$(uname)"
            return 1 ;;
    esac
}

tool_path() {
    local bindir="$1"
    local tool="$2"

    if [ -x "$bindir/$tool" ]; then
        echo "$bindir/$tool"
    elif [ -x "$bindir/$tool.exe" ]; then
        echo "$bindir/$tool.exe"
    else
        error "Unable to find NDK tool %s in %s" "$tool" "$bindir"
        return 1
    fi
}

write_wrapper() {
    local wrapper="$1"
    local target="$2"
    local extra="$3"

    cat > "$wrapper" <<EOF
#!/bin/sh
exec "$target" $extra "\$@"
EOF
    chmod a+x "$wrapper"
}

write_cmd_wrapper() {
    local wrapper="$1"
    local target="$2"
    local extra="$3"
    local wintarget="$target"

    if command -v cygpath >/dev/null 2>&1; then
        wintarget="$(cygpath -m "$target")"
    fi

    {
        printf '@echo off\r\n'
        if [ "$extra" != "" ]; then
            printf '"%s" %s %%*\r\n' "$wintarget" "$extra"
        else
            printf '"%s" %%*\r\n' "$wintarget"
        fi
    } > "$wrapper"
}

download_ndk() {
    local platform package url hash hashcmd nhash ndkdir

    case $(uname) in
        MINGW*|MSYS*)
            platform="windows"
            hash="ab3bb30fbb9e6903666d60c55d11e78b04e07472"
            ;;
        Linux*)
            platform="linux"
            hash="87e2bb7e9be5d6a1c6cdf5ec40dd4e0c6d07c30b"
            ;;
        Darwin*)
            error "Install NDK %s with Android SDK Manager on macOS and set ANDROID_NDK_HOME. Direct r29 macOS downloads are DMG-based." "$ndkfullver"
            return 1
            ;;
        *)
            error "Unsupported NDK host: %s" "$(uname)"
            return 1
            ;;
    esac

    ndkdir="android-ndk-${ndkver}"
    package="${ndkdir}-${platform}.zip"
    url="https://dl.google.com/android/repository/${package}"

    mkdir -p "$ndkpath"
    if [ ! -f "$ndkpath/$package" ]; then
        msg "Downloading Android NDK %s..." "$ndkver"
        if command -v wget >/dev/null 2>&1; then
            wget -O "$ndkpath/$package" "$url"
        else
            curl -o "$ndkpath/$package" "$url"
        fi
        if [ ! -f "$ndkpath/$package" ]; then
            error "Unable to download NDK!"
            return 1
        fi
    fi

    if command -v shasum >/dev/null 2>&1; then
        hashcmd=(shasum -a 1)
        nhash=$("${hashcmd[@]}" "$ndkpath/$package" | cut -f1 -d' ')
    else
        nhash=$(openssl sha1 "$ndkpath/$package" | cut -f2 -d' ')
    fi

    if [ "$nhash" != "$hash" ]; then
        error "Invalid NDK hash %s, expected %s" "$nhash" "$hash"
        rm -f "$ndkpath/$package"
        return 1
    fi

    msg "Extracting Android NDK %s..." "$ndkver"
    rm -rf "$ndkpath/$ndkdir"
    unzip -q "$ndkpath/$package" -d "$ndkpath" || return 1
    [ -d "$ndkpath/$ndkdir" ] || return 1
    echo "$ndkpath/$ndkdir"
}

ndkroot="$(find_existing_ndk)"
if [ "$ndkroot" == "" ]; then
    ndkroot="$(download_ndk)" || exit 1
fi

host="$(host_tag)" || exit 1
llvmbin="$ndkroot/toolchains/llvm/prebuilt/$host/bin"
clang="$(tool_path "$llvmbin" clang)" || exit 1
clangxx="$(tool_path "$llvmbin" clang++)" || exit 1

msg "Using Android NDK at %s" "$ndkroot"

for ((i=0; $i<$archnum; i++)); do
    arch="${ndkarch[$i]}"
    api="${ndkapi[$i]}"
    abi="${ndkabi[$i]}"
    toolchain="$dstdir/ndk/toolchain-$arch"

    msg "Making %s (%s/%s) wrapper toolchain in %s" "$arch" "$api" "$abi" "$toolchain"
    rm -rf "$toolchain"
    mkdir -p "$toolchain/bin" || exit 1

    write_wrapper "$toolchain/bin/clang" "$clang" "--target=${abi}${api}"
    write_wrapper "$toolchain/bin/clang++" "$clangxx" "--target=${abi}${api}"
    write_wrapper "$toolchain/bin/${abi}-clang" "$clang" "--target=${abi}${api}"
    write_wrapper "$toolchain/bin/${abi}-clang++" "$clangxx" "--target=${abi}${api}"
    if is_windows_host; then
        write_cmd_wrapper "$toolchain/bin/clang.cmd" "$clang" "--target=${abi}${api}"
        write_cmd_wrapper "$toolchain/bin/clang++.cmd" "$clangxx" "--target=${abi}${api}"
        write_cmd_wrapper "$toolchain/bin/${abi}-clang.cmd" "$clang" "--target=${abi}${api}"
        write_cmd_wrapper "$toolchain/bin/${abi}-clang++.cmd" "$clangxx" "--target=${abi}${api}"
    fi

    for tool in ar ranlib strip nm objcopy objdump readelf; do
        target="$(tool_path "$llvmbin" "llvm-$tool")" || exit 1
        write_wrapper "$toolchain/bin/$tool" "$target" ""
        write_wrapper "$toolchain/bin/${abi}-${tool}" "$target" ""
        if is_windows_host; then
            write_cmd_wrapper "$toolchain/bin/$tool.cmd" "$target" ""
            write_cmd_wrapper "$toolchain/bin/${abi}-${tool}.cmd" "$target" ""
        fi
    done

    echo "$abi" > "$toolchain/triple"
    echo "$api" > "$toolchain/api"
    echo "${ndkver}-${ndkrel}" > "$toolchain/version"
    echo "$ndkroot" > "$toolchain/ndk-root"
done

exit 0
