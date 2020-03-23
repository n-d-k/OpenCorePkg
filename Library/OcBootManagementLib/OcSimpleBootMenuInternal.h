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
#include <Library/OcPngLib.h>
#include <Library/OcFileLib.h>
#include <Library/OcStorageLib.h>
#include <Library/OcTimerLib.h>

#include <BootManagementInternal.h>

/*========== UI's defined variables ==========*/

#define UI_IMAGE_POINTER              L"EFI\\OC\\Icons\\pointer4k.png"
#define UI_IMAGE_POINTER_ALT          L"EFI\\OC\\Icons\\pointer.png"
#define UI_IMAGE_FONT                 L"EFI\\OC\\Icons\\font.png"
#define UI_IMAGE_FONT_COLOR           L"EFI\\OC\\Icons\\font_color.png"
#define UI_IMAGE_BACKGROUND           L"EFI\\OC\\Icons\\background4k.png"
#define UI_IMAGE_BACKGROUND_ALT       L"EFI\\OC\\Icons\\background.png"
#define UI_IMAGE_BACKGROUND_COLOR     L"EFI\\OC\\Icons\\background_color.png"
#define UI_IMAGE_SELECTOR             L"EFI\\OC\\Icons\\selector4k.png"
#define UI_IMAGE_SELECTOR_ALT         L"EFI\\OC\\Icons\\selector.png"
#define UI_IMAGE_SELECTOR_OFF         L"EFI\\OC\\Icons\\no_selector.png"
#define UI_IMAGE_LABEL                L"EFI\\OC\\Icons\\label.png"
#define UI_IMAGE_LABEL_OFF            L"EFI\\OC\\Icons\\no_label.png"
#define UI_IMAGE_ALPHA_OFF            L"EFI\\OC\\Icons\\no_alpha.png"
#define UI_IMAGE_TEXT_SCALE_OFF       L"EFI\\OC\\Icons\\No_text_scaling.png"
#define UI_IMAGE_ICON_SCALE_OFF       L"EFI\\OC\\Icons\\No_icon_scaling.png"


#define UI_ICON_WIN                   L"EFI\\OC\\Icons\\os_win.icns"
#define UI_ICON_WIN10                 L"EFI\\OC\\Icons\\os_win10.icns"
#define UI_ICON_INSTALL               L"EFI\\OC\\Icons\\os_install.icns"
#define UI_ICON_MAC                   L"EFI\\OC\\Icons\\os_mac.icns"
#define UI_ICON_MAC_CATA              L"EFI\\OC\\Icons\\os_cata.icns"
#define UI_ICON_MAC_MOJA              L"EFI\\OC\\Icons\\os_moja.icns"
#define UI_ICON_MAC_RECOVERY          L"EFI\\OC\\Icons\\os_recovery.icns"
#define UI_ICON_CLONE                 L"EFI\\OC\\Icons\\os_clone.icns"
#define UI_ICON_FREEBSD               L"EFI\\OC\\Icons\\os_freebsd.icns"
#define UI_ICON_LINUX                 L"EFI\\OC\\Icons\\os_linux.icns"
#define UI_ICON_REDHAT                L"EFI\\OC\\Icons\\os_redhat.icns"
#define UI_ICON_UBUNTU                L"EFI\\OC\\Icons\\os_ubuntu.icns"
#define UI_ICON_FEDORA                L"EFI\\OC\\Icons\\os_fedora.icns"
#define UI_ICON_CUSTOM                L"EFI\\OC\\Icons\\os_custom.icns"
#define UI_ICON_SHELL                 L"EFI\\OC\\Icons\\tool_shell.icns"
#define UI_ICON_RESETNVRAM            L"EFI\\OC\\Icons\\func_resetnvram.icns"
#define UI_ICON_UNKNOWN               L"EFI\\OC\\Icons\\os_unknown.icns"

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

#define ICON_OPACITY_LEVEL  240
#define ICON_OPACITY_FULL   0

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

VOID
PrintLabel (
  IN OC_BOOT_ENTRY   *Entries,
  IN UINTN           *VisibleList,
  IN UINTN           VisibleIndex,
  IN INTN            Xpos,
  IN INTN            Ypos
  );

BOOLEAN
MouseInRect (
  IN AREA_RECT     *Place
  );

BOOLEAN
IsMouseInPlace (
  IN INTN          Xpos,
  IN INTN          Ypos,
  IN INTN          AreaWidth,
  IN INTN          AreaHeight
  );

VOID
DrawPointer (
  VOID
  );

VOID
HidePointer (
  VOID
  );

VOID
RedrawPointer (
  VOID
  );

#endif /* OcSimpleBootMenuInternal_h */
