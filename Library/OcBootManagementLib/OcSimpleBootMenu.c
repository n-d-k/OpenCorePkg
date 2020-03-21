/** @file
  Copyright (C) 2019. All rights reserved.

  All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include <OcSimpleBootMenuInternal.h>
#include <FontData.h>

STATIC
BOOLEAN
mAllowSetDefault;

STATIC
UINTN
mDefaultEntry;

STATIC
INTN
mCurrentSelection;

STATIC
INTN
mMenuIconsCount;

/*========= Pointer Setting ============*/

POINTERS mPointer = {NULL, NULL, NULL, NULL,
{0, 0, POINTER_WIDTH, POINTER_HEIGHT},
{0, 0, POINTER_WIDTH, POINTER_HEIGHT}, 0,
{0, 0, 0, FALSE, FALSE}, NoEvents};

STATIC
INTN
mPointerSpeed = 6;

STATIC
UINT64
mDoubleClickTime = 500;

/*========== Graphic UI Setting ==========*/

STATIC
EFI_GRAPHICS_OUTPUT_PROTOCOL *
mGraphicsOutput;

STATIC
EFI_UGA_DRAW_PROTOCOL *
mUgaDraw;

STATIC
INTN
mScreenWidth;

STATIC
INTN
mScreenHeight;

STATIC
INTN
mFontWidth = 8;

STATIC
INTN
mFontHeight = 18;

STATIC
INTN
mTextHeight = 19;

STATIC
INTN
mTextScale = 0;  // not actual scale, will be set after getting screen resolution. (16 will be no scaling, 28 will be for 4k screen)

STATIC
INTN
mUiScale = 0;

STATIC
UINTN
mIconSpaceSize;  // Default 144/288 pixels space to contain icons with size 128x128/256x256

STATIC
UINTN
mIconPaddingSize;

STATIC
EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *
mFileSystem = NULL;

NDK_UI_IMAGE *
mFontImage = NULL;

BOOLEAN
mProportional = TRUE;

BOOLEAN
mDarkMode = TRUE;

STATIC
NDK_UI_IMAGE *
mBackgroundImage = NULL;

STATIC
NDK_UI_IMAGE *
mMenuImage = NULL;

STATIC
NDK_UI_IMAGE *
mSelectionImage = NULL;

STATIC
NDK_UI_IMAGE *
mLabelImage = NULL;

BOOLEAN
mPrintLabel = TRUE;

BOOLEAN
mSelectorUsed = TRUE;

BOOLEAN
mAlphaEffect = TRUE;

/*=========== Default colors settings ==============*/

EFI_GRAPHICS_OUTPUT_BLT_PIXEL mTransparentPixel  = {0x00, 0x00, 0x00, 0x00};
EFI_GRAPHICS_OUTPUT_BLT_PIXEL mBluePixel  = {0x7f, 0x0f, 0x0f, 0xff};
EFI_GRAPHICS_OUTPUT_BLT_PIXEL mBlackPixel  = {0x00, 0x00, 0x00, 0xff};
EFI_GRAPHICS_OUTPUT_BLT_PIXEL mLowWhitePixel  = {0xb8, 0xbd, 0xbf, 0xff};
EFI_GRAPHICS_OUTPUT_BLT_PIXEL mGrayPixel  = {0xaa, 0xaa, 0xaa, 0xff};

// Selection and Entry's description font color
EFI_GRAPHICS_OUTPUT_BLT_PIXEL *mFontColorPixel = &mLowWhitePixel;

// Background color
EFI_GRAPHICS_OUTPUT_BLT_PIXEL *mBackgroundPixel = &mBlackPixel;

/*=============== Functions =============*/

STATIC
BOOLEAN
FileExist (
  IN CHAR16                        *FilePath
  )
{
  EFI_STATUS                       Status;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *FileSystem;
  VOID                             *Buffer;
  UINT32                           BufferSize;
  EFI_HANDLE                       *Handles;
  UINTN                            HandleCount;
  UINTN                            Index;

  BufferSize = 0;
  HandleCount = 0;
  FileSystem = NULL;
  Buffer = NULL;
  
  if (mFileSystem == NULL) {
    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiPartTypeSystemPartGuid, NULL, &HandleCount, &Handles);
    if (!EFI_ERROR (Status) && HandleCount > 0) {
      for (Index = 0; Index < HandleCount; ++Index) {
        Status = gBS->HandleProtocol (
                        Handles[Index],
                        &gEfiSimpleFileSystemProtocolGuid,
                        (VOID **) &FileSystem
                        );
        if (EFI_ERROR (Status)) {
          FileSystem = NULL;
          continue;
        }
        
        Buffer = ReadFile (FileSystem, FilePath, &BufferSize, BASE_16MB);
        if (Buffer != NULL) {
          mFileSystem = FileSystem;
          DEBUG ((DEBUG_INFO, "OCUI: FileSystem found!  Handle(%d) \n", Index));
          break;
        }
        FileSystem = NULL;
      }
      
      if (Handles != NULL) {
        FreePool (Handles);
      }
    }
    
  } else {
    Buffer = ReadFile (mFileSystem, FilePath, &BufferSize, BASE_16MB);
  }
  
  if (Buffer != NULL) {
    FreePool (Buffer);
    return TRUE;
  }
  return FALSE;
}

VOID
DrawImageArea (
  IN NDK_UI_IMAGE      *Image,
  IN INTN              AreaXpos,
  IN INTN              AreaYpos,
  IN INTN              AreaWidth,
  IN INTN              AreaHeight,
  IN INTN              ScreenXpos,
  IN INTN              ScreenYpos
  )
{
  EFI_STATUS           Status;
  
  if (Image == NULL) {
    return;
  }
  
  if (ScreenXpos < 0 || ScreenXpos >= mScreenWidth || ScreenYpos < 0 || ScreenYpos >= mScreenHeight) {
    DEBUG ((DEBUG_INFO, "OCUI: Invalid Screen coordinate requested...x:%d - y:%d \n", ScreenXpos, ScreenYpos));
    return;
  }
  
  if (AreaWidth == 0) {
    AreaWidth = Image->Width;
  }
  
  if (AreaHeight == 0) {
    AreaHeight = Image->Height;
  }
  
  if ((AreaXpos != 0) || (AreaYpos != 0)) {
    RestrictImageArea (Image, AreaXpos, AreaYpos, &AreaWidth, &AreaHeight);
    if (AreaWidth == 0) {
      DEBUG ((DEBUG_INFO, "OCUI: invalid area position requested\n"));
      return;
    }
  }
  
  if (ScreenXpos + AreaWidth > mScreenWidth) {
    AreaWidth = mScreenWidth - ScreenXpos;
  }
  
  if (ScreenYpos + AreaHeight > mScreenHeight) {
    AreaHeight = mScreenHeight - ScreenYpos;
  }
  
  if (mGraphicsOutput != NULL) {
    Status = mGraphicsOutput->Blt(mGraphicsOutput,
                                  Image->Bitmap,
                                  EfiBltBufferToVideo,
                                  (UINTN) AreaXpos,
                                  (UINTN) AreaYpos,
                                  (UINTN) ScreenXpos,
                                  (UINTN) ScreenYpos,
                                  (UINTN) AreaWidth,
                                  (UINTN) AreaHeight,
                                  (UINTN) Image->Width * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)
                                  );
  } else {
    ASSERT (mUgaDraw != NULL);
    Status = mUgaDraw->Blt(mUgaDraw,
                            (EFI_UGA_PIXEL *) Image->Bitmap,
                            EfiUgaBltBufferToVideo,
                            (UINTN) AreaXpos,
                            (UINTN) AreaYpos,
                            (UINTN) ScreenXpos,
                            (UINTN) ScreenYpos,
                            (UINTN) AreaWidth,
                            (UINTN) AreaHeight,
                            (UINTN) Image->Width * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)
                            );
  }
  
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "OCUI: Draw Image Area...%r\n", Status));
  }
}

STATIC
VOID
TakeImage (
  IN NDK_UI_IMAGE      *Image,
  IN INTN              ScreenXpos,
  IN INTN              ScreenYpos,
  IN INTN              AreaWidth,
  IN INTN              AreaHeight
  )
{
  EFI_STATUS           Status;
  
  if (ScreenXpos + AreaWidth > mScreenWidth) {
    AreaWidth = mScreenWidth - ScreenXpos;
  }
  
  if (ScreenYpos + AreaHeight > mScreenHeight) {
    AreaHeight = mScreenHeight - ScreenYpos;
  }
    
  if (mGraphicsOutput != NULL) {
    Status = mGraphicsOutput->Blt(mGraphicsOutput,
                                  (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *) Image->Bitmap,
                                  EfiBltVideoToBltBuffer,
                                  ScreenXpos,
                                  ScreenYpos,
                                  0,
                                  0,
                                  AreaWidth,
                                  AreaHeight,
                                  (UINTN) Image->Width * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)
                                  );
  } else {
    ASSERT (mUgaDraw != NULL);
    Status = mUgaDraw->Blt(mUgaDraw,
                           (EFI_UGA_PIXEL *) Image->Bitmap,
                           EfiUgaVideoToBltBuffer,
                           ScreenXpos,
                           ScreenYpos,
                           0,
                           0,
                           AreaWidth,
                           AreaHeight,
                           (UINTN) Image->Width * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)
                           );
  }
}

STATIC
VOID
CreateMenuImage (
  IN NDK_UI_IMAGE        *Icon,
  IN UINTN               IconCount
  )
{
  NDK_UI_IMAGE           *NewImage;
  UINT16                 Width;
  UINT16                 Height;
  BOOLEAN                IsTwoRow;
  UINTN                  IconsPerRow;
  INTN                   Xpos;
  INTN                   Ypos;
  INTN                   Offset;
  INTN                   IconRowSpace;
  
  NewImage = NULL;
  Xpos = 0;
  Ypos = 0;
  IconRowSpace = (32 * mUiScale >> 4) + 10;
  
  if (mMenuImage != NULL) {
    Width = mMenuImage->Width;
    Height = mMenuImage->Height;
    IsTwoRow = mMenuImage->Height > mIconSpaceSize + IconRowSpace;
    
    if (IsTwoRow) {
      IconsPerRow = mMenuImage->Width / mIconSpaceSize;
      Xpos = (IconCount - IconsPerRow) * mIconSpaceSize;
      Ypos = mIconSpaceSize + IconRowSpace;
    } else {
      if (mMenuImage->Width + (mIconSpaceSize * 2) <= mScreenWidth) {
        Width = mMenuImage->Width + mIconSpaceSize;
        Xpos = mMenuImage->Width;
      } else {
        Height = mMenuImage->Height + mIconSpaceSize + IconRowSpace;
        Ypos = mIconSpaceSize + IconRowSpace;
      }
    }
  } else {
    Width = mIconSpaceSize;
    Height = Width;
  }
  
  NewImage = CreateFilledImage (Width, Height, TRUE, &mTransparentPixel);
  if (NewImage == NULL) {
    return;
  }
  
  if (mMenuImage != NULL) {
    ComposeImage (NewImage, mMenuImage, 0, 0);
    if (mMenuImage != NULL) {
      FreeImage (mMenuImage);
    }
  }
  
  Offset = (mIconSpaceSize - (Icon->Width + (mIconPaddingSize * 2))) > 0 ? (mIconSpaceSize - (Icon->Width + (mIconPaddingSize * 2))) / 2 : 0;
  
  ComposeImage (NewImage, Icon, Xpos + mIconPaddingSize + Offset, Ypos + mIconPaddingSize + Offset);
  if (Icon != NULL) {
    FreeImage (Icon);
  }
  
  mMenuImage = NewImage;
}

