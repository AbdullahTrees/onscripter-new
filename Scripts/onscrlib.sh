#!/bin/bash

#
# onscrlib.sh
# ONScripter-RU
#
# Xcode dependency generation script.
#
# Consult LICENSE file for licensing terms and copyright holders.
#

IOS=0
ARCH="${1}"
PROJECT_DIR="${2}"
ACTION="${3}"
MACOS_DEPLOYMENT_FLOOR=14.0
IOS_DEPLOYMENT_FLOOR=17.0

if [ ! -d "${PROJECT_DIR}/Dependencies" ]; then
  echo "Invalid path project path: ${PROJECT_DIR}!"
  exit 1
fi

if [ "${ARCH}" == "x86_64" ]; then
  DST="${PROJECT_DIR}/DerivedData/Xcode/onscrlib64"
  BLD_ARCH="x86_64"
  VERMIN="$MACOS_DEPLOYMENT_FLOOR"
elif [ "${ARCH}" == "arm64" ]; then
  DST="${PROJECT_DIR}/DerivedData/Xcode/onscrlib-arm64"
  BLD_ARCH="arm64"
  VERMIN="$IOS_DEPLOYMENT_FLOOR"
  IOS=1
else
  echo "Unsupported Xcode dependency architecture '${ARCH}'. Supported architectures are x86_64 for macOS and arm64 for iOS."
  exit 1
fi

# Trash Xcode overrides.
export PATH="/opt/local/bin:/usr/local/bin:$(getconf PATH)"

mkdir -p "${DST}"

if [ "${ACTION}" == "clean" ]; then
  echo "NOT cleaning onscrlib, do that manually!"
  exit 0
fi

pushd "${PROJECT_DIR}/Dependencies";
find . -type d -exec mkdir -p ${DST}/{} \;
find . -type f -exec cp {} ${DST}/{} \;
find . -type l -exec cp -a {} ${DST}/{} \;
popd

export PATH="/opt/local/bin:/opt/local/sbin:/usr/local/bin/:/usr/local/sbin/:$PATH"

# Fixes bz2 unpack issues
function tar() {
  /usr/bin/tar "$@"
}
export -f tar

pushd ${DST};
chmod +x build.sh

if (( $IOS )); then
  # Do NOT mess with our SDKs
  unset SDKROOT
  unset IPHONEOS_DEPLOYMENT_TARGET
  unset MACOSX_DEPLOYMENT_TARGET

  ret=0
  ./build.sh -i -a ${BLD_ARCH} -m ${VERMIN} onscrlib || ret=1
else
  ret=0
  ./build.sh -a ${BLD_ARCH} -m ${VERMIN} onscrlib || ret=1
fi

if (( $ret )); then
  exit 1
fi

popd
