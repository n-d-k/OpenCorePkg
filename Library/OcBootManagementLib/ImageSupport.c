//
//  ImageSupport.c
//  
//
//  Created by N-D-K on 1/21/20.
//

#include <OcSimpleBootMenuInternal.h>

VOID
FreeImage (
  IN NDK_UI_IMAGE    *Image
  )
{
  if (Image != NULL) {
    if (Image->Bitmap != NULL) {
      FreePool (Image->Bitmap);
      Image->Bitmap = NULL;
    }
    FreePool (Image);
  }
}

NDK_UI_IMAGE *
CreateImage (
  IN UINT16       Width,
  IN UINT16       Height,
  IN BOOLEAN      IsAlpha
  )
{
  NDK_UI_IMAGE    *NewImage;
  
  NewImage = (NDK_UI_IMAGE *) AllocateZeroPool (sizeof (NDK_UI_IMAGE));
  
  if (NewImage == NULL) {
    return NULL;
  }
  
  if (Width * Height == 0) {
    FreeImage (NewImage);
    return NULL;
  }
  
  NewImage->Bitmap = AllocateZeroPool (Width * Height * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
  if (NewImage->Bitmap == NULL) {
    FreePool (NewImage);
    return NULL;
  }
  
  NewImage->Width = Width;
  NewImage->Height = Height;
  NewImage->IsAlpha = IsAlpha;
  
  return NewImage;
}

VOID
RestrictImageArea (
  IN     NDK_UI_IMAGE       *Image,
  IN     INTN               AreaXpos,
  IN     INTN               AreaYpos,
  IN OUT INTN               *AreaWidth,
  IN OUT INTN               *AreaHeight
  )
{
  if (Image == NULL || AreaWidth == NULL || AreaHeight == NULL) {
    DEBUG ((DEBUG_INFO, "OCUI: invalid argument\n"));
    return;
  }

  if (AreaXpos >= Image->Width || AreaYpos >= Image->Height) {
    *AreaWidth  = 0;
    *AreaHeight = 0;
  } else {
    if (*AreaWidth > Image->Width - AreaXpos) {
      *AreaWidth = Image->Width - AreaXpos;
    }
    if (*AreaHeight > Image->Height - AreaYpos) {
      *AreaHeight = Image->Height - AreaYpos;
    }
  }
}

VOID
ComposeImage (
  IN OUT NDK_UI_IMAGE        *Image,
  IN     NDK_UI_IMAGE        *TopImage,
  IN     INTN                Xpos,
  IN     INTN                Ypos
  )
{
  INTN                       CompWidth;
  INTN                       CompHeight;
  
  if (TopImage == NULL || Image == NULL) {
    return;
  }

  CompWidth  = TopImage->Width;
  CompHeight = TopImage->Height;
  RestrictImageArea (Image, Xpos, Ypos, &CompWidth, &CompHeight);

  if (CompWidth > 0) {
    if (TopImage->IsAlpha) {
      if (Image->IsAlpha) {
        RawCompose (Image->Bitmap + Ypos * Image->Width + Xpos,
                    TopImage->Bitmap,
                    CompWidth,
                    CompHeight,
                    Image->Width,
                    TopImage->Width
                    );
      } else {
        RawComposeOnFlat (Image->Bitmap + Ypos * Image->Width + Xpos,
                          TopImage->Bitmap,
                          CompWidth,
                          CompHeight,
                          Image->Width,
                          TopImage->Width
                          );
      }
    } else {
      RawCopy (Image->Bitmap + Ypos * Image->Width + Xpos,
               TopImage->Bitmap,
               CompWidth,
               CompHeight,
               Image->Width,
               TopImage->Width
               );
    }
  }
}

VOID
RawCompose (
  IN OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL *CompBasePtr,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL *TopBasePtr,
  IN     INTN                          Width,
  IN     INTN                          Height,
  IN     INTN                          CompLineOffset,
  IN     INTN                          TopLineOffset
  )
{
  INT64                                X;
  INT64                                Y;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL        *TopPtr;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL        *CompPtr;
  INTN                                 TopAlpha;
  INTN                                 Alpha;
  INTN                                 CompAlpha;
  INTN                                 RevAlpha;
  INTN                                 TempAlpha;

  if (CompBasePtr == NULL || TopBasePtr == NULL) {
    return;
  }

  for (Y = 0; Y < Height; ++Y) {
    TopPtr = TopBasePtr;
    CompPtr = CompBasePtr;
    for (X = 0; X < Width; ++X) {
      TopAlpha = TopPtr->Reserved & 0xFF;
      
      if (TopAlpha == 255) {
        CompPtr->Blue  = TopPtr->Blue;
        CompPtr->Green = TopPtr->Green;
        CompPtr->Red   = TopPtr->Red;
        CompPtr->Reserved = (UINT8) TopAlpha;
      } else if (TopAlpha != 0) {
        CompAlpha = CompPtr->Reserved & 0xFF;
        RevAlpha = 255 - TopAlpha;
        TempAlpha = CompAlpha * RevAlpha;
        TopAlpha *= 255;
        Alpha = TopAlpha + TempAlpha;

        CompPtr->Blue = (UINT8) ((TopPtr->Blue * TopAlpha + CompPtr->Blue * TempAlpha) / Alpha);
        CompPtr->Green = (UINT8) ((TopPtr->Green * TopAlpha + CompPtr->Green * TempAlpha) / Alpha);
        CompPtr->Red = (UINT8) ((TopPtr->Red * TopAlpha + CompPtr->Red * TempAlpha) / Alpha);
        CompPtr->Reserved = (UINT8) (Alpha / 255);
      }
      TopPtr++;
      CompPtr++;
    }
    TopBasePtr += TopLineOffset;
    CompBasePtr += CompLineOffset;
  }
}

VOID
RawComposeOnFlat (
  IN OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL *CompBasePtr,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL *TopBasePtr,
  IN     INTN                          Width,
  IN     INTN                          Height,
  IN     INTN                          CompLineOffset,
  IN     INTN                          TopLineOffset
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL        *TopPtr;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL        *CompPtr;
  UINT32                               TopAlpha;
  UINT32                               RevAlpha;
  UINTN                                Temp;
  INT64                                X;
  INT64                                Y;

  if (CompBasePtr == NULL || TopBasePtr == NULL) {
    return;
  }

  for (Y = 0; Y < Height; ++Y) {
    TopPtr = TopBasePtr;
    CompPtr = CompBasePtr;
    for (X = 0; X < Width; ++X) {
      TopAlpha = TopPtr->Reserved;
      RevAlpha = 255 - TopAlpha;

      Temp = ((UINT8) CompPtr->Blue * RevAlpha) + ((UINT8) TopPtr->Blue * TopAlpha);
      CompPtr->Blue = (UINT8) (Temp / 255);

      Temp = ((UINT8) CompPtr->Green * RevAlpha) + ((UINT8) TopPtr->Green * TopAlpha);
      CompPtr->Green = (UINT8) (Temp / 255);

      Temp = ((UINT8) CompPtr->Red * RevAlpha) + ((UINT8) TopPtr->Red * TopAlpha);
      CompPtr->Red = (UINT8) (Temp / 255);

      CompPtr->Reserved = (UINT8)(255);

      TopPtr++;
      CompPtr++;
    }
    TopBasePtr += TopLineOffset;
    CompBasePtr += CompLineOffset;
  }
}

VOID
RawCopy (
  IN OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL *CompBasePtr,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL *TopBasePtr,
  IN     INTN                          Width,
  IN     INTN                          Height,
  IN     INTN                          CompLineOffset,
  IN     INTN                          TopLineOffset
  )
{
  INTN       X;
  INTN       Y;

  if (CompBasePtr == NULL || TopBasePtr == NULL) {
    return;
  }

  for (Y = 0; Y < Height; ++Y) {
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *TopPtr = TopBasePtr;
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *CompPtr = CompBasePtr;
    for (X = 0; X < Width; ++X) {
      *CompPtr = *TopPtr;
      TopPtr++;
      CompPtr++;
    }
    TopBasePtr += TopLineOffset;
    CompBasePtr += CompLineOffset;
  }
}

VOID
RawComposeAlpha (
  IN OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL *CompBasePtr,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL *TopBasePtr,
  IN     INTN                          Width,
  IN     INTN                          Height,
  IN     INTN                          CompLineOffset,
  IN     INTN                          TopLineOffset,
  IN     INTN                          Opacity
  )
{
  INTN       X;
  INTN       Y;
  INTN       Alpha;
  INTN       InvAlpha;
  INTN       TopAlpha;

  if (CompBasePtr == NULL || TopBasePtr == NULL) {
    return;
  }
  
  if (Opacity == 0) {
    RawCompose (CompBasePtr,
                TopBasePtr,
                Width,
                Height,
                CompLineOffset,
                TopLineOffset
                );
    return;
  }
  
  for (Y = 0; Y < Height; ++Y) {
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *TopPtr = TopBasePtr;
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *CompPtr = CompBasePtr;
    for (X = 0; X < Width; ++X) {
      TopAlpha = TopPtr->Reserved & 0xFF;
      if (TopAlpha != 0) {
        Alpha =  (Opacity * TopAlpha) / 255;
        InvAlpha = 255 - ((Opacity * TopAlpha) / 255);
        CompPtr->Blue = (UINT8) ((TopPtr->Blue * Alpha + CompPtr->Blue * InvAlpha) >> 8);
        CompPtr->Green = (UINT8) ((TopPtr->Green * Alpha + CompPtr->Green * InvAlpha) >> 8);
        CompPtr->Red = (UINT8) ((TopPtr->Red * Alpha + CompPtr->Red * InvAlpha) >> 8);
        CompPtr->Reserved = (UINT8) (255);
      }
      TopPtr++;
      CompPtr++;
    }
    TopBasePtr += TopLineOffset;
    CompBasePtr += CompLineOffset;
  }
}

VOID
RawComposeColor (
  IN OUT EFI_GRAPHICS_OUTPUT_BLT_PIXEL *CompBasePtr,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL *TopBasePtr,
  IN     INTN                          Width,
  IN     INTN                          Height,
  IN     INTN                          CompLineOffset,
  IN     INTN                          TopLineOffset,
  IN     INTN                          ColorDiff
  )
{
  INT64                                X;
  INT64                                Y;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL        *TopPtr;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL        *CompPtr;
  INTN                                 TopAlpha;
  INTN                                 Alpha;
  INTN                                 CompAlpha;
  INTN                                 RevAlpha;
  INTN                                 TempAlpha;
  INTN                                 TempColor;
  INTN                                 Base;

  if (CompBasePtr == NULL || TopBasePtr == NULL) {
    return;
  }
  
  if (ColorDiff == 0) {
    RawCompose (CompBasePtr,
                TopBasePtr,
                Width,
                Height,
                CompLineOffset,
                TopLineOffset
                );
    return;
  } else if (ColorDiff > 0) {
    Base = 255;
  } else {
    Base = 0;
  }
  
  for (Y = 0; Y < Height; ++Y) {
    TopPtr = TopBasePtr;
    CompPtr = CompBasePtr;
    for (X = 0; X < Width; ++X) {
      TopAlpha = TopPtr->Reserved & 0xFF;
      if (TopAlpha == 255) {
        TempColor = Base == 0 ? TopPtr->Blue + (TopPtr->Blue * ColorDiff / 255) : MIN (TopPtr->Blue + (TopPtr->Blue * ColorDiff / 255), Base);
        CompPtr->Blue  = (UINT8) TempColor;
        TempColor = Base == 0 ? TopPtr->Green + (TopPtr->Green * ColorDiff / 255) : MIN (TopPtr->Green + (TopPtr->Green * ColorDiff / 255), Base);
        CompPtr->Green = (UINT8) TempColor;
        TempColor = Base == 0 ? TopPtr->Red + (TopPtr->Red * ColorDiff / 255) : MIN (TopPtr->Red + (TopPtr->Red * ColorDiff / 255), Base);
        CompPtr->Red   = (UINT8) TempColor;
        CompPtr->Reserved = (UINT8) TopAlpha;
      } else if (TopAlpha != 0) {
        CompAlpha = CompPtr->Reserved & 0xFF;
        RevAlpha = 255 - TopAlpha;
        TempAlpha = CompAlpha * RevAlpha;
        TopAlpha *= 255;
        Alpha = TopAlpha + TempAlpha;

        CompPtr->Blue = (UINT8) ((Base == 0 ? TopPtr->Blue + (TopPtr->Blue * ColorDiff / 255) : MIN (TopPtr->Blue + (TopPtr->Blue * ColorDiff / 255), Base) * TopAlpha + CompPtr->Blue * TempAlpha) / Alpha);
        CompPtr->Green = (UINT8) ((Base == 0 ? TopPtr->Green + (TopPtr->Green * ColorDiff / 255) : MIN (TopPtr->Green + (TopPtr->Green * ColorDiff / 255), Base) * TopAlpha + CompPtr->Green * TempAlpha) / Alpha);
        CompPtr->Red = (UINT8) ((Base == 0 ? TopPtr->Red + (TopPtr->Red * ColorDiff / 255) : MIN (TopPtr->Red + (TopPtr->Red * ColorDiff / 255), Base) * TopAlpha + CompPtr->Red * TempAlpha) / Alpha);
        CompPtr->Reserved = (UINT8) (Alpha / 255);
      }
      TopPtr++;
      CompPtr++;
    }
    TopBasePtr += TopLineOffset;
    CompBasePtr += CompLineOffset;
  }
}

VOID
FillImage (
  IN OUT NDK_UI_IMAGE                  *Image,
  IN     EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Color
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL        FillColor;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL        *PixelPtr;
  INTN                                 Index;
  
  if (Image == NULL || Color == NULL) {
    return;
  }
  

  if (!Image->IsAlpha) {
    FillColor.Reserved = 0;
  }

  FillColor = *Color;

  PixelPtr = Image->Bitmap;
  for (Index = 0; Index < Image->Width * Image->Height; ++Index) {
    *PixelPtr++ = FillColor;
  }
}

NDK_UI_IMAGE *
CreateFilledImage (
  IN INTN                          Width,
  IN INTN                          Height,
  IN BOOLEAN                       IsAlpha,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Color
  )
{
  NDK_UI_IMAGE      *NewImage;
  
  NewImage = CreateImage (Width, Height, IsAlpha);
  if (NewImage == NULL) {
    return NULL;
  }
  
  FillImage (NewImage, Color);
  
  return NewImage;
}

NDK_UI_IMAGE *
CopyImage (
  IN NDK_UI_IMAGE   *Image
  )
{
  NDK_UI_IMAGE      *NewImage;
  if (Image == NULL || (Image->Width * Image->Height) == 0) {
    return NULL;
  }

  NewImage = CreateImage (Image->Width, Image->Height, Image->IsAlpha);
  if (NewImage == NULL) {
    return NULL;
  }
  
  CopyMem (NewImage->Bitmap, Image->Bitmap, (UINTN) (Image->Width * Image->Height * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL)));
  return NewImage;
}