STATIC
VOID
BltImageAlpha (
  IN NDK_UI_IMAGE                  *Image,
  IN INTN                          Xpos,
  IN INTN                          Ypos,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BackgroundPixel,
  IN INTN                          Scale
  )
{
  NDK_UI_IMAGE        *CompImage;
  NDK_UI_IMAGE        *NewImage;
  INTN                Width;
  INTN                Height;
  
  NewImage = NULL;
  Width    = Scale << 3;
  Height   = Width;

  if (Image != NULL) {
    NewImage = CopyScaledImage (Image, Scale);
    Width = NewImage->Width;
    Height = NewImage->Height;
  }

  CompImage = CreateFilledImage (Width, Height, (mBackgroundImage != NULL), BackgroundPixel);
  ComposeImage (CompImage, NewImage, 0, 0);
  if (NewImage != NULL) {
    FreeImage (NewImage);
  }
  if (mBackgroundImage == NULL) {
    DrawImageArea (CompImage, 0, 0, 0, 0, Xpos, Ypos);
    FreeImage (CompImage);
    return;
  }
  
  // Background Image was used.
  NewImage = CreateImage (Width, Height, FALSE);
  if (NewImage == NULL) {
    return;
  }
  RawCopy (NewImage->Bitmap,
           mBackgroundImage->Bitmap + Ypos * mBackgroundImage->Width + Xpos,
           Width,
           Height,
           Width,
           mBackgroundImage->Width
           );
  // Compose
  ComposeImage (NewImage, CompImage, 0, 0);
  FreeImage (CompImage);
  // Draw to screen
  DrawImageArea (NewImage, 0, 0, 0, 0, Xpos, Ypos);
  FreeImage (NewImage);
}

STATIC
VOID
BltMenuImage (
  IN NDK_UI_IMAGE        *Image,
  IN INTN                Xpos,
  IN INTN                Ypos
  )
{
  NDK_UI_IMAGE           *NewImage;
  
  if (Image == NULL) {
    return;
  }
  
  NewImage = CreateImage (Image->Width, Image->Height, FALSE);
  if (NewImage == NULL) {
    return;
  }
  
  RawCopy (NewImage->Bitmap,
           mBackgroundImage->Bitmap + Ypos * mBackgroundImage->Width + Xpos,
           Image->Width,
           Image->Height,
           Image->Width,
           mBackgroundImage->Width
           );
  
  RawComposeAlpha (NewImage->Bitmap,
                   Image->Bitmap,
                   NewImage->Width,
                   NewImage->Height,
                   NewImage->Width,
                   Image->Width,
                   mAlphaEffect ? ICON_OPACITY_LEVEL : ICON_OPACITY_FULL
                   );
  
  DrawImageArea (NewImage, 0, 0, 0, 0, Xpos, Ypos);
  FreeImage (NewImage);
}

STATIC
NDK_UI_IMAGE *
DecodePNGFile (
  IN CHAR16                        *FilePath
  )
{
  EFI_STATUS                       Status;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *FileSystem;
  VOID                             *Buffer;
  UINT32                           BufferSize;
  EFI_HANDLE                       *Handles;
  UINTN                            HandleCount;
  UINTN                            Index;

  BufferSize = 0;
  HandleCount = 0;
  FileSystem = NULL;
  Buffer = NULL;
  
  if (mFileSystem == NULL) {
    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiPartTypeSystemPartGuid, NULL, &HandleCount, &Handles);
    if (!EFI_ERROR (Status) && HandleCount > 0) {
      for (Index = 0; Index < HandleCount; ++Index) {
        Status = gBS->HandleProtocol (
                        Handles[Index],
                        &gEfiSimpleFileSystemProtocolGuid,
                        (VOID **) &FileSystem
                        );
        if (EFI_ERROR (Status)) {
          FileSystem = NULL;
          continue;
        }
        
        Buffer = ReadFile (FileSystem, FilePath, &BufferSize, BASE_16MB);
        if (Buffer != NULL) {
          mFileSystem = FileSystem;
          DEBUG ((DEBUG_INFO, "OCUI: FileSystem found!  Handle(%d) \n", Index));
          break;
        }
        FileSystem = NULL;
      }
      
      if (Handles != NULL) {
        FreePool (Handles);
      }
    }
    
  } else {
    Buffer = ReadFile (mFileSystem, FilePath, &BufferSize, BASE_16MB);
  }
  
  if (Buffer == NULL) {
    DEBUG ((DEBUG_ERROR, "OCUI: Failed to locate %s file\n", FilePath));
    return NULL;
  }
  return DecodePNG (Buffer, BufferSize);
}

STATIC
VOID
TakeScreenShot (
  IN CHAR16              *FilePath
  )
{
  EFI_STATUS              Status;
  EFI_FILE_PROTOCOL       *Fs;
  EFI_TIME                Date;
  NDK_UI_IMAGE            *Image;
  EFI_UGA_PIXEL           *ImagePNG;
  VOID                    *Buffer;
  UINTN                   BufferSize;
  UINTN                   Index;
  UINTN                   ImageSize;
  CHAR16                  *Path;
  UINTN                   Size;
  
  Buffer     = NULL;
  BufferSize = 0;
  
  Status = gRT->GetTime (&Date, NULL);
  if (EFI_ERROR (Status)) {
    ZeroMem (&Date, sizeof (Date));
  }
  
  Size = StrSize (FilePath) + L_STR_SIZE (L"-0000-00-00-000000.png");
  Path = AllocatePool (Size);
  UnicodeSPrint (Path,
                 Size,
                 L"%s-%04u-%02u-%02u-%02u%02u%02u.png",
                 FilePath,
                 (UINT32) Date.Year,
                 (UINT32) Date.Month,
                 (UINT32) Date.Day,
                 (UINT32) Date.Hour,
                 (UINT32) Date.Minute,
                 (UINT32) Date.Second
  );
  
  Image = CreateImage (mScreenWidth, mScreenHeight, FALSE);
  if (Image == NULL) {
    DEBUG ((DEBUG_INFO, "Failed to take screen shot!\n"));
    return;
  }
    
  TakeImage (Image, 0, 0, mScreenWidth, mScreenHeight);
  
  ImagePNG = (EFI_UGA_PIXEL *) Image->Bitmap;
  ImageSize = Image->Width * Image->Height;
  
  // Convert BGR to RGBA with Alpha set to 0xFF
  for (Index = 0; Index < ImageSize; ++Index) {
      UINT8 Temp = ImagePNG[Index].Blue;
      ImagePNG[Index].Blue = ImagePNG[Index].Red;
      ImagePNG[Index].Red = Temp;
      ImagePNG[Index].Reserved = 0xFF;
  }

  // Encode raw RGB image to PNG format
  Status = EncodePng (ImagePNG,
                      (UINTN) Image->Width,
                      (UINTN) Image->Height,
                      &Buffer,
                      &BufferSize
                      );
  FreeImage (Image);
  if (Buffer == NULL) {
    DEBUG ((DEBUG_INFO, "OCUI: Fail Encoding!\n"));
    return;
  }
  
  Status = mFileSystem->OpenVolume (mFileSystem, &Fs);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "OCUI: Locating Writeable file system - %r\n", Status));
    return;
  }
  
  Status = SetFileData (Fs, Path, Buffer, (UINT32) BufferSize);
  DEBUG ((DEBUG_INFO, "OCUI: Screenshot was taken - %r\n", Status));
  if (Buffer != NULL) {
    FreePool (Buffer);
  }
  if (Path != NULL) {
    FreePool (Path);
  }
}
//
// Saving Entries' devicepath to file (Forked function only)
//
VOID
SaveEntriesDataToFile (
  IN CHAR16            *FilePath,
  IN OC_BOOT_ENTRY     *Entries,
  IN UINTN             Count
  )
{
  EFI_STATUS                           Status;
  EFI_TIME                             Date;
  EFI_FILE_PROTOCOL                    *Fs;
  UINTN                                Index;
  CHAR8                                AsciiStr[512];
  CHAR8                                *AsciiBuffer;
  CHAR16                               *DevicePathText;
  CHAR16                               *Path;
  UINTN                                Size;
  
  DevicePathText = NULL;
  AsciiBuffer = AllocateZeroPool (EFI_PAGE_SIZE * Count);
  
  if (AsciiBuffer != NULL) {
    AsciiSPrint (AsciiStr, sizeof (AsciiStr), "====== Boot Entries Summary ======\n\n");
    AsciiStrCatS (AsciiBuffer, EFI_PAGE_SIZE, AsciiStr);
    
    for (Index = 0; Index < Count; ++Index) {
      if (Entries[Index].Type == OC_BOOT_SYSTEM || Entries[Index].DevicePath == NULL) {
        continue;
      }
      
      AsciiSPrint (AsciiStr, sizeof (AsciiStr), "Entry name: %s\n",
                   Entries[Index].Name
                   );
      
      AsciiStrCatS (AsciiBuffer, EFI_PAGE_SIZE, AsciiStr);
      DevicePathText = ConvertDevicePathToText (Entries[Index].DevicePath, FALSE, FALSE);
      AsciiSPrint (AsciiStr, sizeof (AsciiStr), "DevicePath: %s\n\n", DevicePathText);
      AsciiStrCatS (AsciiBuffer, EFI_PAGE_SIZE, AsciiStr);
      FreePool (DevicePathText);
      DevicePathText = NULL;
    }

    Status = gRT->GetTime (&Date, NULL);
    if (EFI_ERROR (Status)) {
      ZeroMem (&Date, sizeof (Date));
    }
    
    AsciiSPrint (AsciiStr, sizeof (AsciiStr), "========= End =========\n\n");
    AsciiStrCatS (AsciiBuffer, EFI_PAGE_SIZE, AsciiStr);

    Size = StrSize (FilePath) + L_STR_SIZE (L"-0000-00-00-000000.txt");
    Path = AllocatePool (Size);
    UnicodeSPrint (Path,
                   Size,
                   L"%s-%04u-%02u-%02u-%02u%02u%02u.txt",
                   FilePath,
                   (UINT32) Date.Year,
                   (UINT32) Date.Month,
                   (UINT32) Date.Day,
                   (UINT32) Date.Hour,
                   (UINT32) Date.Minute,
                   (UINT32) Date.Second
    );
    
    Status = mFileSystem->OpenVolume (mFileSystem, &Fs);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "OCUI: Locating Writeable file system - %r\n", Status));
    }

    Status = SetFileData (Fs, Path, AsciiBuffer, AsciiStrLen (AsciiBuffer));
    DEBUG ((DEBUG_INFO, "OCUI: Saving boot entries data to file - %r\n", Status));

    if (AsciiBuffer != NULL) {
      FreePool (AsciiBuffer);
    }
    if (Path != NULL) {
      FreePool (Path);
    }
  } else {
    DEBUG ((DEBUG_WARN, "OCUI: Cannot allocate memory for buffer\n"));
  }
}

