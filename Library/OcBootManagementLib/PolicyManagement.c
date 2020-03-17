/** @file
  Copyright (C) 2019, vit9696. All rights reserved.

  All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include "BootManagementInternal.h"

#include <Guid/AppleApfsInfo.h>
#include <Guid/AppleBless.h>
#include <Guid/AppleHfsInfo.h>
#include <Guid/AppleVariable.h>
#include <Guid/Gpt.h>
#include <Guid/FileInfo.h>
#include <Guid/GlobalVariable.h>
#include <Guid/OcVariables.h>

#include <Protocol/AppleBootPolicy.h>
#include <Protocol/ApfsEfiBootRecordInfo.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/SimpleTextOut.h>
#include <Protocol/LoadedImage.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/OcBootManagementLib.h>
#include <Library/OcDevicePathLib.h>
#include <Library/OcFileLib.h>
#include <Library/OcStringLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

UINT32
OcGetDevicePolicyType (
  IN  EFI_HANDLE   Handle,
  OUT BOOLEAN      *External  OPTIONAL
  )
{
  EFI_STATUS                Status;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  UINT8                     SubType;

  if (External != NULL) {
    *External = FALSE;
  }

  Status = gBS->HandleProtocol (Handle, &gEfiDevicePathProtocolGuid, (VOID **) &DevicePath);
  if (EFI_ERROR (Status)) {
    return 0;
  }

  //
  // FIXME: This code does not take care of RamDisk exception as specified per
  // https://github.com/acidanthera/bugtracker/issues/335.
  // Currently we do not need it, but in future we may.
  //

  while (!IsDevicePathEnd (DevicePath)) {
    if (DevicePathType (DevicePath) == MESSAGING_DEVICE_PATH) {
      SubType = DevicePathSubType (DevicePath);
      switch (SubType) {
        case MSG_SATA_DP:
          return OC_SCAN_ALLOW_DEVICE_SATA;
        case MSG_SASEX_DP:
          return OC_SCAN_ALLOW_DEVICE_SASEX;
        case MSG_SCSI_DP:
          return OC_SCAN_ALLOW_DEVICE_SCSI;
        case MSG_NVME_NAMESPACE_DP:
          return OC_SCAN_ALLOW_DEVICE_NVME;
        case MSG_ATAPI_DP:
          if (External != NULL) {
            *External = TRUE;
          }
          return OC_SCAN_ALLOW_DEVICE_ATAPI;
        case MSG_USB_DP:
          if (External != NULL) {
            *External = TRUE;
          }
          return OC_SCAN_ALLOW_DEVICE_USB;
        case MSG_1394_DP:
          if (External != NULL) {
            *External = TRUE;
          }
          return OC_SCAN_ALLOW_DEVICE_FIREWIRE;
        case MSG_SD_DP:
        case MSG_EMMC_DP:
          if (External != NULL) {
            *External = TRUE;
          }
          return OC_SCAN_ALLOW_DEVICE_SDCARD;
      }

      //
      // We do not have good protection against device tunneling.
      // These things must be considered:
      // - Thunderbolt 2 PCI-e pass-through
      // - Thunderbolt 3 PCI-e pass-through (Type-C, may be different from 2)
      // - FireWire devices
      // For now we hope that first messaging type protects us, and all
      // subsequent messaging types are tunneled.
      //

      break;
    }

    DevicePath = NextDevicePathNode (DevicePath);
  }

  return 0;
}

/**
  Microsoft partitions.
  https://docs.microsoft.com/ru-ru/windows/win32/api/vds/ns-vds-create_partition_parameters
**/
EFI_GUID mMsftBasicDataPartitionTypeGuid = {
  0xEBD0A0A2, 0xB9E5, 0x4433, {0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7}
};

EFI_GUID mMsftReservedPartitionTypeGuid = {
  0xE3C9E316, 0x0B5C, 0x4DB8, {0x81, 0x7D, 0xF9, 0x2D, 0xF0, 0x02, 0x15, 0xAE}
};

