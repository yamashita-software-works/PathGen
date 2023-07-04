#pragma once 

#ifndef NTAPI
#define NTAPI __stdcall
#endif

EXTERN_C
NTSTATUS
NTAPI
OpenVolume_U(
    UNICODE_STRING *pusVolumeName,
    ULONG Flags,
    HANDLE *pHandle
    );

EXTERN_C
NTSTATUS
NTAPI
OpenVolume(
    PCWSTR VolumeName,
    ULONG Flags,
    HANDLE *pHandle
    );

#define OPEN_READ_DATA  0x1

EXTERN_C
NTSTATUS
NTAPI
OpenRootDirectory(
    PCWSTR pszRootDirectory,
    ULONG Flags,
    HANDLE *pHandle
    );

EXTERN_C
ULONG
NTAPI
GetVolumeDeviceType(
    HANDLE Handle,
    PULONG pDeviceType,
    PULONG pCharacteristics
    );

typedef struct _VOLUME_FS_OBJECTID_INFORMATION
{
    UCHAR ObjectId[16];
    UCHAR ExtendedInfo[48];
} VOLUME_FS_OBJECTID_INFORMATION;

EXTERN_C
ULONG
NTAPI
GetVolumeObjectId(
    HANDLE Handle, //  Handle that Volume device or Root directory
    VOLUME_FS_OBJECTID_INFORMATION *ObjectId
    );

typedef enum {
    FILEFS_VOLUME_INFORMATION = 1,
    FILEFS_LABEL_INFORMATION,
    FILEFS_SIZE_INFORMATION,
    FILEFS_ATTRIBUTE_INFORMATION,
    FILEFS_FULLSIZE_INFORMATION,
    FILEFS_OBJECTID_INFORMATION,
    FILEFS_DRIVERPATH_INFORMATION,
    FILEFS_VOLUMEFLAGS_INFORMATION,
    FILEFS_SECTORSIZE_INFORMATION,
} FILEFS_INFORMATION_CLASS;

EXTERN_C
NTSTATUS
NTAPI
GetVolumeFsInformation(
    IN HANDLE Handle,
    IN FILEFS_INFORMATION_CLASS InfoClass,
    OUT PVOID *Buffer
    );