STATIC
VOID
CreateIcon (
  IN CHAR16               *Name,
  IN OC_BOOT_ENTRY_TYPE   Type,
  IN UINTN                IconCount,
  IN BOOLEAN              Ext,
  IN BOOLEAN              Dmg
  )
{
  CHAR16                 *FilePath;
  NDK_UI_IMAGE           *Icon;
  NDK_UI_IMAGE           *ScaledImage;
  INTN                   IconScale;
  
  Icon = NULL;
  ScaledImage = NULL;
  IconScale = 16;
  
  switch (Type) {
    case OC_BOOT_WINDOWS:
      if (StrStr (Name, L"10") != NULL) {
        FilePath = L"EFI\\OC\\Icons\\os_win10.icns";
      } else {
        FilePath = L"EFI\\OC\\Icons\\os_win.icns";
      }
      break;
    case OC_BOOT_APPLE_OS:
      if (StrStr (Name, L"Install") != NULL) {
        FilePath = L"EFI\\OC\\Icons\\os_Install.icns";
      } else if (StrStr (Name, L"Cata") != NULL) {
        FilePath = L"EFI\\OC\\Icons\\os_cata.icns";
      } else if (StrStr (Name, L"Moja") != NULL) {
        FilePath = L"EFI\\OC\\Icons\\os_moja.icns";
      } else if (StrStr (Name, L"Clone") != NULL) {
        FilePath = L"EFI\\OC\\Icons\\os_clone.icns";
      } else {
        FilePath = L"EFI\\OC\\Icons\\os_mac.icns";
      }
      break;
    case OC_BOOT_APPLE_RECOVERY:
      FilePath = L"EFI\\OC\\Icons\\os_recovery.icns";
      break;
    case OC_BOOT_APPLE_TIME_MACHINE:
      FilePath = L"EFI\\OC\\Icons\\os_clone.icns";
      break;
    case OC_BOOT_EXTERNAL_OS:
      if (StrStr (Name, L"Free") != NULL) {
        FilePath = L"EFI\\OC\\Icons\\os_freebsd.icns";
      } else if (StrStr (Name, L"Linux") != NULL) {
        FilePath = L"EFI\\OC\\Icons\\os_linux.icns";
      } else if (StrStr (Name, L"Redhat") != NULL) {
        FilePath = L"EFI\\OC\\Icons\\os_redhat.icns";
      } else if (StrStr (Name, L"Ubuntu") != NULL) {
        FilePath = L"EFI\\OC\\Icons\\os_ubuntu.icns";
      } else if (StrStr (Name, L"Fedora") != NULL) {
        FilePath = L"EFI\\OC\\Icons\\os_fedora.icns";
      } else if (StrStr (Name, L"10") != NULL) {
        FilePath = L"EFI\\OC\\Icons\\os_win10.icns";
      } else if (StrStr (Name, L"Win") != NULL) {
        FilePath = L"EFI\\OC\\Icons\\os_win.icns";
      } else {
        FilePath = L"EFI\\OC\\Icons\\os_custom.icns";
      }
      break;
    case OC_BOOT_EXTERNAL_TOOL:
      if (StrStr (Name, L"Shell") != NULL) {
        FilePath = L"EFI\\OC\\Icons\\tool_shell.icns";
      } else {
          FilePath = L"EFI\\OC\\Icons\\os_custom.icns";
      }
      break;
    case OC_BOOT_APPLE_ANY:
      FilePath = L"EFI\\OC\\Icons\\os_mac.icns";
      break;
    case OC_BOOT_SYSTEM:
      FilePath = L"EFI\\OC\\Icons\\func_resetnvram.icns";
      break;
    case OC_BOOT_UNKNOWN:
      FilePath = L"EFI\\OC\\Icons\\os_unknown.icns";
      break;
      
    default:
      FilePath = L"EFI\\OC\\Icons\\os_unknown.icns";
      break;
  }
  
  if (FileExist (FilePath)) {
    Icon = DecodePNGFile (FilePath);
  } else {
    Icon = CreateFilledImage ((mIconSpaceSize - (mIconPaddingSize * 2)), (mIconSpaceSize - (mIconPaddingSize * 2)), TRUE, &mBluePixel);
  }
  
  if (Icon->Width == 256 && mScreenHeight < 2160) {
    IconScale = 8;
  }
  
  if (Icon->Width == 256 && mScreenHeight <= 800) {
    IconScale = 4;
  }
  
  if (Icon->Width > 128 && mMenuImage == NULL) {
    mIconSpaceSize = ((Icon->Width * IconScale) >> 4) + (mIconPaddingSize * 2);
    mUiScale = (mUiScale == 8) ? 8 : 16;
  }
  
  ScaledImage = CopyScaledImage (Icon, (IconScale < mUiScale) ? IconScale : mUiScale);
  FreeImage (Icon);
  CreateMenuImage (ScaledImage, IconCount);
}

STATIC
VOID
SwitchIconSelection (
  IN UINTN               IconCount,
  IN UINTN               IconIndex,
  IN BOOLEAN             Selected
  )
{
  NDK_UI_IMAGE           *NewImage;
  NDK_UI_IMAGE           *SelectorImage;
  NDK_UI_IMAGE           *Icon;
  BOOLEAN                IsTwoRow;
  INTN                   Xpos;
  INTN                   Ypos;
  INTN                   Offset;
  UINT16                 Width;
  UINT16                 Height;
  UINTN                  IconsPerRow;
  INTN                   IconRowSpace;
  
  /* Begin Calculating Xpos and Ypos of current selected icon on screen*/
  NewImage = NULL;
  Icon = NULL;
  IsTwoRow = FALSE;
  Xpos = 0;
  Ypos = 0;
  Width = mIconSpaceSize;
  Height = Width;
  IconsPerRow = 1;
  IconRowSpace = (32 * mUiScale >> 4) + 10;
  
  for (IconsPerRow = 1; IconsPerRow < IconCount; ++IconsPerRow) {
    Width = Width + mIconSpaceSize;
    if ((Width + (mIconSpaceSize * 2)) >= mScreenWidth) {
      break;
    }
  }
  
  if (IconsPerRow < IconCount) {
    IsTwoRow = TRUE;
    Height = mIconSpaceSize * 2;
    if (IconIndex <= IconsPerRow) {
      Xpos = (mScreenWidth - Width) / 2 + (mIconSpaceSize * IconIndex);
      Ypos = (mScreenHeight / 2) - mIconSpaceSize;
    } else {
      Xpos = (mScreenWidth - Width) / 2 + (mIconSpaceSize * (IconIndex - (IconsPerRow + 1)));
      Ypos = mScreenHeight / 2 + IconRowSpace;
    }
  } else {
    Xpos = (mScreenWidth - Width) / 2 + (mIconSpaceSize * IconIndex);
    Ypos = (mScreenHeight / 2) - mIconSpaceSize;
  }
  /* Done Calculating Xpos and Ypos of current selected icon on screen*/
  
  Icon = CreateImage (mIconSpaceSize - (mIconPaddingSize * 2), mIconSpaceSize - (mIconPaddingSize * 2), TRUE);
  if (Icon == NULL) {
    return;
  }
  
  RawCopy (Icon->Bitmap,
           mMenuImage->Bitmap + ((IconIndex <= IconsPerRow) ? mIconPaddingSize : mIconPaddingSize + mIconSpaceSize + IconRowSpace) * mMenuImage->Width + ((Xpos + mIconPaddingSize) - ((mScreenWidth - Width) / 2)),
           Icon->Width,
           Icon->Height,
           Icon->Width,
           mMenuImage->Width
           );
  
  if (Selected && mSelectorUsed) {
    if (mSelectionImage != NULL && (mSelectionImage->Width == 144 || mSelectionImage->Width == 288)) {
      NewImage = CreateImage (mIconSpaceSize, mIconSpaceSize, FALSE);
      
      RawCopy (NewImage->Bitmap,
               mBackgroundImage->Bitmap + Ypos * mBackgroundImage->Width + Xpos,
               mIconSpaceSize,
               mIconSpaceSize,
               mIconSpaceSize,
               mBackgroundImage->Width
               );
      
      SelectorImage = CopyScaledImage (mSelectionImage, (mSelectionImage->Width == mIconSpaceSize) ? 16 : mUiScale);
      
      Offset = (NewImage->Width - SelectorImage->Width) >> 1;
      
      RawCompose (NewImage->Bitmap + Offset * NewImage->Width + Offset,
                  SelectorImage->Bitmap,
                  SelectorImage->Width,
                  SelectorImage->Height,
                  NewImage->Width,
                  SelectorImage->Width
                  );
      
      FreeImage (SelectorImage);
    } else {
      NewImage = CreateFilledImage (mIconSpaceSize, mIconSpaceSize, FALSE, mFontColorPixel);
      RawCopy (NewImage->Bitmap + mIconPaddingSize * NewImage->Width + mIconPaddingSize,
               mBackgroundImage->Bitmap + (Ypos + mIconPaddingSize) * mBackgroundImage->Width + (Xpos + mIconPaddingSize),
               mIconSpaceSize - (mIconPaddingSize * 2),
               mIconSpaceSize - (mIconPaddingSize * 2),
               mIconSpaceSize,
               mBackgroundImage->Width
               );
    }
  } else {
    NewImage = CreateImage (mIconSpaceSize, mIconSpaceSize, FALSE);
    
    RawCopy (NewImage->Bitmap,
             mBackgroundImage->Bitmap + Ypos * mBackgroundImage->Width + Xpos,
             mIconSpaceSize,
             mIconSpaceSize,
             mIconSpaceSize,
             mBackgroundImage->Width
             );
  }
  
  RawComposeAlpha (NewImage->Bitmap + mIconPaddingSize * NewImage->Width + mIconPaddingSize,
                   Icon->Bitmap,
                   Icon->Width,
                   Icon->Height,
                   NewImage->Width,
                   Icon->Width,
                   (!Selected && mAlphaEffect) ? ICON_OPACITY_LEVEL : ICON_OPACITY_FULL
                   );
  
  FreeImage (Icon);
  BltImage (NewImage, Xpos, Ypos);
  FreeImage (NewImage);
}

VOID
ScaleBackgroundImage (
  VOID
  )
{
  NDK_UI_IMAGE                  *Image;
  INTN                          OffsetX;
  INTN                          OffsetX1;
  INTN                          OffsetX2;
  INTN                          OffsetY;
  INTN                          OffsetY1;
  INTN                          OffsetY2;
  
  Image = CopyScaledImage (mBackgroundImage, (mScreenWidth << 4) / mBackgroundImage->Width);
  FreeImage (mBackgroundImage);
  mBackgroundImage = CreateFilledImage (mScreenWidth, mScreenHeight, FALSE, &mGrayPixel);
  OffsetX = mScreenWidth - Image->Width;
  if (OffsetX >= 0) {
    OffsetX1 = OffsetX >> 1;
    OffsetX2 = 0;
    OffsetX = Image->Width;
  } else {
    OffsetX1 = 0;
    OffsetX2 = (-OffsetX) >> 1;
    OffsetX = mScreenWidth;
  }
  
  OffsetY = mScreenHeight - Image->Height;
  if (OffsetY >= 0) {
    OffsetY1 = OffsetY >> 1;
    OffsetY2 = 0;
    OffsetY = Image->Height;
  } else {
    OffsetY1 = 0;
    OffsetY2 = (-OffsetY) >> 1;
    OffsetY = mScreenHeight;
  }
  
  RawCopy (mBackgroundImage->Bitmap + OffsetY1 * mScreenWidth + OffsetX1,
           Image->Bitmap + OffsetY2 * Image->Width + OffsetX2,
           OffsetX,
           OffsetY,
           mScreenWidth,
           Image->Width
           );
  
  FreeImage (Image);
}

