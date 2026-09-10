/** @file

  Copyright (c) 2022-2024 DuoWoA authors

  SPDX-License-Identifier: MIT

**/
#include <PiDxe.h>

#include <Library/ArmLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryMapHelperLib.h>
#include <Library/SerialPortLib.h>

#include <Resources/FbColor.h>
#include <Resources/font5x12.h>

#include <Library/FrameBufferSerialPortLib.h>

ARM_MEMORY_REGION_DESCRIPTOR_EX DisplayMemoryRegion;

FBCON_POSITION* p_Position = NULL;
FBCON_POSITION m_MaxPosition;
FBCON_COLOR    m_Color;
BOOLEAN        m_Initialized = FALSE;

UINTN gWidth = FixedPcdGet32(PcdMipiFrameBufferWidth);
// Reserve half screen for output
UINTN gHeight = FixedPcdGet32(PcdMipiFrameBufferHeight);
UINTN gBpp    = FixedPcdGet32(PcdMipiFrameBufferPixelBpp);

// Module-used internal routine
void FbConPutCharWithFactor(char c, int type, unsigned scale_factor);

void FbConDrawglyph(
    char *pixels, unsigned stride, unsigned bpp, unsigned *glyph,
    unsigned scale_factor);

void FbConReset(void);
void FbConScrollUp(void);
void FbConFlush(void);

#ifdef ELDEN_COMPACT_LOG
STATIC UINTN mDirtyStart = MAX_UINTN;
STATIC UINTN mDirtyEnd;
STATIC VOID EldenDirtyRows(UINTN First, UINTN Rows)
{
  UINTN Start = First * gWidth * 4;
  UINTN End = (First + Rows) * gWidth * 4;
  if (Start < mDirtyStart) mDirtyStart = Start;
  if (End > mDirtyEnd) mDirtyEnd = End;
}
#endif

RETURN_STATUS
EFIAPI
SerialPortInitialize(VOID)
{
  // Prevent dup initialization
  if (m_Initialized)
    return RETURN_SUCCESS;

  EFI_STATUS Status = LocateMemoryMapAreaByName("Display Reserved", &DisplayMemoryRegion);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  // The shared cursor follows the pixel buffer. Validate before touching either.
  if (gWidth < (FONT_WIDTH + 1) * SCALE_FACTOR ||
      gHeight < FONT_HEIGHT * (SCALE_FACTOR + 1) || gBpp != 32 ||
      (UINT64)gWidth * gHeight * 4 + sizeof(FBCON_POSITION) > DisplayMemoryRegion.Length) {
    return RETURN_UNSUPPORTED;
  }
  
  p_Position = (FBCON_POSITION*)(DisplayMemoryRegion.Address + (FixedPcdGet32(PcdMipiFrameBufferWidth) * FixedPcdGet32(PcdMipiFrameBufferHeight) * FixedPcdGet32(PcdMipiFrameBufferPixelBpp) / 8));

  // Reset console
  FbConReset();
#ifdef ELDEN_FRAMEBUFFER_CONSOLE
  // PrePi explicitly resets once per boot. Later module instances preserve
  // the shared cursor, but never trust out-of-range coordinates.
  if (p_Position->x < 0 || p_Position->x >= (INTN)(m_MaxPosition.x / SCALE_FACTOR) ||
      p_Position->y < 0 || p_Position->y >= (INTN)(m_MaxPosition.y - SCALE_FACTOR)) {
    p_Position->x = 0;
    p_Position->y = 0;
  }
#else
  p_Position->x = 0;
  p_Position->y = 0;
#endif

  // Set flag
  m_Initialized = TRUE;

  return RETURN_SUCCESS;
}

// Called only at the Elden PrePi C entry, never by DXE constructors.
VOID EFIAPI EldenResetScreenLog(VOID)
{
  if (RETURN_ERROR(SerialPortInitialize()))
    return;
  p_Position->x = 0;
  p_Position->y = 0;
  ResetFb();
  FbConFlush();
}

