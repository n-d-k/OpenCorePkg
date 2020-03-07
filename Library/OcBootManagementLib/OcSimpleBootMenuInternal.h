//
//  OcSimpleBootMenu.h
//
//
//  Created by N-D-K on 1/24/20.
//

#ifndef OcSimpleBootMenuInternal_h
#define OcSimpleBootMenuInternal_h

#include <Guid/AppleVariable.h>

#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleTextOut.h>
#include <Protocol/UgaDraw.h>
#include <Protocol/AppleKeyMapAggregator.h>
#include <Protocol/SimplePointer.h>

#include <IndustryStandard/AppleCsrConfig.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/OcDebugLogLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/OcAppleKeyMapLib.h>
#include <Library/OcBootManagementLib.h>
#include <Library/OcConsoleLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/OcPngLib.h>
#include <Library/OcFileLib.h>
#include <Library/OcStorageLib.h>
#include <Library/OcTimerLib.h>

#include <BootManagementInternal.h>

/*========== Image ==========*/

typedef struct _NDK_UI_IMAGE {
  UINT16                          Width;
  UINT16                          Height;
  BOOLEAN                         IsAlpha;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL   *Bitmap;
} NDK_UI_IMAGE;

/*========== Pointer ==========*/

#define POINTER_WIDTH  32
#define POINTER_HEIGHT 32

typedef enum {
  NoEvents,
  Move,
  LeftClick,
  RightClick,
  DoubleClick,
  ScrollClick,
  ScrollDown,
  ScrollUp,
  LeftMouseDown,
  RightMouseDown,
  MouseMove
} MOUSE_EVENT;

typedef struct {
  INTN     Xpos;
  INTN     Ypos;
  INTN     Width;
  INTN     Height;
} AREA_RECT;

typedef struct _pointers {
  EFI_SIMPLE_POINTER_PROTOCOL *SimplePointerProtocol;
  NDK_UI_IMAGE *Pointer;
  NDK_UI_IMAGE *NewImage;
  NDK_UI_IMAGE *OldImage;
  
  AREA_RECT  NewPlace;
  AREA_RECT  OldPlace;
  
  UINT64  LastClickTime;
  EFI_SIMPLE_POINTER_STATE State;
  MOUSE_EVENT MouseEvent;
} POINTERS;

/*================ ImageSupport.c =============*/

NDK_UI_IMAGE *
CreateImage (
  IN UINT16       Width,
  IN UINT16       Height,
  IN BOOLEAN      IsAlpha
  );

NDK_UI_IMAGE *
CreateFilledImage (
  IN INTN                          Width,
  IN INTN                          Height,
  IN BOOLEAN                       IsAlpha,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Color
  );

NDK_UI_IMAGE *
CopyImage (
  IN NDK_UI_IMAGE   *Image
  );

NDK_UI_IMAGE *
CopyScaledImage (
  IN NDK_UI_IMAGE      *OldImage,
  IN INTN              Ratio
  );

NDK_UI_IMAGE *
DecodePNG (
  IN VOID                          *Buffer,
  IN UINT32                        BufferSize
  );

VOID
BltImage (
  IN NDK_UI_IMAGE        *Image,
  IN INTN                Xpos,
  IN INTN                Ypos
  );

VOID
RestrictImageArea (
  IN     NDK_UI_IMAGE       *Image,
  IN     INTN               AreaXpos,
  IN     INTN               AreaYpos,
  IN OUT INTN               *AreaWidth,
  IN OUT INTN               *AreaHeight
  );

VOID
ComposeImage (
  IN OUT NDK_UI_IMAGE        *Image,
  IN     NDK_UI_IMAGE        *TopImage,
  IN     INTN                Xpos,
  IN     INTN                Ypos
  );

VOID
RawCompose (
  IN OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL *CompBasePtr,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL *TopBasePtr,
  IN     INTN                          Width,
  IN     INTN                          Height,
  IN     INTN                          CompLineOffset,
  IN     INTN                          TopLineOffset
  );

VOID
RawComposeOnFlat (
  IN OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL *CompBasePtr,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL *TopBasePtr,
  IN     INTN                          Width,
  IN     INTN                          Height,
  IN     INTN                          CompLineOffset,
  IN     INTN                          TopLineOffset
  );

VOID
RawCopy (
  IN OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL *CompBasePtr,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL *TopBasePtr,
  IN     INTN                          Width,
  IN     INTN                          Height,
  IN     INTN                          CompLineOffset,
  IN     INTN                          TopLineOffset
  );
//
// Opacity level can be set from 1-255, 0 = Off
//
VOID
RawComposeAlpha (
  IN OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL *CompBasePtr,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL *TopBasePtr,
  IN     INTN                          Width,
  IN     INTN                          Height,
  IN     INTN                          CompLineOffset,
  IN     INTN                          TopLineOffset,
  IN     INTN                          Opacity
  );

VOID
FillImage (
  IN OUT NDK_UI_IMAGE                  *Image,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Color
  );

VOID
FreeImage (
  IN NDK_UI_IMAGE    *Image
  );

/*======= OcSimpleBootMenuInternal.c =========*/

VOID
DrawImageArea (
  IN NDK_UI_IMAGE      *Image,
  IN INTN              AreaXpos,
  IN INTN              AreaYpos,
  IN INTN              AreaWidth,
  IN INTN              AreaHeight,
  IN INTN              ScreenXpos,
  IN INTN              ScreenYpos
  );

#endif /* OcSimpleBootMenuInternal_h */
