#!/bin/bash
cd edk2 || exit 1
source edksetup.sh
build -a X64 -b RELEASE -t XCODE5 -p FatPkg/FatPkg.dsc
cd ..

if [[ -d "$(pwd)"/edk2/Build/Fat/RELEASE_XCODE5/X64 ]]; then
  open "$(pwd)"/edk2/Build/Fat/RELEASE_XCODE5/X64
else
  echo && echo "Directory not found."
  sleep 2
fi