EFI_GUID mMsftRecoveryPartitionTypeGuid = {
  0xDE94BBA4, 0x06D1, 0x4D40, {0xA1, 0x6A, 0xBF, 0xD5, 0x01, 0x79, 0xD6, 0xAC}
};

/**
  Linux partitions.
  https://en.wikipedia.org/wiki/GUID_Partition_Table#Partition_type_GUIDs
**/
EFI_GUID mLinuxRootX86PartitionTypeGuid = {
  0x44479540, 0xF297, 0x41B2, {0x9A, 0xF7, 0xD1, 0x31, 0xD5, 0xF0, 0x45, 0x8A}
};

EFI_GUID mLinuxRootX8664PartitionTypeGuid = {
  0x4F68BCE3, 0xE8CD, 0x4DB1, {0x96, 0xE7, 0xFB, 0xCA, 0xF9, 0x84, 0xB7, 0x09}
};

UINT32
OcGetFileSystemPolicyType (
  IN  EFI_HANDLE   Handle
  )
{
  CONST EFI_PARTITION_ENTRY  *PartitionEntry;

  PartitionEntry = OcGetGptPartitionEntry (Handle);

  if (PartitionEntry == NULL) {
    DEBUG ((DEBUG_INFO, "OCB: Missing partition info for %p\n", Handle));
    return 0;
  }

  if (CompareGuid (&PartitionEntry->PartitionTypeGUID, &gAppleApfsPartitionTypeGuid)) {
    return OC_SCAN_ALLOW_FS_APFS;
  }

  if (CompareGuid (&PartitionEntry->PartitionTypeGUID, &gEfiPartTypeSystemPartGuid)) {
    return OC_SCAN_ALLOW_FS_ESP;
  }

  //
  // Unsure whether these two should be separate, likely not.
  //
  if (CompareGuid (&PartitionEntry->PartitionTypeGUID, &gAppleHfsPartitionTypeGuid)
    || CompareGuid (&PartitionEntry->PartitionTypeGUID, &gAppleHfsBootPartitionTypeGuid)) {
    return OC_SCAN_ALLOW_FS_HFS;
  }

  if (CompareGuid (&PartitionEntry->PartitionTypeGUID, &mMsftBasicDataPartitionTypeGuid)) {
    return OC_SCAN_ALLOW_FS_NTFS;
  }

  if (CompareGuid (&PartitionEntry->PartitionTypeGUID, &mLinuxRootX86PartitionTypeGuid)
    || CompareGuid (&PartitionEntry->PartitionTypeGUID, &mLinuxRootX8664PartitionTypeGuid)) {
    return OC_SCAN_ALLOW_FS_EXT;
  }

  return 0;
}

RETURN_STATUS
InternalCheckScanPolicy (
  IN  EFI_HANDLE                       Handle,
  IN  UINT32                           Policy,
  OUT BOOLEAN                          *External OPTIONAL
  )
{
  UINT32                     DevicePolicy;
  UINT32                     FileSystemPolicy;

  //
  // Always request policy type due to external checks.
  //
  DevicePolicy = OcGetDevicePolicyType (Handle, External);
  if ((Policy & OC_SCAN_DEVICE_LOCK) != 0 && (Policy & DevicePolicy) == 0) {
    DEBUG ((DEBUG_INFO, "OCB: Invalid device policy (%x/%x) for %p\n", DevicePolicy, Policy, Handle));
    return EFI_SECURITY_VIOLATION;
  }

  if ((Policy & OC_SCAN_FILE_SYSTEM_LOCK) != 0) {
    FileSystemPolicy = OcGetFileSystemPolicyType (Handle);

    if ((Policy & FileSystemPolicy) == 0) {
      DEBUG ((DEBUG_INFO, "OCB: Invalid file system policy (%x/%x) for %p\n", FileSystemPolicy, Policy, Handle));
      return EFI_SECURITY_VIOLATION;
    }
  }

  return RETURN_SUCCESS;
}