NDK_UI_IMAGE *
CopyScaledImage (
  IN NDK_UI_IMAGE      *OldImage,
  IN INTN              Ratio
  )
{
  BOOLEAN                             Grey = FALSE;
  NDK_UI_IMAGE                        *NewImage;
  INTN                                x, x0, x1, x2, y, y0, y1, y2;
  INTN                                NewH, NewW;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL       *Dest;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL       *Src;
  INTN                                OldW;

  if (Ratio < 0) {
    Ratio = -Ratio;
    Grey = TRUE;
  }

  if (OldImage == NULL) {
    return NULL;
  }
  Src =  OldImage->Bitmap;
  OldW = OldImage->Width;

  NewW = (OldImage->Width * Ratio) >> 4;
  NewH = (OldImage->Height * Ratio) >> 4;


  if (Ratio == 16) {
    NewImage = CopyImage (OldImage);
  } else {
    NewImage = CreateImage (NewW, NewH, OldImage->IsAlpha);
    if (NewImage == NULL) {
      return NULL;
    }
    Dest = NewImage->Bitmap;
    for (y = 0; y < NewH; y++) {
      y1 = (y << 4) / Ratio;
      y0 = ((y1 > 0) ? (y1-1) : y1) * OldW;
      y2 = ((y1 < (OldImage->Height - 1)) ? (y1+1) : y1) * OldW;
      y1 *= OldW;
      for (x = 0; x < NewW; x++) {
        x1 = (x << 4) / Ratio;
        x0 = (x1 > 0) ? (x1 - 1) : x1;
        x2 = (x1 < (OldW - 1)) ? (x1+1) : x1;
        Dest->Blue = (UINT8)(((INTN)Src[x1+y1].Blue * 2 + Src[x0+y1].Blue +
                           Src[x2+y1].Blue + Src[x1+y0].Blue + Src[x1+y2].Blue) / 6);
        Dest->Green = (UINT8)(((INTN)Src[x1+y1].Green * 2 + Src[x0+y1].Green +
                           Src[x2+y1].Green + Src[x1+y0].Green + Src[x1+y2].Green) / 6);
        Dest->Red = (UINT8)(((INTN)Src[x1+y1].Red * 2 + Src[x0+y1].Red +
                           Src[x2+y1].Red + Src[x1+y0].Red + Src[x1+y2].Red) / 6);
        Dest->Reserved = Src[x1+y1].Reserved;
        Dest++;
      }
    }
  }
  if (Grey) {
    Dest = NewImage->Bitmap;
    for (y = 0; y < NewH; y++) {
      for (x = 0; x < NewW; x++) {
        Dest->Blue = (UINT8)((INTN)((UINTN)Dest->Blue + (UINTN)Dest->Green + (UINTN)Dest->Red) / 3);
        Dest->Green = Dest->Red = Dest->Blue;
        Dest++;
      }
    }
  }

  return NewImage;
}