STATIC
VOID
ClearScreen (
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *Color
  )
{
  NDK_UI_IMAGE                  *Image;
  
  if (FileExist (L"EFI\\OC\\Icons\\Background4k.png") && mScreenHeight >= 2160) {
    mBackgroundImage = DecodePNGFile (L"EFI\\OC\\Icons\\Background4k.png");
  } else if (FileExist (L"EFI\\OC\\Icons\\Background.png")) {
    mBackgroundImage = DecodePNGFile (L"EFI\\OC\\Icons\\Background.png");
  }
  
  if (mBackgroundImage != NULL && (mBackgroundImage->Width != mScreenWidth || mBackgroundImage->Height != mScreenHeight)) {
    ScaleBackgroundImage ();
  }
  
  if (mBackgroundImage == NULL) {
    if (FileExist (L"EFI\\OC\\Icons\\background_color.png")) {
      Image = DecodePNGFile (L"EFI\\OC\\Icons\\background_color.png");
      if (Image != NULL) {
        CopyMem (mBackgroundPixel, &Image->Bitmap[0], sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
        mBackgroundPixel->Reserved = 0xff;
      } else {
        Image = DecodePNGFile (L"EFI\\OC\\Icons\\os_mac.icns");
        if (Image != NULL) {
          CopyMem (mBackgroundPixel, &Image->Bitmap[0], sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
          mBackgroundPixel->Reserved = 0xff;
        }
      }
      FreeImage (Image);
    }
    mBackgroundImage = CreateFilledImage (mScreenWidth, mScreenHeight, FALSE, mBackgroundPixel);
  }
  
  if (mBackgroundImage != NULL) {
    BltImage (mBackgroundImage, 0, 0);
  }
  
  if (FileExist (L"EFI\\OC\\Icons\\font_color.png")) {
    Image = DecodePNGFile (L"EFI\\OC\\Icons\\font_color.png");
    if (Image != NULL) {
      CopyMem (mFontColorPixel, &Image->Bitmap[0], sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
      mFontColorPixel->Reserved = 0xff;
      FreeImage (Image);
    }
  }
  
  if (FileExist (L"EFI\\OC\\Icons\\No_alpha.png")) {
    mAlphaEffect = FALSE;
  } else if (FileExist (L"EFI\\OC\\Icons\\No_selector.png")) {
    mSelectorUsed = FALSE;
  }
  
  if (mSelectorUsed && FileExist (L"EFI\\OC\\Icons\\Selector4k.png") && mScreenHeight >= 2160) {
    mSelectionImage = DecodePNGFile (L"EFI\\OC\\Icons\\Selector4k.png");
  } else if (mSelectorUsed && FileExist (L"EFI\\OC\\Icons\\Selector.png")) {
    mSelectionImage = DecodePNGFile (L"EFI\\OC\\Icons\\Selector.png");
  }
  
  if (FileExist (L"EFI\\OC\\Icons\\No_label.png")) {
    mPrintLabel = FALSE;
  }
  
  if (FileExist (L"EFI\\OC\\Icons\\Label.png")) {
    mLabelImage = DecodePNGFile (L"EFI\\OC\\Icons\\Label.png");
  } else {
    mLabelImage = CreateFilledImage (mIconSpaceSize, 32, TRUE, &mTransparentPixel);
  }
}

STATIC
VOID
ClearScreenArea (
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *Color,
  IN INTN                           Xpos,
  IN INTN                           Ypos,
  IN INTN                           Width,
  IN INTN                           Height
  )
{
  NDK_UI_IMAGE                      *Image;
  NDK_UI_IMAGE                      *NewImage;
  
  Image = CreateFilledImage (Width, Height, (mBackgroundImage != NULL), Color);
  if (mBackgroundImage == NULL) {
    DrawImageArea (Image, 0, 0, 0, 0, Xpos, Ypos);
    FreeImage (Image);
    return;
  }
  
  NewImage = CreateImage (Width, Height, FALSE);
  if (NewImage == NULL) {
    return;
  }
  RawCopy (NewImage->Bitmap,
           mBackgroundImage->Bitmap + Ypos * mBackgroundImage->Width + Xpos,
           Width,
           Height,
           Width,
           mBackgroundImage->Width
           );

  ComposeImage (NewImage, Image, 0, 0);
  FreeImage (Image);

  DrawImageArea (NewImage, 0, 0, 0, 0, Xpos, Ypos);
  FreeImage (NewImage);
}

STATIC
VOID
InitScreen (
  VOID
  )
{
  EFI_STATUS        Status;
  EFI_HANDLE        Handle;
  UINT32            ColorDepth;
  UINT32            RefreshRate;
  UINT32            ScreenWidth;
  UINT32            ScreenHeight;
  
  Handle = NULL;
  mUgaDraw = NULL;
  //
  // Try to open GOP first
  //
  if (mGraphicsOutput == NULL) {
    Status = gBS->HandleProtocol (gST->ConsoleOutHandle, &gEfiGraphicsOutputProtocolGuid, (VOID **) &mGraphicsOutput);
    if (EFI_ERROR (Status)) {
      mGraphicsOutput = NULL;
      //
      // Open GOP failed, try to open UGA
      //
      Status = gBS->HandleProtocol (gST->ConsoleOutHandle, &gEfiUgaDrawProtocolGuid, (VOID **) &mUgaDraw);
      if (EFI_ERROR (Status)) {
        mUgaDraw = NULL;
      }
    }
  }
  
  if (mGraphicsOutput != NULL) {
    Status = OcSetConsoleResolution (0, 0, 0);
    mScreenWidth = mGraphicsOutput->Mode->Info->HorizontalResolution;
    mScreenHeight = mGraphicsOutput->Mode->Info->VerticalResolution;
  } else {
    ASSERT (mUgaDraw != NULL);
    Status = mUgaDraw->GetMode (mUgaDraw, &ScreenWidth, &ScreenHeight, &ColorDepth, &RefreshRate);
    mScreenWidth = ScreenWidth;
    mScreenHeight = ScreenHeight;
  }
  DEBUG ((DEBUG_INFO, "OCUI: Initialize Graphic Screen...%r\n", Status));
  
  mTextScale = (mTextScale == 0 && mScreenHeight >= 2160 && !(FileExist (L"EFI\\OC\\Icons\\No_text_scaling.png"))) ? 32 : 16;
  if (mUiScale == 0 && mScreenHeight >= 2160 && !(FileExist (L"EFI\\OC\\Icons\\No_icon_scaling.png"))) {
    mUiScale = 32;
    mIconPaddingSize = 16;
    mIconSpaceSize = 288;
  } else if (mUiScale == 0 && mScreenHeight <= 800) {
    mUiScale = 8;
    mTextScale = 16;
    mIconPaddingSize = 3;
    mIconSpaceSize = 70;
    mPrintLabel = FALSE;
  } else {
    mUiScale = 16;
    mIconPaddingSize = 8;
    mIconSpaceSize = 144;
  }
}
//
// Text rendering
//
STATIC
NDK_UI_IMAGE *
LoadFontImage (
  IN INTN                       Cols,
  IN INTN                       Rows
  )
{
  NDK_UI_IMAGE                  *NewImage;
  NDK_UI_IMAGE                  *NewFontImage;
  INTN                          ImageWidth;
  INTN                          ImageHeight;
  INTN                          X;
  INTN                          Y;
  INTN                          Ypos;
  INTN                          J;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *PixelPtr;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL FirstPixel;
  
  NewImage = NULL;
  
  if (FileExist (L"EFI\\OC\\Icons\\font.png")) {
    NewImage = DecodePNGFile (L"EFI\\OC\\Icons\\font.png");
  } else {
    NewImage = DecodePNG ((VOID *) &emb_font_data, (UINT32) emb_font_data_size);
  }
  
  ImageWidth = NewImage->Width;
  ImageHeight = NewImage->Height;
  PixelPtr = NewImage->Bitmap;
  NewFontImage = CreateImage (ImageWidth * Rows, ImageHeight / Rows, TRUE); // need to be Alpha
  
  if (NewFontImage == NULL) {
    if (NewImage != NULL) {
      FreeImage (NewImage);
    }
    return NULL;
  }
  
  mFontWidth = ImageWidth / Cols;
  mFontHeight = ImageHeight / Rows;
  mTextHeight = mFontHeight + 1;
  FirstPixel = *PixelPtr;
  for (Y = 0; Y < Rows; ++Y) {
    for (J = 0; J < mFontHeight; J++) {
      Ypos = ((J * Rows) + Y) * ImageWidth;
      for (X = 0; X < ImageWidth; ++X) {
        if ((PixelPtr->Blue == FirstPixel.Blue)
            && (PixelPtr->Green == FirstPixel.Green)
            && (PixelPtr->Red == FirstPixel.Red)) {
          PixelPtr->Reserved = 0;
        } else if (mDarkMode) {
          *PixelPtr = *mFontColorPixel;
        }
        NewFontImage->Bitmap[Ypos + X] = *PixelPtr++;
      }
    }
  }
  
  FreeImage (NewImage);
  
  return NewFontImage;
}

STATIC
VOID
PrepareFont (
  VOID
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *PixelPtr;
  INTN                          Width;
  INTN                          Height;

  mTextHeight = mFontHeight + 1;

  if (mFontImage != NULL) {
    FreeImage (mFontImage);
    mFontImage = NULL;
  }
  
  mFontImage = LoadFontImage (16, 16);
  
  if (mFontImage != NULL) {
    if (!mDarkMode) {
      //invert the font for DarkMode
      PixelPtr = mFontImage->Bitmap;
      for (Height = 0; Height < mFontImage->Height; Height++){
        for (Width = 0; Width < mFontImage->Width; Width++, PixelPtr++){
          PixelPtr->Blue  ^= 0xFF;
          PixelPtr->Green ^= 0xFF;
          PixelPtr->Red   ^= 0xFF;
        }
      }
    }
  } else {
    DEBUG ((DEBUG_INFO, "OCUI: Failed to load font file...\n"));
  }
}


STATIC
BOOLEAN
EmptyPix (
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Ptr,
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *FirstPixel
  )
{
  //compare with first pixel of the array top-left point [0][0]
   return ((Ptr->Red >= FirstPixel->Red - (FirstPixel->Red >> 2)) && (Ptr->Red <= FirstPixel->Red + (FirstPixel->Red >> 2)) &&
           (Ptr->Green >= FirstPixel->Green - (FirstPixel->Green >> 2)) && (Ptr->Green <= FirstPixel->Green + (FirstPixel->Green >> 2)) &&
           (Ptr->Blue >= FirstPixel->Blue - (FirstPixel->Blue >> 2)) && (Ptr->Blue <= FirstPixel->Blue + (FirstPixel->Blue >> 2)) &&
           (Ptr->Reserved == FirstPixel->Reserved));
}

STATIC
INTN
GetEmpty (
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Ptr,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL *FirstPixel,
  IN INTN                          MaxWidth,
  IN INTN                          Step,
  IN INTN                          Row
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *Ptr0;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *Ptr1;
  INTN                             Index;
  INTN                             J;
  INTN                             M;
  

  Ptr1 = (Step > 0) ? Ptr : Ptr - 1;
  M = MaxWidth;
  for (J = 0; J < mFontHeight; ++J) {
    Ptr0 = Ptr1 + J * Row;
    for (Index = 0; Index < MaxWidth; ++Index) {
      if (!EmptyPix (Ptr0, FirstPixel)) {
        break;
      }
      Ptr0 += Step;
    }
    M = (Index > M) ? M : Index;
  }
  return M;
}

STATIC
INTN
RenderText (
  IN     CHAR16                 *Text,
  IN OUT NDK_UI_IMAGE           *CompImage,
  IN     INTN                   Xpos,
  IN     INTN                   Ypos,
  IN     INTN                   Cursor
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BufferPtr;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *FontPixelData;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *FirstPixelBuf;
  INTN                          BufferLineWidth;
  INTN                          BufferLineOffset;
  INTN                          FontLineOffset;
  INTN                          TextLength;
  INTN                          Index;
  UINT16                        C;
  UINT16                        C0;
  UINT16                        C1;
  UINTN                         Shift;
  UINTN                         LeftSpace;
  UINTN                         RightSpace;
  INTN                          RealWidth;
  INTN                          ScaledWidth;
  
  ScaledWidth = (INTN) CHAR_WIDTH;
  Shift       = 0;
  RealWidth   = 0;
  
  TextLength = StrLen (Text);
  if (mFontImage == NULL) {
    PrepareFont ();
  }
  
  BufferPtr = CompImage->Bitmap;
  BufferLineOffset = CompImage->Width;
  BufferLineWidth = BufferLineOffset - Xpos;
  BufferPtr += Xpos + Ypos * BufferLineOffset;
  FirstPixelBuf = BufferPtr;
  FontPixelData = mFontImage->Bitmap;
  FontLineOffset = mFontImage->Width;

  if (ScaledWidth < mFontWidth) {
    Shift = (mFontWidth - ScaledWidth) >> 1;
  }
  C0 = 0;
  RealWidth = ScaledWidth;
  for (Index = 0; Index < TextLength; ++Index) {
    C = Text[Index];
    C1 = (((C >= 0xC0) ? (C - (0xC0 - 0xC0)) : C) & 0xff);
    C = C1;

    if (mProportional) {
      if (C0 <= 0x20) {
        LeftSpace = 2;
      } else {
        LeftSpace = GetEmpty (BufferPtr, FirstPixelBuf, ScaledWidth, -1, BufferLineOffset);
      }
      if (C <= 0x20) {
        RightSpace = 1;
        RealWidth = (ScaledWidth >> 1) + 1;
      } else {
        RightSpace = GetEmpty (FontPixelData + C * mFontWidth, FontPixelData, mFontWidth, 1, FontLineOffset);
        if (RightSpace >= ScaledWidth + Shift) {
          RightSpace = 0;
        }
        RealWidth = mFontWidth - RightSpace;
      }

    } else {
      LeftSpace = 2;
      RightSpace = Shift;
    }
    C0 = C;
    if ((UINTN) BufferPtr + RealWidth * 4 > (UINTN) FirstPixelBuf + BufferLineWidth * 4) {
      break;
    }
    RawCompose (BufferPtr - LeftSpace + 2, FontPixelData + C * mFontWidth + RightSpace,
                RealWidth,
                mFontHeight,
                BufferLineOffset,
                FontLineOffset
                );
    
    if (Index == Cursor) {
      C = 0x5F;
      RawCompose (BufferPtr - LeftSpace + 2, FontPixelData + C * mFontWidth + RightSpace,
                  RealWidth, mFontHeight,
                  BufferLineOffset, FontLineOffset
                  );
    }
    BufferPtr += RealWidth - LeftSpace + 2;
  }
  return ((INTN) BufferPtr - (INTN) FirstPixelBuf) / sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
}

NDK_UI_IMAGE *
CreateTextImage (
  IN CHAR16         *String
  )
{
  NDK_UI_IMAGE      *Image;
  NDK_UI_IMAGE      *TmpImage;
  NDK_UI_IMAGE      *ScaledTextImage;
  INTN              Width;
  INTN              TextWidth;
  
  TextWidth = 0;
  
  if (String == NULL) {
    return NULL;
  }
  
  Width = ((StrLen (String) + 1) * (INTN) CHAR_WIDTH);
  Image = CreateFilledImage (Width, mTextHeight, TRUE, &mTransparentPixel);
  if (Image != NULL) {
    TextWidth = RenderText (String, Image, 0, 0, 0xFFFF);
  }
  
  TmpImage = CreateImage (TextWidth, mFontHeight, TRUE);
  RawCopy (TmpImage->Bitmap,
           Image->Bitmap,
           TmpImage->Width,
           TmpImage->Height,
           TmpImage->Width,
           Image->Width
           );
  
  FreeImage (Image);
  ScaledTextImage = CopyScaledImage (TmpImage, mTextScale);
    
  if (ScaledTextImage == NULL) {
    DEBUG ((DEBUG_INFO, "OCUI: Failed to scale image!\n"));
    FreeImage (TmpImage);
  }
  
  return ScaledTextImage;
}

STATIC
VOID
PrintTextGraphicXY (
  IN CHAR16                           *String,
  IN INTN                             Xpos,
  IN INTN                             Ypos
  )
{
  NDK_UI_IMAGE                        *TextImage;
  NDK_UI_IMAGE                        *NewImage;

  NewImage = NULL;
  
  TextImage = CreateTextImage (String);
  if (TextImage == NULL) {
    return;
  }
  
  if (Xpos < 0) {
    Xpos = (mScreenWidth - TextImage->Width) / 2;
  }
  
  if ((Xpos + TextImage->Width + 8) > mScreenWidth) {
    Xpos = mScreenWidth - (TextImage->Width + 8);
  }
  
  if ((Ypos + TextImage->Height + 5) > mScreenHeight) {
    Ypos = mScreenHeight - (TextImage->Height + 5);
  }
  
  BltImageAlpha (TextImage, Xpos, Ypos, &mTransparentPixel, 16);
}
//
//     Text rendering end
//
VOID
PrintLabel (
  IN OC_BOOT_ENTRY   *Entries,
  IN UINTN           *VisibleList,
  IN UINTN           VisibleIndex,
  IN INTN            Xpos,
  IN INTN            Ypos
  )
{
  NDK_UI_IMAGE    *TextImage;
  NDK_UI_IMAGE    *NewImage;
  NDK_UI_IMAGE    *LabelImage;
  UINTN           Index;
  UINTN           Needle;
  CHAR16          *String;
  UINTN           Length;
  INTN            Rows;
  INTN            IconsPerRow;
  INTN            NewXpos;
  INTN            NewYpos;
  
  Length = (144 / (INTN) CHAR_WIDTH) - 2;
  Rows = mMenuImage->Height / mIconSpaceSize;
  IconsPerRow = mMenuImage->Width / mIconSpaceSize;
  NewXpos = Xpos;
  NewYpos = Ypos;
  
  for (Index = 0; Index < VisibleIndex; ++Index) {
    if (StrLen (Entries[VisibleList[Index]].Name) > Length) {
      String = AllocateZeroPool ((Length + 1) * sizeof (CHAR16));
      StrnCpyS (String, Length + 1, Entries[VisibleList[Index]].Name, Length);
      for (Needle = Length; Needle > 0; --Needle) {
        if (String[Needle] == 0x20) {
          StrnCpyS (String, Needle + 1, Entries[VisibleList[Index]].Name, Needle);
          break;
        }
      }
      TextImage = CreateTextImage (String);
      FreePool (String);
    } else {
      TextImage = CreateTextImage (Entries[VisibleList[Index]].Name);
    }
    
    if (TextImage == NULL) {
      return;
    }
    
    LabelImage = CopyScaledImage (mLabelImage, (mIconSpaceSize << 4) / mLabelImage->Width);
     
    NewImage = CreateImage (LabelImage->Width, LabelImage->Height, FALSE);
    
    if (Index == IconsPerRow) {
      NewXpos = Xpos;
      NewYpos = Ypos + mIconSpaceSize + (32 * mUiScale >> 4) + 10;
    }
     
    TakeImage (NewImage, NewXpos, NewYpos + mIconSpaceSize + 10, LabelImage->Width, LabelImage->Height);
     
    RawCompose (NewImage->Bitmap,
                LabelImage->Bitmap,
                NewImage->Width,
                NewImage->Height,
                NewImage->Width,
                LabelImage->Width
                );
     
    FreeImage (LabelImage);
    
    ComposeImage (NewImage, TextImage, (NewImage->Width - TextImage->Width) >> 1, (NewImage->Height - TextImage->Height) >> 1);
    
    DrawImageArea (NewImage, 0, 0, 0, 0, NewXpos, NewYpos + mIconSpaceSize + 10);
    FreeImage (TextImage);
    FreeImage (NewImage);
    NewXpos = NewXpos + mIconSpaceSize;
  }
}

STATIC
VOID
PrintDateTime (
  IN BOOLEAN         ShowAll
  )
{
  EFI_STATUS         Status;
  EFI_TIME           DateTime;
  CHAR16             DateStr[12];
  CHAR16             TimeStr[12];
  UINTN              Hour;
  CHAR16             *Str;
  
  Str = NULL;
  Hour = 0;
  Status = gRT->GetTime (&DateTime, NULL);
  if (EFI_ERROR (Status)) {
    ZeroMem (&DateTime, sizeof (DateTime));
  }
  
  if (!EFI_ERROR (Status) && ShowAll) {
    Hour = (UINTN) DateTime.Hour;
    Str = Hour >= 12 ? L"PM" : L"AM";
    if (Hour > 12) {
      Hour = Hour - 12;
    }
    ClearScreenArea (&mTransparentPixel, 0, 0, mScreenWidth, mFontHeight * 5);
    UnicodeSPrint (DateStr, sizeof (DateStr), L" %02u/%02u/%04u", DateTime.Month, DateTime.Day, DateTime.Year);
    UnicodeSPrint (TimeStr, sizeof (TimeStr), L" %02u:%02u:%02u%s", Hour, DateTime.Minute, DateTime.Second, Str);
    PrintTextGraphicXY (DateStr, mScreenWidth, 5);
    PrintTextGraphicXY (TimeStr, mScreenWidth, (mTextScale == 16) ? (mFontHeight + 5 + 2) : ((mFontHeight * 2) + 5 + 2));
  } else {
    ClearScreenArea (&mTransparentPixel, 0, 0, mScreenWidth, mFontHeight * 5);
  }
}

STATIC
VOID
PrintOcVersion (
  IN CONST CHAR8         *String,
  IN BOOLEAN             ShowAll
  )
{
  CHAR16                 *NewString;
  
  if (String == NULL) {
    return;
  }
  
  NewString = AsciiStrCopyToUnicode (String, 0);
  if (String != NULL && ShowAll) {
    PrintTextGraphicXY (NewString, mScreenWidth, mScreenHeight);
  } else {
    ClearScreenArea (&mTransparentPixel,
                       mScreenWidth - ((StrLen(NewString) * mFontWidth) * 2),
                       mScreenHeight - mFontHeight * 2,
                       (StrLen(NewString) * mFontWidth) * 2,
                       mFontHeight * 2
                       );
  }
}

STATIC
VOID
PrintDefaultBootMode (
  IN BOOLEAN         ShowAll
  )
{
  CHAR16             String[18];
  if (ShowAll) {
    UnicodeSPrint (String, sizeof (String), L"Auto default: %s", mAllowSetDefault ? L"Off" : L"On");
    PrintTextGraphicXY (String, 8, mScreenHeight);
  } else {
    ClearScreenArea (&mTransparentPixel,
                       0,
                       mScreenHeight - mFontHeight * 2,
                       mFontWidth * 2 * sizeof (String),
                       mFontHeight * 2
                       );
  }
}

STATIC
BOOLEAN
PrintTimeOutMessage (
  IN UINTN             Timeout
  )
{
  CHAR16               String[52];
  
  if (Timeout > 0) {
    UnicodeSPrint (String, sizeof (String), L"%s %02u %s.", L"The default boot selection will start in", Timeout, L"seconds");
    PrintTextGraphicXY (String, -1, (mScreenHeight / 4) * 3);
  } else {
    ClearScreenArea (&mTransparentPixel, 0, ((mScreenHeight / 4) * 3) - 4, mScreenWidth, mFontHeight * 2);
  }
  return !(Timeout > 0);
}

STATIC
VOID
PrintTextDescription (
  IN UINTN         MaxStrWidth,
  IN UINTN         Selected,
  IN OC_BOOT_ENTRY *Entry
  )
{
  NDK_UI_IMAGE    *TextImage;
  NDK_UI_IMAGE    *NewImage;
  CHAR16          Code[3];
  CHAR16          String[MaxStrWidth + 1];
  
  if (mPrintLabel) {
    return;
  }
  
  Code[0] = 0x20;
  Code[1] = OC_INPUT_STR[Selected];
  Code[2] = '\0';

  UnicodeSPrint (String, sizeof (String), L" %s%s%s%s%s ",
                 Code,
                 (mAllowSetDefault && mDefaultEntry == Selected) ? L".*" : L". ",
                 Entry->Name,
                 Entry->IsExternal ? L" (ext)" : L"",
                 Entry->IsFolder ? L" (dmg)" : L""
                 );

  TextImage = CreateTextImage (String);
  if (TextImage == NULL) {
    return;
  }
  NewImage = CreateFilledImage (mScreenWidth, TextImage->Height, TRUE, &mTransparentPixel);
  if (NewImage == NULL) {
    FreeImage (TextImage);
    return;
  }
  ComposeImage (NewImage, TextImage, (NewImage->Width - TextImage->Width) / 2, 0);
  if (TextImage != NULL) {
    FreeImage (TextImage);
  }

  BltImageAlpha (NewImage,
                 (mScreenWidth - NewImage->Width) / 2,
                 (mScreenHeight / 2) + mIconSpaceSize,
                 &mTransparentPixel,
                 16
                 );
}

/* Mouse Functions Begin */

STATIC
VOID
HidePointer (
  VOID
  )
{
  if (mPointer.SimplePointerProtocol != NULL) {
    DrawImageArea (mPointer.OldImage, 0, 0, 0, 0, mPointer.OldPlace.Xpos, mPointer.OldPlace.Ypos);
  }
}

STATIC
VOID
DrawPointer (
  VOID
  )
{
  if (mPointer.SimplePointerProtocol == NULL) {
    return;
  }
  TakeImage (mPointer.OldImage,
             mPointer.NewPlace.Xpos,
             mPointer.NewPlace.Ypos,
             POINTER_WIDTH,
             POINTER_HEIGHT
             );
  
  CopyMem (&mPointer.OldPlace, &mPointer.NewPlace, sizeof(AREA_RECT));
  
  CopyMem (mPointer.NewImage->Bitmap,
           mPointer.OldImage->Bitmap,
           (UINTN) (POINTER_WIDTH * POINTER_HEIGHT * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL))
           );

  RawCompose (mPointer.NewImage->Bitmap,
              mPointer.Pointer->Bitmap,
              mPointer.NewImage->Width,
              mPointer.NewImage->Height,
              mPointer.NewImage->Width,
              mPointer.Pointer->Width
              );
  
  DrawImageArea (mPointer.NewImage,
                 0,
                 0,
                 POINTER_WIDTH,
                 POINTER_HEIGHT,
                 mPointer.OldPlace.Xpos,
                 mPointer.OldPlace.Ypos
                 );
}

STATIC
VOID
RedrawPointer (
  VOID
  )
{
  if (mPointer.SimplePointerProtocol == NULL) {
   return;
  }
  
  HidePointer ();
  DrawPointer ();
}

EFI_STATUS
InitMouse (
  VOID
  )
{
  EFI_STATUS          Status;
  CHAR16              *FilePath;
  
  Status = EFI_UNSUPPORTED;
  
  if (mPointer.SimplePointerProtocol != NULL) {
    DrawPointer ();
    return EFI_SUCCESS;
  }
  
  Status = gBS->HandleProtocol (gST->ConsoleInHandle, &gEfiSimplePointerProtocolGuid, (VOID**) &mPointer.SimplePointerProtocol);
  if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "OCMouse: No SimplePointerProtocol found!\n"));
      Status = gBS->LocateProtocol (&gEfiSimplePointerProtocolGuid, NULL, (VOID**) &mPointer.SimplePointerProtocol);
  }

  if (EFI_ERROR (Status)) {
    mPointer.Pointer = NULL;
    mPointer.MouseEvent = NoEvents;
    mPointer.SimplePointerProtocol = NULL;
    DEBUG ((DEBUG_INFO, "OCMouse: No Mouse found!\n"));
    return Status;
  }
  
  if (mUiScale == 28 || mScreenHeight >= 2160) {
    FilePath = L"EFI\\OC\\Icons\\pointer4k.png";
  } else {
    FilePath = L"EFI\\OC\\Icons\\pointer.png";
  }
  
  if (FileExist (FilePath)) {
    mPointer.Pointer = DecodePNGFile (FilePath);
  } else {
    mPointer.Pointer = CreateFilledImage (POINTER_WIDTH, POINTER_HEIGHT, TRUE, &mBluePixel);
  }
  
  if(mPointer.Pointer == NULL) {
    DEBUG ((DEBUG_INFO, "OCMouse: No Mouse Icon found!\n"));
    mPointer.SimplePointerProtocol = NULL;
    return EFI_NOT_FOUND;
  }

  mPointer.LastClickTime = 0;
  mPointer.OldPlace.Xpos = (INTN) (mScreenWidth >> 2);
  mPointer.OldPlace.Ypos = (INTN) (mScreenHeight >> 2);
  mPointer.OldPlace.Width = POINTER_WIDTH;
  mPointer.OldPlace.Height = POINTER_HEIGHT;
  
  CopyMem (&mPointer.NewPlace, &mPointer.OldPlace, sizeof (AREA_RECT));
  
  mPointer.OldImage = CreateImage(POINTER_WIDTH, POINTER_HEIGHT, FALSE);
  mPointer.NewImage = CreateFilledImage(POINTER_WIDTH, POINTER_HEIGHT, TRUE, &mTransparentPixel);
  DrawPointer ();
  mPointer.MouseEvent = NoEvents;
  return Status;
}