OC_BOOT_ENTRY_TYPE
OcGetBootDevicePathType (
  IN  EFI_DEVICE_PATH_PROTOCOL  *DevicePath,
  OUT BOOLEAN                   *IsFolder  OPTIONAL
  )
{
  EFI_DEVICE_PATH_PROTOCOL    *CurrNode;
  FILEPATH_DEVICE_PATH        *LastNode;
  UINTN                       PathLen;
  UINTN                       RestLen;
  UINTN                       Index;
  INTN                        CmpResult;
  OC_BOOT_ENTRY_TYPE          Type;
  BOOLEAN                     Folder;
  BOOLEAN                     Overflowed;

  LastNode = NULL;
  Type     = OC_BOOT_UNKNOWN;
  Folder   = FALSE;

  for (CurrNode = DevicePath; !IsDevicePathEnd (CurrNode); CurrNode = NextDevicePathNode (CurrNode)) {
    if ((DevicePathType (CurrNode) == MEDIA_DEVICE_PATH)
     && (DevicePathSubType (CurrNode) == MEDIA_FILEPATH_DP)) {
      LastNode = (FILEPATH_DEVICE_PATH *) CurrNode;
      PathLen  = OcFileDevicePathNameLen (LastNode);
      if (PathLen == 0) {
        continue;
      }

      //
      // Only the trailer of the last (non-empty) FilePath node matters.
      //
      Folder = (LastNode->PathName[PathLen - 1] == L'\\');

      //
      // Detect macOS recovery by com.apple.recovery.boot in the bootloader path.
      //
      if (Type == OC_BOOT_UNKNOWN) {
        Overflowed = OcOverflowSubUN (PathLen, L_STR_LEN (L"com.apple.recovery.boot"), &RestLen);
        if (Overflowed) {
          continue;
        }

        for (Index = 0; Index < RestLen; ++Index) {
          CmpResult = CompareMem (
            &LastNode->PathName[Index],
            L"com.apple.recovery.boot",
            L_STR_SIZE_NT (L"com.apple.recovery.boot")
            );
          if (CmpResult == 0) {
            Type = OC_BOOT_APPLE_RECOVERY;
            break;
          }
        }
      }
    } else {
      Folder = FALSE;
    }
  }

  if (IsFolder != NULL) {
    *IsFolder = Folder;
  }

  if (LastNode == NULL || Type != OC_BOOT_UNKNOWN) {
    return Type;
  }

  //
  // Detect macOS by boot.efi in the bootloader name.
  // Detect macOS Time Machine by tmbootpicker.efi in the bootloader name.
  //
  STATIC CONST CHAR16 *Bootloaders[] = {
    L"boot.efi",
    L"tmbootpicker.efi"
  };

  STATIC CONST UINTN BootloaderLengths[] = {
    L_STR_LEN (L"boot.efi"),
    L_STR_LEN (L"tmbootpicker.efi")
  };

  STATIC CONST OC_BOOT_ENTRY_TYPE BootloaderTypes[] = {
    OC_BOOT_APPLE_OS,
    OC_BOOT_APPLE_TIME_MACHINE
  };

  for (Index = 0; Index < ARRAY_SIZE (Bootloaders); ++Index) {
    if (PathLen < BootloaderLengths[Index]) {
      continue;
    }

    RestLen = PathLen - BootloaderLengths[Index];
    if ((RestLen == 0 || LastNode->PathName[RestLen - 1] == L'\\')
      && CompareMem (
        &LastNode->PathName[RestLen],
        Bootloaders[Index],
        BootloaderLengths[Index] * sizeof (LastNode->PathName[0])) == 0) {
      return BootloaderTypes[Index];
    }
  }

  return OC_BOOT_UNKNOWN;
}

EFI_LOADED_IMAGE_PROTOCOL *
OcGetAppleBootLoadedImage (
  IN EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS                  Status;
  EFI_LOADED_IMAGE_PROTOCOL   *LoadedImage;

  Status = gBS->HandleProtocol (
    ImageHandle,
    &gEfiLoadedImageProtocolGuid,
    (VOID **)&LoadedImage
    );

  if (!EFI_ERROR (Status)
    && LoadedImage->FilePath != NULL
    && (OcGetBootDevicePathType (LoadedImage->FilePath, NULL) & OC_BOOT_APPLE_ANY) != 0) {
    return LoadedImage;
  }

  return NULL;
}