VOID
BltImage (
  IN NDK_UI_IMAGE        *Image,
  IN INTN                Xpos,
  IN INTN                Ypos
  )
{
  if (Image == NULL) {
    return;
  }
  
  DrawImageArea (Image, 0, 0, 0, 0, Xpos, Ypos);
}

NDK_UI_IMAGE *
DecodePNG (
  IN VOID                          *Buffer,
  IN UINT32                        BufferSize
  )
{
  EFI_STATUS                       Status;
  NDK_UI_IMAGE                     *NewImage;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *Pixel;
  VOID                             *Data;
  UINT32                           Width;
  UINT32                           Height;
  UINT8                            *DataWalker;
  UINTN                            X;
  UINTN                            Y;
  BOOLEAN                          IsAlpha;
  
  if (Buffer == NULL) {
    return NULL;
  }
  
  Status = DecodePng (
               Buffer,
               BufferSize,
               &Data,
               &Width,
               &Height,
               &IsAlpha
              );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "OCUI: DecodePNG...%r\n", Status));
    if (Buffer != NULL) {
      FreePool (Buffer);
    }
    return NULL;
  }
    
  NewImage = CreateImage ((INTN) Width, (INTN) Height, IsAlpha);
  if (NewImage == NULL) {
    if (Buffer != NULL) {
      FreePool (Buffer);
    }
    return NULL;
  }
  
  Pixel = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *) NewImage->Bitmap;
  DataWalker = (UINT8 *) Data;
  for (Y = 0; Y < NewImage->Height; Y++) {
    for (X = 0; X < NewImage->Width; X++) {
      Pixel->Red = *DataWalker++;
      Pixel->Green = *DataWalker++;
      Pixel->Blue = *DataWalker++;
      Pixel->Reserved = *DataWalker++;
      Pixel++;
    }
  }
  
  if (Buffer != NULL) {
    FreePool (Buffer);
  }
  
  if (Data != NULL) {
    FreePool (Data);
  }
  
  NewImage->IsAlpha = IsAlpha;
  return NewImage;
}