STATIC
VOID
PointerUpdate (
  VOID
  )
{
  UINT64                    Now;
  EFI_STATUS                Status;
  EFI_SIMPLE_POINTER_STATE  tmpState;
  EFI_SIMPLE_POINTER_MODE   *CurrentMode;
  INTN                      ScreenRelX;
  INTN                      ScreenRelY;
  
  Now = GetTimeInNanoSecond (GetPerformanceCounter ());
  Status = mPointer.SimplePointerProtocol->GetState (mPointer.SimplePointerProtocol, &tmpState);
  if (!EFI_ERROR (Status)) {
    if (!mPointer.State.LeftButton && tmpState.LeftButton) {
      mPointer.MouseEvent = LeftMouseDown;
    } else if (!mPointer.State.RightButton && tmpState.RightButton) {
      mPointer.MouseEvent = RightMouseDown;
    } else if (mPointer.State.LeftButton && !tmpState.LeftButton) {
      if (Now < (mPointer.LastClickTime + mDoubleClickTime * 1000000ULL)) {
        mPointer.MouseEvent = DoubleClick;
      } else {
        mPointer.MouseEvent = LeftClick;
      }
      mPointer.LastClickTime = Now;
    } else if (mPointer.State.RightButton && !tmpState.RightButton) {
      mPointer.MouseEvent = RightClick;
    } else if (mPointer.State.RelativeMovementZ > 0) {
      mPointer.MouseEvent = ScrollDown;
    } else if (mPointer.State.RelativeMovementZ < 0) {
      mPointer.MouseEvent = ScrollUp;
    } else if (mPointer.State.RelativeMovementX || mPointer.State.RelativeMovementY) {
      mPointer.MouseEvent = MouseMove;
    } else {
      mPointer.MouseEvent = NoEvents;
    }
    
    CopyMem (&mPointer.State, &tmpState, sizeof (EFI_SIMPLE_POINTER_STATE));
    CurrentMode = mPointer.SimplePointerProtocol->Mode;
  
    ScreenRelX = ((mScreenWidth * mPointer.State.RelativeMovementX / (INTN) CurrentMode->ResolutionX) * mPointerSpeed) >> 10;
    
    mPointer.NewPlace.Xpos += ScreenRelX;
    
    if (mPointer.NewPlace.Xpos < 0) {
      mPointer.NewPlace.Xpos = 0;
    }
    if (mPointer.NewPlace.Xpos > mScreenWidth - 1) {
      mPointer.NewPlace.Xpos = mScreenWidth - 1;
    }
    
    ScreenRelY = ((mScreenHeight * mPointer.State.RelativeMovementY / (INTN) CurrentMode->ResolutionY) * mPointerSpeed) >> 10;
    mPointer.NewPlace.Ypos += ScreenRelY;
    
    if (mPointer.NewPlace.Ypos < 0) {
      mPointer.NewPlace.Ypos = 0;
    }
    
    if (mPointer.NewPlace.Ypos > mScreenHeight - 1) {
      mPointer.NewPlace.Ypos = mScreenHeight - 1;
    }
    
    RedrawPointer();
  }
}