void ResetFb(void)
{
#ifdef ELDEN_COMPACT_LOG
  SetMem32((VOID *)(UINTN)DisplayMemoryRegion.Address,
           gWidth * gHeight * 4, FB_BGRA8888_BLACK);
  EldenDirtyRows(0, gHeight);
#else
  // Clear current screen.
  char *Pixels  = (void *)DisplayMemoryRegion.Address;
  UINTN BgColor = FB_BGRA8888_BLACK;

  // Set to black color.
  for (UINTN i = 0; i < gWidth; i++) {
    for (UINTN j = 0; j < gHeight; j++) {
      BgColor = FB_BGRA8888_BLACK;
      // Set pixel bit
      for (UINTN p = 0; p < (gBpp / 8); p++) {
        *Pixels = (unsigned char)BgColor;
        BgColor = BgColor >> 8;
        Pixels++;
      }
    }
  }
#endif
}

void FbConReset(void)
{
  // Calc max position.
  m_MaxPosition.x = gWidth / (FONT_WIDTH + 1);
  m_MaxPosition.y = (gHeight - 1) / FONT_HEIGHT;

  // Reset color.
  m_Color.Foreground = FB_BGRA8888_WHITE;
  m_Color.Background = FB_BGRA8888_BLACK;
}

void FbConPutCharWithFactor(char c, int type, unsigned scale_factor)
{
  char *Pixels;
  BOOLEAN intstate = ArmGetInterruptState();

#ifndef ELDEN_FRAMEBUFFER_CONSOLE
paint:
#endif

  if ((unsigned char)c > 127)
    return;

  if ((unsigned char)c < 32) {
    if (c == '\n') {
      goto newline;
    }
    else if (c == '\r') {
      p_Position->x = 0;
      return;
    }
    else {
      return;
    }
  }

  // Save some space
  if (p_Position->x == 0 && (unsigned char)c == ' ' &&
      type != FBCON_SUBTITLE_MSG && type != FBCON_TITLE_MSG)
    return;

  if (intstate)
    ArmDisableInterrupts();

  Pixels = (void *)DisplayMemoryRegion.Address;
  Pixels += p_Position->y * ((gBpp / 8) * FONT_HEIGHT * gWidth);
  Pixels += p_Position->x * scale_factor * ((gBpp / 8) * (FONT_WIDTH + 1));

  FbConDrawglyph(
      Pixels, gWidth, (gBpp / 8), font5x12 + (c - 32) * 2, scale_factor);
#ifdef ELDEN_COMPACT_LOG
  EldenDirtyRows(p_Position->y * FONT_HEIGHT, FONT_HEIGHT * scale_factor);
#endif

  p_Position->x++;

  if (p_Position->x >= (int)(m_MaxPosition.x / scale_factor))
    goto newline;

  if (intstate)
    ArmEnableInterrupts();
  return;

newline:
  p_Position->y += scale_factor;
  p_Position->x = 0;
  if (p_Position->y >= m_MaxPosition.y - scale_factor) {
#ifdef ELDEN_FRAMEBUFFER_CONSOLE
    FbConScrollUp();
    p_Position->y -= scale_factor;
    if (intstate)
      ArmEnableInterrupts();
    return;
#else
    ResetFb();
    FbConFlush();
    p_Position->y = 0;

    if (intstate)
      ArmEnableInterrupts();
    goto paint;
#endif
  }
  else {
#ifndef ELDEN_COMPACT_LOG
    FbConFlush();
#endif
    if (intstate)
      ArmEnableInterrupts();
  }
}

