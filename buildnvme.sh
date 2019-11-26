#!/bin/bash
cd edk2 || exit 1
source edksetup.sh
build -a X64 -b RELEASE -t XCODE5 -p NvmExpressDxePkg/NvmExpressDxePkg.dsc
cd ..

if [[ -d "$(pwd)"/edk2/Build/NvmExpressDxe/RELEASE_XCODE5/X64 ]]; then
  open "$(pwd)"/edk2/Build/NvmExpressDxe/RELEASE_XCODE5/X64
else
  echo && echo "Directory not found."
  sleep 2
fi