STATIC
BOOLEAN
MouseInRect (
  IN AREA_RECT     *Place
  )
{
  return  ((mPointer.NewPlace.Xpos >= Place->Xpos)
           && (mPointer.NewPlace.Xpos < (Place->Xpos + (INTN) Place->Width))
           && (mPointer.NewPlace.Ypos >= Place->Ypos)
           && (mPointer.NewPlace.Ypos < (Place->Ypos + (INTN) Place->Height) + (32 * mUiScale >> 4) + 10)
           );
}

STATIC
INTN
CheckIconClick (
  VOID
  )
{
  INTN       IconsPerRow;
  INTN       Rows;
  INTN       Xpos;
  INTN       Ypos;
  INTN       NewXpos;
  INTN       NewYpos;
  INTN       Result;
  UINTN      Index;
  AREA_RECT  Place;
  
  Result = -1;
  
  if (mMenuImage == NULL) {
    return Result;
  }
  
  Place.Width = mIconSpaceSize;
  Place.Height = mIconSpaceSize;
  
  Rows = mMenuImage->Height / mIconSpaceSize;
  IconsPerRow = mMenuImage->Width / mIconSpaceSize;
  Xpos = (mScreenWidth - mMenuImage->Width) / 2;
  Ypos = (mScreenHeight / 2) - mIconSpaceSize;
  NewXpos = Xpos;
  NewYpos = Ypos;
  
  for (Index = 0; Index < mMenuIconsCount; ++Index) {
    Place.Xpos = NewXpos;
    Place.Ypos = NewYpos;
    if (MouseInRect (&Place)) {
      Result = (INTN) Index;
      break;
    }
    NewXpos = NewXpos + mIconSpaceSize;
    if (Index == (IconsPerRow - 1)) {
      NewXpos = Xpos;
      NewYpos = Ypos + mIconSpaceSize + (32 * mUiScale >> 4) + 10;
    }
  }
  return Result;
}

STATIC
VOID
KillMouse (
  VOID
  )
{
  if (mPointer.SimplePointerProtocol == NULL) {
    return;
  }
  
  FreeImage (mPointer.NewImage);
  mPointer.NewImage = NULL;
  FreeImage (mPointer.OldImage);
  mPointer.OldImage = NULL;

  if (mPointer.Pointer != NULL) {
    FreeImage (mPointer.Pointer);
  }

  mPointer.Pointer = NULL;
  mPointer.MouseEvent = NoEvents;
  mPointer.SimplePointerProtocol = NULL;
}

/* Mouse Functions End*/