void FbConDrawglyph(
    char *pixels, unsigned stride, unsigned bpp, unsigned *glyph,
    unsigned scale_factor)
{
  char *       bg_pixels = pixels;
  unsigned     x, y, i, j, k;
  unsigned     data, temp;
  unsigned int fg_color = m_Color.Foreground;
  unsigned int bg_color = m_Color.Background;
  stride -= FONT_WIDTH * scale_factor;

  for (y = 0; y < FONT_HEIGHT / 2; ++y) {
    for (i = 0; i < scale_factor; i++) {
      for (x = 0; x < FONT_WIDTH; ++x) {
        for (j = 0; j < scale_factor; j++) {
          bg_color = m_Color.Background;
          for (k = 0; k < bpp; k++) {
            *bg_pixels = (unsigned char)bg_color;
            bg_color   = bg_color >> 8;
            bg_pixels++;
          }
        }
      }
      bg_pixels += (stride * bpp);
    }
  }

  for (y = 0; y < FONT_HEIGHT / 2; ++y) {
    for (i = 0; i < scale_factor; i++) {
      for (x = 0; x < FONT_WIDTH; ++x) {
        for (j = 0; j < scale_factor; j++) {
          bg_color = m_Color.Background;
          for (k = 0; k < bpp; k++) {
            *bg_pixels = (unsigned char)bg_color;
            bg_color   = bg_color >> 8;
            bg_pixels++;
          }
        }
      }
      bg_pixels += (stride * bpp);
    }
  }

  data = glyph[0];
  for (y = 0; y < FONT_HEIGHT / 2; ++y) {
    temp = data;
    for (i = 0; i < scale_factor; i++) {
      data = temp;
      for (x = 0; x < FONT_WIDTH; ++x) {
        if (data & 1) {
          for (j = 0; j < scale_factor; j++) {
            fg_color = m_Color.Foreground;
            for (k = 0; k < bpp; k++) {
              *pixels  = (unsigned char)fg_color;
              fg_color = fg_color >> 8;
              pixels++;
            }
          }
        }
        else {
          for (j = 0; j < scale_factor; j++) {
            pixels = pixels + bpp;
          }
        }
        data >>= 1;
      }
      pixels += (stride * bpp);
    }
  }

  data = glyph[1];
  for (y = 0; y < FONT_HEIGHT / 2; ++y) {
    temp = data;
    for (i = 0; i < scale_factor; i++) {
      data = temp;
      for (x = 0; x < FONT_WIDTH; ++x) {
        if (data & 1) {
          for (j = 0; j < scale_factor; j++) {
            fg_color = m_Color.Foreground;
            for (k = 0; k < bpp; k++) {
              *pixels  = (unsigned char)fg_color;
              fg_color = fg_color >> 8;
              pixels++;
            }
          }
        }
        else {
          for (j = 0; j < scale_factor; j++) {
            pixels = pixels + bpp;
          }
        }
        data >>= 1;
      }
      pixels += (stride * bpp);
    }
  }
}

/* TODO: Take stride into account */
void FbConScrollUp(void)
{
#ifdef ELDEN_COMPACT_LOG
  // Normal mapped framebuffer RAM. CopyMem explicitly supports overlap.
  UINTN RowBytes = FONT_HEIGHT * SCALE_FACTOR * gWidth * 4;
  UINTN Bytes = gWidth * gHeight * 4;
  UINT8 *Pixels = (VOID *)(UINTN)DisplayMemoryRegion.Address;
  CopyMem(Pixels, Pixels + RowBytes, Bytes - RowBytes);
  SetMem32(Pixels + Bytes - RowBytes, RowBytes, (UINT32)m_Color.Background);
  EldenDirtyRows(0, gHeight);
#elif defined(ELDEN_FRAMEBUFFER_CONSOLE)
  // 32-bit framebuffer, one scaled text row. Forward copy handles overlap.
  volatile UINT32 *dst = (volatile UINT32 *)(UINTN)DisplayMemoryRegion.Address;
  UINTN row = FONT_HEIGHT * SCALE_FACTOR;
  UINTN count = gWidth * (gHeight - row);
  for (UINTN i = 0; i < count; i++)
    dst[i] = dst[i + gWidth * row];
  for (UINTN i = count; i < gWidth * gHeight; i++)
    dst[i] = (UINT32)m_Color.Background;
#else
  unsigned short *dst   = (void *)DisplayMemoryRegion.Address;
  unsigned short *src   = dst + (gWidth * FONT_HEIGHT);
  unsigned        count = gWidth * (gHeight - FONT_HEIGHT);

  while (count--) {
    *dst++ = *src++;
  }

  count = gWidth * FONT_HEIGHT;
  while (count--) {
    *dst++ = m_Color.Background;
  }
#endif

#ifndef ELDEN_COMPACT_LOG
  FbConFlush();
#endif
}

void FbConFlush(void)
{
#ifdef ELDEN_COMPACT_LOG
  // Flush once per SerialPortWrite, not once per newline plus once per write.
  // Clean without invalidation: scrolling immediately reads these pixels again.
  if (mDirtyStart < mDirtyEnd) {
    WriteBackDataCacheRange((VOID *)(UINTN)(DisplayMemoryRegion.Address + mDirtyStart),
                           mDirtyEnd - mDirtyStart);
  }
  mDirtyStart = MAX_UINTN;
  mDirtyEnd = 0;
#else
  unsigned total_x, total_y;
  unsigned bytes_per_bpp;

  total_x       = gWidth;
  total_y       = gHeight;
  bytes_per_bpp = (gBpp / 8);

  WriteBackInvalidateDataCacheRange(
      (void *)DisplayMemoryRegion.Address,
      (total_x * total_y * bytes_per_bpp));
#endif
}

