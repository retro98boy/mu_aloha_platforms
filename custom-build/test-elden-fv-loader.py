#!/usr/bin/env python3
"""Execute the platform FV loader with mock UEFI services; no hardware I/O."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'Platforms/PakalaPkg/Device/lenovo-elden/EldenStorageDxe/EldenStorageDxe.c').read_text()
body = source[source.index('STATIC EFI_STATUS\nEldenStartFvImage '):
              source.index('STATIC EFI_STATUS\nEldenStartSd ')]
prefix = r'''
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
typedef void VOID;
typedef size_t UINTN;
typedef uint32_t UINT32;
typedef bool BOOLEAN;
typedef int EFI_STATUS;
typedef void *EFI_HANDLE;
typedef struct { int unused; } EFI_GUID;
typedef struct { int unused; } EFI_DEVICE_PATH_PROTOCOL;
typedef struct { int unused; } MEDIA_FW_VOL_FILEPATH_DEVICE_PATH;
typedef unsigned EFI_FV_FILETYPE;
typedef unsigned EFI_FV_FILE_ATTRIBUTES;
typedef struct { void *ImageBase; uint64_t ImageSize; } EFI_LOADED_IMAGE_PROTOCOL;
typedef struct FV FV;
struct FV {
 EFI_STATUS (*ReadFile)(FV *, EFI_GUID *, void **, UINTN *, EFI_FV_FILETYPE *, EFI_FV_FILE_ATTRIBUTES *, UINT32 *);
};
typedef FV EFI_FIRMWARE_VOLUME2_PROTOCOL;
#define STATIC static
#define FALSE false
#define TRUE true
#define EFI_SUCCESS 0
#define EFI_NOT_READY 1
#define EFI_NOT_FOUND 2
#define EFI_UNSUPPORTED 3
#define EFI_OUT_OF_RESOURCES 4
#define EFI_SECURITY_VIOLATION 5
#define EFI_DEVICE_ERROR 6
#define EFI_ERROR(x) ((x) != EFI_SUCCESS)
#define EFI_FV_FILETYPE_FREEFORM 2
#define ByProtocol 0
#define DEBUG(x) ((void)0)
static EFI_GUID gEfiFirmwareVolume2ProtocolGuid, gEfiLoadedImageProtocolGuid;
static int mode, loads, starts, unloads, freed, reads;
static int image_token, parent_token;
static EFI_HANDLE gImageHandle = &parent_token;
static EFI_DEVICE_PATH_PROTOCOL fv_path;
static EFI_LOADED_IMAGE_PROTOCOL loaded;
static FV volumes[3];
static EFI_STATUS read_file(FV *fv, EFI_GUID *guid, void **buffer, UINTN *size,
                           EFI_FV_FILETYPE *type, EFI_FV_FILE_ATTRIBUTES *attr, UINT32 *auth) {
 (void)guid; (void)attr; (void)auth;
 assert(buffer == NULL); reads++;
 if (fv == &volumes[0] || mode == 2) return EFI_NOT_FOUND;
 if (mode == 3) return EFI_DEVICE_ERROR;
 *size = 4096; *type = mode == 4 ? 7 : EFI_FV_FILETYPE_FREEFORM;
 return EFI_SUCCESS;
}
static EFI_STATUS locate(int how, EFI_GUID *guid, void *registration, UINTN *count, EFI_HANDLE **handles) {
 assert(how == ByProtocol && guid == &gEfiFirmwareVolume2ProtocolGuid && registration == NULL);
 if (mode == 1) return EFI_NOT_FOUND;
 *handles = malloc(3 * sizeof(**handles)); assert(*handles);
 for (int i = 0; i < 3; i++) (*handles)[i] = &volumes[i];
 *count = 3; return EFI_SUCCESS;
}
static EFI_STATUS protocol(EFI_HANDLE handle, EFI_GUID *guid, void **out) {
 if (guid == &gEfiFirmwareVolume2ProtocolGuid) { *out = handle; return EFI_SUCCESS; }
 assert(guid == &gEfiLoadedImageProtocolGuid && handle == &image_token);
 if (mode == 9) return EFI_DEVICE_ERROR;
 *out = &loaded; return EFI_SUCCESS;
}
static EFI_DEVICE_PATH_PROTOCOL *DevicePathFromHandle(EFI_HANDLE handle) {
 assert(handle == &volumes[1]); return mode == 5 ? NULL : &fv_path;
}
static void EfiInitializeFwVolDevicepathNode(MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *node, EFI_GUID *guid) {
 (void)node; (void)guid;
}
static EFI_DEVICE_PATH_PROTOCOL *AppendDevicePathNode(EFI_DEVICE_PATH_PROTOCOL *path,
                                                     EFI_DEVICE_PATH_PROTOCOL *node) {
 assert(path == &fv_path && node);
 return mode == 6 ? NULL : malloc(sizeof(*path));
}
static void FreePool(void *p) { assert(p); freed++; free(p); }
static EFI_STATUS load(BOOLEAN boot, EFI_HANDLE parent, EFI_DEVICE_PATH_PROTOCOL *path,
                       void *buffer, UINTN size, EFI_HANDLE *image) {
 assert(!boot && parent == gImageHandle && path && !buffer && !size); loads++;
 if (mode == 8) return EFI_DEVICE_ERROR;
 *image = &image_token;
 return mode == 7 ? EFI_SECURITY_VIOLATION : EFI_SUCCESS;
}
static EFI_STATUS unload(EFI_HANDLE image) {
 assert(image == &image_token); unloads++; return EFI_SUCCESS;
}
static EFI_STATUS start(EFI_HANDLE image, UINTN *exit_size, uint16_t **exit_data) {
 assert(image == &image_token && !exit_size && !exit_data); starts++;
 if (mode == 10) {
   // An unsuccessful StartImage unloads its image. Poison the old metadata.
   loaded.ImageBase = NULL; loaded.ImageSize = 0;
   return EFI_DEVICE_ERROR;
 }
 return EFI_SUCCESS;
}
static struct {
 EFI_STATUS (*LocateHandleBuffer)(int, EFI_GUID *, void *, UINTN *, EFI_HANDLE **);
 EFI_STATUS (*HandleProtocol)(EFI_HANDLE, EFI_GUID *, void **);
 EFI_STATUS (*LoadImage)(BOOLEAN, EFI_HANDLE, EFI_DEVICE_PATH_PROTOCOL *, void *, UINTN, EFI_HANDLE *);
 EFI_STATUS (*UnloadImage)(EFI_HANDLE);
 EFI_STATUS (*StartImage)(EFI_HANDLE, UINTN *, uint16_t **);
} services = {locate, protocol, load, unload, start}, *gBS = &services;
'''
suffix = r'''
int main(void) {
 EFI_GUID file = {0};
 for (mode = 0; mode <= 10; mode++) {
   BOOLEAN returned = true; EFI_STATUS status = 99; UINTN base = 99, size = 99;
   loads = starts = unloads = freed = reads = 0;
   loaded.ImageBase = (void *)(uintptr_t)0x12340000; loaded.ImageSize = 0x1f000;
   for (int i = 0; i < 3; i++) volumes[i].ReadFile = read_file;
   EFI_STATUS result = EldenStartFvImage(&file, &returned, &status, &base, &size);
   assert((result == EFI_SUCCESS) == (mode == 0));
   assert(loads <= 1 && starts <= 1); // Never retry a failed selected image in another FV.
   if (mode == 0) {
     assert(returned && status == EFI_SUCCESS && base == 0x12340000 && size == 0x1f000);
   } else {
     assert(base == 0 && size == 0);
     assert(returned == (mode == 10));
     assert(status == (mode == 10 ? EFI_DEVICE_ERROR : EFI_NOT_READY));
   }
   assert(unloads == ((mode == 7 || mode == 9) ? 1 : 0));
   assert(freed == (mode == 1 ? 0 : mode >= 7 || mode == 0 ? 2 : 1));
 }
 puts("PASS: FV selection, dependency-independent loader failures, auth cleanup, no retries and no stale image bounds");
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='elden-fv-loader-') as tmp:
    binary = str(Path(tmp) / 'test')
    subprocess.run(['cc', '-x', 'c', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-o', binary, '-'],
                   input=prefix + body + suffix, text=True, check=True)
    subprocess.run([binary], check=True)