STATIC
INTN
OcWaitForKeyIndex (
  IN OUT OC_PICKER_CONTEXT                  *Context,
  IN     APPLE_KEY_MAP_AGGREGATOR_PROTOCOL  *KeyMap,
  IN     UINTN                              Timeout,
  IN     BOOLEAN                            PollHotkeys,
     OUT BOOLEAN                            *SetDefault  OPTIONAL
  )
{
  EFI_STATUS                         Status;
  APPLE_KEY_CODE                     KeyCode;

  UINTN                              NumKeys;
  APPLE_MODIFIER_MAP                 Modifiers;
  APPLE_KEY_CODE                     Keys[OC_KEY_MAP_DEFAULT_SIZE];

  BOOLEAN                            HasCommand;
  BOOLEAN                            HasShift;
  BOOLEAN                            HasKeyC;
  BOOLEAN                            HasKeyK;
  BOOLEAN                            HasKeyS;
  BOOLEAN                            HasKeyV;
  BOOLEAN                            HasKeyMinus;
  BOOLEAN                            WantsZeroSlide;
  BOOLEAN                            WantsDefault;
  UINT32                             CsrActiveConfig;
  UINT64                             CurrTime;
  UINT64                             EndTime;
  UINTN                              CsrActiveConfigSize;
  INTN                               KeyClick;
  INTN                               CycleCount;
  
  CycleCount = 0;
  
  CurrTime  = GetTimeInNanoSecond (GetPerformanceCounter ());
  EndTime   = CurrTime + Timeout * 1000000ULL;
  if (SetDefault != NULL) {
    *SetDefault = FALSE;
  }
  
  while (Timeout == 0 || CurrTime == 0 || CurrTime < EndTime) {
    if (mPointer.SimplePointerProtocol != NULL) {
      PointerUpdate();
      switch (mPointer.MouseEvent) {
        case LeftClick:
          mPointer.MouseEvent = NoEvents;
          KeyClick = CheckIconClick ();
          if (KeyClick >= 0 && KeyClick != mCurrentSelection) {
            mCurrentSelection = KeyClick;
            return OC_INPUT_POINTER;
          }
          break;
        case RightClick:
          mPointer.MouseEvent = NoEvents;
          return OC_INPUT_SPACEBAR;
        case DoubleClick:
          mPointer.MouseEvent = NoEvents;
          KeyClick = CheckIconClick ();
          if (KeyClick >= 0) {
            return KeyClick;
          }
          break;
        case MouseMove:
          CycleCount++;
          mPointer.MouseEvent = NoEvents;
          if (CycleCount == 10) {
            KeyClick = CheckIconClick ();
            if (KeyClick >= 0 && KeyClick != mCurrentSelection) {
              mCurrentSelection = KeyClick;
              return OC_INPUT_POINTER;
            }
            CycleCount = 0;
          }
          break;
        default:
          break;
      }
      mPointer.MouseEvent = NoEvents;
    }
    NumKeys = ARRAY_SIZE (Keys);
    Status = KeyMap->GetKeyStrokes (
                       KeyMap,
                       &Modifiers,
                       &NumKeys,
                       Keys
                       );

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "OCB: GetKeyStrokes - %r\n", Status));
      return OC_INPUT_INVALID;
    }

    CurrTime    = GetTimeInNanoSecond (GetPerformanceCounter ());
    
    if (PollHotkeys) {
      HasCommand = (Modifiers & (APPLE_MODIFIER_LEFT_COMMAND | APPLE_MODIFIER_RIGHT_COMMAND)) != 0;
      HasShift   = (Modifiers & (APPLE_MODIFIER_LEFT_SHIFT | APPLE_MODIFIER_RIGHT_SHIFT)) != 0;
      HasKeyC    = OcKeyMapHasKey (Keys, NumKeys, AppleHidUsbKbUsageKeyC);
      HasKeyK    = OcKeyMapHasKey (Keys, NumKeys, AppleHidUsbKbUsageKeyK);
      HasKeyS    = OcKeyMapHasKey (Keys, NumKeys, AppleHidUsbKbUsageKeyS);
      HasKeyV    = OcKeyMapHasKey (Keys, NumKeys, AppleHidUsbKbUsageKeyV);

      HasKeyMinus = OcKeyMapHasKey (Keys, NumKeys, AppleHidUsbKbUsageKeyMinus)
        || OcKeyMapHasKey (Keys, NumKeys, AppleHidUsbKbUsageKeyPadMinus);

      if (HasShift) {
        if (OcGetArgumentFromCmd (Context->AppleBootArgs, "-x", L_STR_LEN ("-x")) == NULL) {
          DEBUG ((DEBUG_INFO, "OCB: Shift means -x\n"));
          OcAppendArgumentToCmd (Context, Context->AppleBootArgs, "-x", L_STR_LEN ("-x"));
        }
        continue;
      }

      //
      // CMD+V is always valid and enables Verbose Mode.
      //
      if (HasCommand && HasKeyV) {
        if (OcGetArgumentFromCmd (Context->AppleBootArgs, "-v", L_STR_LEN ("-v")) == NULL) {
          DEBUG ((DEBUG_INFO, "OCB: CMD+V means -v\n"));
          OcAppendArgumentToCmd (Context, Context->AppleBootArgs, "-v", L_STR_LEN ("-v"));
        }
        continue;
      }

      if (HasCommand && HasKeyC && HasKeyMinus) {
        if (OcGetArgumentFromCmd (Context->AppleBootArgs, "-no_compat_check", L_STR_LEN ("-no_compat_check")) == NULL) {
          DEBUG ((DEBUG_INFO, "OCB: CMD+C+MINUS means -no_compat_check\n"));
          OcAppendArgumentToCmd (Context, Context->AppleBootArgs, "-no_compat_check", L_STR_LEN ("-no_compat_check"));
        }
        continue;
      }

      if (HasCommand && HasKeyK) {
        if (AsciiStrStr (Context->AppleBootArgs, "kcsuffix=release") == NULL) {
          DEBUG ((DEBUG_INFO, "OCB: CMD+K means kcsuffix=release\n"));
          OcAppendArgumentToCmd (Context, Context->AppleBootArgs, "kcsuffix=release", L_STR_LEN ("kcsuffix=release"));
        }
        continue;
      }

      if (HasCommand && HasKeyS) {
        WantsZeroSlide = HasKeyMinus;

        if (WantsZeroSlide) {
          CsrActiveConfig     = 0;
          CsrActiveConfigSize = sizeof (CsrActiveConfig);
          Status = gRT->GetVariable (
            L"csr-active-config",
            &gAppleBootVariableGuid,
            NULL,
            &CsrActiveConfigSize,
            &CsrActiveConfig
            );
          
          WantsZeroSlide = !EFI_ERROR (Status) && (CsrActiveConfig & CSR_ALLOW_UNRESTRICTED_NVRAM) != 0;
        }

        if (WantsZeroSlide) {
          if (AsciiStrStr (Context->AppleBootArgs, "slide=0") == NULL) {
            DEBUG ((DEBUG_INFO, "OCB: CMD+S+MINUS means slide=0\n"));
            OcAppendArgumentToCmd (Context, Context->AppleBootArgs, "slide=0", L_STR_LEN ("slide=0"));
          }
        } else if (OcGetArgumentFromCmd (Context->AppleBootArgs, "-s", L_STR_LEN ("-s")) == NULL) {
          DEBUG ((DEBUG_INFO, "OCB: CMD+S means -s\n"));
          OcAppendArgumentToCmd (Context, Context->AppleBootArgs, "-s", L_STR_LEN ("-s"));
        }
        continue;
      }
    }

    if ((Modifiers & (APPLE_MODIFIER_LEFT_COMMAND | APPLE_MODIFIER_RIGHT_COMMAND)) != 0
      && OcKeyMapHasKey (Keys, NumKeys, AppleHidUsbKbUsageKeyF5)) {
      OcKeyMapFlush (KeyMap, 0, TRUE);
      return OC_INPUT_VOICE_OVER;
    }
    
    if (OcKeyMapHasKey (Keys, NumKeys, AppleHidUsbKbUsageKeyEscape)
     || OcKeyMapHasKey (Keys, NumKeys, AppleHidUsbKbUsageKeyZero)) {
      OcKeyMapFlush (KeyMap, 0, TRUE);
      return OC_INPUT_ABORTED;
    }
    
    WantsDefault = Modifiers != 0 && (Modifiers & ~(APPLE_MODIFIER_LEFT_CONTROL | APPLE_MODIFIER_RIGHT_CONTROL)) == 0;

    if ((Modifiers == 0 || WantsDefault) && NumKeys == 1) {
      if (Keys[0] == AppleHidUsbKbUsageKeySpaceBar) {
        OcKeyMapFlush (KeyMap, Keys[0], TRUE);
        return OC_INPUT_SPACEBAR;
      }
      
      if (Keys[0] == AppleHidUsbKbUsageKeyEnter
        || Keys[0] == AppleHidUsbKbUsageKeyReturn
        || Keys[0] == AppleHidUsbKbUsageKeyPadEnter) {
        if (WantsDefault && SetDefault != NULL) {
          *SetDefault = TRUE;
        }
        OcKeyMapFlush (KeyMap, Keys[0], TRUE);
        return OC_INPUT_RETURN;
      }
      
      if (Keys[0] == AppleHidUsbKbUsageKeyUpArrow || Keys[0] == AppleHidUsbKbUsageKeyLeftArrow) {
        OcKeyMapFlush (KeyMap, Keys[0], TRUE);
        return OC_INPUT_UP;
      }
      
      if (Keys[0] == AppleHidUsbKbUsageKeyDownArrow || Keys[0] == AppleHidUsbKbUsageKeyRightArrow) {
        OcKeyMapFlush (KeyMap, Keys[0], TRUE);
        return OC_INPUT_DOWN;
      }

      STATIC_ASSERT (AppleHidUsbKbUsageKeyF1 + 11 == AppleHidUsbKbUsageKeyF12, "Unexpected encoding");
      if (Keys[0] >= AppleHidUsbKbUsageKeyF1 && Keys[0] <= AppleHidUsbKbUsageKeyF12) {
        OcKeyMapFlush (KeyMap, Keys[0], TRUE);
        return OC_INPUT_FUNCTIONAL (Keys[0] - AppleHidUsbKbUsageKeyF1 + 1);
      }

      STATIC_ASSERT (AppleHidUsbKbUsageKeyF13 + 11 == AppleHidUsbKbUsageKeyF24, "Unexpected encoding");
      if (Keys[0] >= AppleHidUsbKbUsageKeyF13 && Keys[0] <= AppleHidUsbKbUsageKeyF24) {
        OcKeyMapFlush (KeyMap, Keys[0], TRUE);
        return OC_INPUT_FUNCTIONAL (Keys[0] - AppleHidUsbKbUsageKeyF13 + 13);
      }
      
      STATIC_ASSERT (AppleHidUsbKbUsageKeyOne + 8 == AppleHidUsbKbUsageKeyNine, "Unexpected encoding");
      for (KeyCode = AppleHidUsbKbUsageKeyOne; KeyCode <= AppleHidUsbKbUsageKeyNine; ++KeyCode) {
        if (OcKeyMapHasKey (Keys, NumKeys, KeyCode)) {
          if (WantsDefault && SetDefault != NULL) {
            *SetDefault = TRUE;
          }
          OcKeyMapFlush (KeyMap, Keys[0], TRUE);
          return (INTN) (KeyCode - AppleHidUsbKbUsageKeyOne);
        }
      }

      STATIC_ASSERT (AppleHidUsbKbUsageKeyA + 25 == AppleHidUsbKbUsageKeyZ, "Unexpected encoding");
      for (KeyCode = AppleHidUsbKbUsageKeyA; KeyCode <= AppleHidUsbKbUsageKeyZ; ++KeyCode) {
        if (OcKeyMapHasKey (Keys, NumKeys, KeyCode)) {
          if (WantsDefault && SetDefault != NULL) {
            *SetDefault = TRUE;
          }
          OcKeyMapFlush (KeyMap, Keys[0], TRUE);
          return (INTN) (KeyCode - AppleHidUsbKbUsageKeyA + 9);
        }
      }
    }

    if (Timeout != 0 && NumKeys != 0) {
      return OC_INPUT_INVALID;
    }

    MicroSecondDelay (10);
  }

  return OC_INPUT_TIMEOUT;
}

STATIC
VOID
RestoreConsoleMode (
  IN OC_PICKER_CONTEXT    *Context
  )
{
  FreeImage (mBackgroundImage);
  FreeImage (mMenuImage);
  mMenuImage = NULL;
  FreeImage (mFontImage);
  mFontImage = NULL;
  FreeImage (mSelectionImage);
  mSelectionImage = NULL;
  FreeImage (mLabelImage);
  mLabelImage = NULL;
  ClearScreenArea (&mBlackPixel, 0, 0, mScreenWidth, mScreenHeight);
  mUiScale = 0;
  mTextScale = 0;
  if (Context->ConsoleAttributes != 0) {
    gST->ConOut->SetAttribute (gST->ConOut, Context->ConsoleAttributes & 0x7FU);
  }
  gST->ConOut->SetCursorPosition (gST->ConOut, 0, 0);
  KillMouse ();
}