#ifdef ELDEN_COMPACT_LOG
// ELDEN_FILTER_BEGIN: operate on bounded writes, never assume a trailing NUL.
STATIC BOOLEAN
EldenHideRoutineLog(CONST UINT8 *Buffer, UINTN Size)
{
  CONST CHAR8 *Prefixes[] = {"PROGRESS CODE:", "Loading driver at ",
                             "Memory Allocation "};
  UINTN I, J;
  // Filter only one complete line. Fragmented/multiline messages pass intact,
  // so an ASSERT following a routine message cannot be accidentally hidden.
  if (Size == 0 || Buffer[Size - 1] != '\n') return FALSE;
  for (I = 0; I + 1 < Size; I++) if (Buffer[I] == '\n') return FALSE;
  for (I = 0; I < sizeof(Prefixes)/sizeof(Prefixes[0]); I++) {
    for (J = 0; Prefixes[I][J] != 0 && J < Size &&
                Buffer[J] == (UINT8)Prefixes[I][J]; J++) {}
    if (Prefixes[I][J] == 0) return TRUE;
  }
  return FALSE;
}
// ELDEN_FILTER_END
#endif

UINTN
EFIAPI
SerialPortWrite(IN UINT8 *Buffer, IN UINTN NumberOfBytes)
{
#ifdef ELDEN_COMPACT_LOG
  if (EldenHideRoutineLog(Buffer, NumberOfBytes)) return NumberOfBytes;
#endif
  if (RETURN_ERROR(SerialPortInitialize()))
    return 0;
  UINT8 *CONST Final          = &Buffer[NumberOfBytes];
  UINTN        InterruptState = ArmGetInterruptState();

  if (InterruptState)
    ArmDisableInterrupts();

  while (Buffer < Final) {
    FbConPutCharWithFactor(*Buffer++, FBCON_COMMON_MSG, SCALE_FACTOR);
  }

  FbConFlush();

  if (InterruptState)
    ArmEnableInterrupts();
  return NumberOfBytes;
}

UINTN
EFIAPI
SerialPortWriteCritical(IN UINT8 *Buffer, IN UINTN NumberOfBytes)
{
  if (RETURN_ERROR(SerialPortInitialize()))
    return 0;
  UINT8 *CONST Final             = &Buffer[NumberOfBytes];
  UINTN        CurrentForeground = m_Color.Foreground;
  UINTN        InterruptState    = ArmGetInterruptState();

  if (InterruptState)
    ArmDisableInterrupts();

  m_Color.Foreground = FB_BGRA8888_YELLOW;

  while (Buffer < Final) {
    FbConPutCharWithFactor(*Buffer++, FBCON_COMMON_MSG, SCALE_FACTOR);
  }

  m_Color.Foreground = CurrentForeground;
  FbConFlush();

  if (InterruptState)
    ArmEnableInterrupts();
  return NumberOfBytes;
}

UINTN
EFIAPI
SerialPortRead(OUT UINT8 *Buffer, IN UINTN NumberOfBytes) { return 0; }

BOOLEAN
EFIAPI
SerialPortPoll(VOID) { return FALSE; }

RETURN_STATUS
EFIAPI
SerialPortSetControl(IN UINT32 Control) { return RETURN_UNSUPPORTED; }

RETURN_STATUS
EFIAPI
SerialPortGetControl(OUT UINT32 *Control) { return RETURN_UNSUPPORTED; }

RETURN_STATUS
EFIAPI
SerialPortSetAttributes(
    IN OUT UINT64 *BaudRate, IN OUT UINT32 *ReceiveFifoDepth,
    IN OUT UINT32 *Timeout, IN OUT EFI_PARITY_TYPE *Parity,
    IN OUT UINT8 *DataBits, IN OUT EFI_STOP_BITS_TYPE *StopBits)
{
  return RETURN_UNSUPPORTED;
}

UINTN SerialPortFlush(VOID) { return 0; }

VOID EnableSynchronousSerialPortIO(VOID)
{
  // Already synchronous
}
