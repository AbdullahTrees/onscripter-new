#!/bin/bash

#
# fatbuild.sh
# ONScripter-RU
#
# macOS universal file generation.
# Run with "x86_64/executable" ["arm64/executable"] "target/executable" action.
#
# Consult LICENSE file for licensing terms and copyright holders.
#

if (( $# != 3 && $# != 4 )); then
  echo "Usage: x86_64/executable [arm64/executable] target/executable action"
  exit 1
fi

executable64="${1}"
if (( $# == 4 )); then
  executablearm64="${2}"
  executabledst="${3}"
  action="${4}"
else
  executablearm64=""
  executabledst="${2}"
  action="${3}"
fi

if [ "$action" == "clean" ]; then
	exit 0
fi

echo "EXE64:    ${executable64}"
echo "EXEARM64: ${executablearm64}"
echo "DST:      ${executabledst}"

if [ ! -x "${executable64}" ] || [ ! -x "${executabledst}" ]; then
  echo "Missing dependent app for packaging!"
  exit 1
fi

if [ "${executablearm64}" == "" ]; then
  cp "${executable64}" "${executabledst}" || exit 1
  exit 0
fi

if [ ! -x "${executablearm64}" ]; then
  echo "Missing arm64 app for universal packaging!"
  exit 1
fi

lipo -create "${executable64}" "${executablearm64}" -output "${executabledst}" || exit 1

exit 0