EFI_STATUS
EFIAPI
OcShowSimpleBootMenu (
  IN OC_PICKER_CONTEXT            *Context,
  IN OC_BOOT_ENTRY                *BootEntries,
  IN UINTN                        Count,
  IN UINTN                        DefaultEntry,
  OUT OC_BOOT_ENTRY               **ChosenBootEntry
  )
{
  EFI_STATUS                         Status;
  UINTN                              Index;
  INTN                               KeyIndex;
  UINT32                             TimeOutSeconds;
  UINTN                              VisibleList[Count];
  UINTN                              VisibleIndex;
  BOOLEAN                            ShowAll;
  UINTN                              Selected;
  UINTN                              MaxStrWidth;
  UINTN                              StrWidth;
  APPLE_KEY_MAP_AGGREGATOR_PROTOCOL  *KeyMap;
  BOOLEAN                            SetDefault;
  BOOLEAN                            NewDefault;
  BOOLEAN                            TimeoutExpired;
  OC_STORAGE_CONTEXT                 *Storage;
  BOOLEAN                            PlayedOnce;
  BOOLEAN                            PlayChosen;
  
  
  Selected         = 0;
  VisibleIndex     = 0;
  MaxStrWidth      = 0;
  TimeoutExpired   = FALSE;
  ShowAll          = !Context->HideAuxiliary;
  TimeOutSeconds   = Context->TimeoutSeconds;
  mAllowSetDefault = Context->AllowSetDefault;
  Storage          = Context->CustomEntryContext;
  mDefaultEntry    = DefaultEntry;
  PlayedOnce       = FALSE;
  PlayChosen       = FALSE;
  
  if (Storage->FileSystem != NULL && mFileSystem == NULL) {
    mFileSystem = Storage->FileSystem;
    DEBUG ((DEBUG_INFO, "OCSBM: FileSystem Found!\n"));
  }
  
  KeyMap = OcAppleKeyMapInstallProtocols (FALSE);
  if (KeyMap == NULL) {
    DEBUG ((DEBUG_ERROR, "OCSBM: Missing AppleKeyMapAggregator\n"));
    return EFI_UNSUPPORTED;
  }
  
  for (Index = 0; Index < MIN (Count, OC_INPUT_MAX); ++Index) {
    StrWidth = StrLen (BootEntries[Index].Name) + ((BootEntries[Index].IsFolder || BootEntries[Index].IsExternal) ? 11 : 5);
    MaxStrWidth = MaxStrWidth > StrWidth ? MaxStrWidth : StrWidth;
  }
  
  InitScreen ();
  ClearScreen (&mTransparentPixel);
  PrepareFont ();
  
  while (TRUE) {
    FreeImage (mMenuImage);
    mMenuImage = NULL;
    for (Index = 0, VisibleIndex = 0; Index < MIN (Count, OC_INPUT_MAX); ++Index) {
      if ((BootEntries[Index].Type == OC_BOOT_APPLE_RECOVERY && !ShowAll)
          || (BootEntries[Index].Type == OC_BOOT_APPLE_TIME_MACHINE && !ShowAll)
          || (BootEntries[Index].Type == OC_BOOT_UNKNOWN && !ShowAll)
          || (BootEntries[Index].DevicePath == NULL && !ShowAll)
          || (BootEntries[Index].IsAuxiliary && !ShowAll)) {
        DefaultEntry = DefaultEntry == Index ? ++DefaultEntry : DefaultEntry;
        continue;
      }
      if (DefaultEntry == Index) {
        Selected = VisibleIndex;
      }
      VisibleList[VisibleIndex] = Index;
      CreateIcon (BootEntries[Index].Name,
                  BootEntries[Index].Type,
                  VisibleIndex,
                  BootEntries[Index].IsExternal,
                  BootEntries[Index].IsFolder
                  );
      ++VisibleIndex;
    }
    
    ClearScreenArea (&mTransparentPixel, 0, (mScreenHeight / 2) - mIconSpaceSize, mScreenWidth, mIconSpaceSize * 3);
    BltMenuImage (mMenuImage, (mScreenWidth - mMenuImage->Width) / 2, (mScreenHeight / 2) - mIconSpaceSize);
    if (mPrintLabel) {
      PrintLabel (BootEntries, VisibleList, VisibleIndex, (mScreenWidth - mMenuImage->Width) / 2, (mScreenHeight / 2) - mIconSpaceSize);
    }
    
    PrintTextDescription (MaxStrWidth,
                          Selected,
                          &BootEntries[DefaultEntry]
                          );
    
    SwitchIconSelection (VisibleIndex, Selected, TRUE);
    mCurrentSelection = Selected;
    mMenuIconsCount = VisibleIndex;
    
    PrintDefaultBootMode (ShowAll);
    PrintOcVersion (Context->TitleSuffix, ShowAll);
    PrintDateTime (ShowAll);
    if (!TimeoutExpired) {
      TimeoutExpired = PrintTimeOutMessage (TimeOutSeconds);
      TimeOutSeconds = TimeoutExpired ? 10000 : TimeOutSeconds;
    }
    
    if (mPointer.SimplePointerProtocol == NULL) {
      InitMouse ();
    } else {
      DrawPointer ();
    }
    
    if (ShowAll && PlayedOnce) {
      OcPlayAudioFile (Context, OcVoiceOverAudioFileShowAuxiliary, FALSE);
    }
    
    if (!PlayedOnce && Context->PickerAudioAssist) {
      OcPlayAudioFile (Context, OcVoiceOverAudioFileChooseOS, FALSE);
      for (Index = 0; Index < VisibleIndex; ++Index) {
        OcPlayAudioEntry (Context, &BootEntries[VisibleList[Index]], 1 + (UINT32) Index);
        if (DefaultEntry == VisibleList[Index] && TimeOutSeconds > 0) {
          OcPlayAudioFile (Context, OcVoiceOverAudioFileDefault, FALSE);
        }
      }
      OcPlayAudioBeep (
        Context,
        OC_VOICE_OVER_SIGNALS_NORMAL,
        OC_VOICE_OVER_SIGNAL_NORMAL_MS,
        OC_VOICE_OVER_SILENCE_NORMAL_MS
        );
      PlayedOnce = TRUE;
    }

    while (TRUE) {
      KeyIndex = OcWaitForKeyIndex (Context, KeyMap, 1000, Context->PollAppleHotKeys, &SetDefault);
      if (PlayChosen && KeyIndex == OC_INPUT_TIMEOUT) {
        OcPlayAudioFile (Context, OcVoiceOverAudioFileSelected, FALSE);
        OcPlayAudioEntry (Context, &BootEntries[DefaultEntry], 1 + (UINT32) Selected);
        PlayChosen = FALSE;
      }
      --TimeOutSeconds;
      if ((KeyIndex == OC_INPUT_TIMEOUT && TimeOutSeconds == 0) || KeyIndex == OC_INPUT_RETURN) {
        *ChosenBootEntry = &BootEntries[DefaultEntry];
        SetDefault = BootEntries[DefaultEntry].DevicePath != NULL
          && !BootEntries[DefaultEntry].IsAuxiliary
          && Context->AllowSetDefault
          && SetDefault;
        NewDefault = BootEntries[DefaultEntry].DevicePath != NULL
          && !BootEntries[DefaultEntry].IsAuxiliary
          && !Context->AllowSetDefault
          && mDefaultEntry != DefaultEntry;
        
        if (SetDefault || NewDefault) {
          if (SetDefault) {
            OcPlayAudioFile (Context, OcVoiceOverAudioFileSelected, FALSE);
            OcPlayAudioFile (Context, OcVoiceOverAudioFileDefault, FALSE);
            OcPlayAudioEntry (Context, &BootEntries[DefaultEntry], 1 + (UINT32) Selected);
          }
          Status = OcSetDefaultBootEntry (Context, &BootEntries[DefaultEntry]);
          DEBUG ((DEBUG_INFO, "OCSBM: Setting default - %r\n", Status));
        }
        RestoreConsoleMode (Context);
        return EFI_SUCCESS;
      } else if (KeyIndex == OC_INPUT_ABORTED) {
        TimeOutSeconds = 0;
        OcPlayAudioFile (Context, OcVoiceOverAudioFileAbortTimeout, FALSE);
        break;
      } else if (KeyIndex == OC_INPUT_FUNCTIONAL(10)) {
        TimeOutSeconds = 0;
        TakeScreenShot (L"ScreenShot");
      } else if (KeyIndex == OC_INPUT_FUNCTIONAL(9)) {
        TimeOutSeconds = 0;
        SaveEntriesDataToFile (L"System-Entries", BootEntries, Count);
      } else if (KeyIndex == OC_INPUT_SPACEBAR) {
        HidePointer ();
        ShowAll = !ShowAll;
        while ((BootEntries[DefaultEntry].IsAuxiliary && !ShowAll && DefaultEntry > 0)
               || (BootEntries[DefaultEntry].Type == OC_BOOT_SYSTEM && !ShowAll && DefaultEntry > 0)) {
          --DefaultEntry;
        }
        TimeOutSeconds = 0;
        break;
      } else if (KeyIndex == OC_INPUT_UP) {
        HidePointer ();
        SwitchIconSelection (VisibleIndex, Selected, FALSE);
        DefaultEntry = Selected > 0 ? VisibleList[Selected - 1] : VisibleList[VisibleIndex - 1];
        Selected = Selected > 0 ? --Selected : VisibleIndex - 1;
        mCurrentSelection = Selected;
        SwitchIconSelection (VisibleIndex, Selected, TRUE);
        PrintTextDescription (MaxStrWidth,
                              Selected,
                              &BootEntries[DefaultEntry]
                              );
        TimeOutSeconds = 0;
        PlayChosen = Context->PickerAudioAssist;
        DrawPointer ();
      } else if (KeyIndex == OC_INPUT_DOWN) {
        HidePointer ();
        SwitchIconSelection (VisibleIndex, Selected, FALSE);
        DefaultEntry = Selected < (VisibleIndex - 1) ? VisibleList[Selected + 1] : VisibleList[0];
        Selected = Selected < (VisibleIndex - 1) ? ++Selected : 0;
        mCurrentSelection = Selected;
        SwitchIconSelection (VisibleIndex, Selected, TRUE);
        PrintTextDescription (MaxStrWidth,
                              Selected,
                              &BootEntries[DefaultEntry]
                              );
        TimeOutSeconds = 0;
        PlayChosen = Context->PickerAudioAssist;
        DrawPointer ();
      } else if (KeyIndex == OC_INPUT_POINTER) {
        HidePointer ();
        SwitchIconSelection (VisibleIndex, Selected, FALSE);
        Selected = mCurrentSelection;
        DefaultEntry = VisibleList[Selected];
        SwitchIconSelection (VisibleIndex, Selected, TRUE);
        PrintTextDescription (MaxStrWidth,
                              Selected,
                              &BootEntries[DefaultEntry]
                              );
        TimeOutSeconds = 0;
        PlayChosen = Context->PickerAudioAssist;
        DrawPointer ();
      } else if (KeyIndex != OC_INPUT_INVALID && (UINTN)KeyIndex < VisibleIndex) {
        ASSERT (KeyIndex >= 0);
        *ChosenBootEntry = &BootEntries[VisibleList[KeyIndex]];
        SetDefault = BootEntries[VisibleList[KeyIndex]].DevicePath != NULL
          && !BootEntries[VisibleList[KeyIndex]].IsAuxiliary
          && Context->AllowSetDefault
          && SetDefault;
        NewDefault = BootEntries[VisibleList[KeyIndex]].DevicePath != NULL
          && !BootEntries[VisibleList[KeyIndex]].IsAuxiliary
          && !Context->AllowSetDefault
          && mDefaultEntry != VisibleList[KeyIndex];
        if (SetDefault || NewDefault) {
          if (SetDefault) {
            OcPlayAudioFile (Context, OcVoiceOverAudioFileSelected, FALSE);
            OcPlayAudioFile (Context, OcVoiceOverAudioFileDefault, FALSE);
            OcPlayAudioEntry (Context, &BootEntries[VisibleList[KeyIndex]], 1 + Selected);
          }
          Status = OcSetDefaultBootEntry (Context, &BootEntries[VisibleList[KeyIndex]]);
          DEBUG ((DEBUG_INFO, "OCSBM: Setting default - %r\n", Status));
        }
        RestoreConsoleMode (Context);
        return EFI_SUCCESS;
      } else if (KeyIndex == OC_INPUT_VOICE_OVER) {
        OcToggleVoiceOver (Context, 0);
        TimeOutSeconds = 0;
        break;
      } else if (KeyIndex != OC_INPUT_TIMEOUT) {
        TimeOutSeconds = 0;
      }

      if (!TimeoutExpired) {
        HidePointer ();
        PrintDateTime (ShowAll);
        TimeoutExpired = PrintTimeOutMessage (TimeOutSeconds);
        TimeOutSeconds = TimeoutExpired ? 10000 : TimeOutSeconds;
        DrawPointer ();
      } else {
        HidePointer ();
        PrintDateTime (ShowAll);
        DrawPointer ();
      }
    }
  }

  ASSERT (FALSE);
}
