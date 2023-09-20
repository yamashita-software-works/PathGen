#pragma once

#ifndef _USE_CRT_HEAP_MEMORY
#define _USE_CRT_HEAP_MEMORY 0
#endif

#if _USE_CRT_HEAP_MEMORY
#include "..\libcwh\mem.h"
#endif
#include "ntnativeapi.h"
#include "ntnativehelp.h"
#include "ntobjecthelp.h"
#include "ntvolumehelp.h"
#include "ntwin32helper.h"
#include "fileitem.h"

typedef struct _COMMAND_RUN_PATH
{
    UNICODE_STRING FullPath;
    UNICODE_STRING FileName;
    RTL_RELATIVE_NAME_U RelativeDirPart;
    BOOLEAN EnumDirectoryFiles;
} COMMAND_RUN_PATH;

class CCommandRunPath : public COMMAND_RUN_PATH
{
public:
    CCommandRunPath()
    {
        RtlZeroMemory(&FullPath,sizeof(FullPath));
        RtlZeroMemory(&FileName,sizeof(FileName));
        RtlZeroMemory(&RelativeDirPart,sizeof(RelativeDirPart));
        EnumDirectoryFiles = false;
    }
    ~CCommandRunPath()
    {
        RtlReleaseRelativeName(&RelativeDirPart);
        RtlFreeUnicodeString(&FullPath);
        RtlFreeUnicodeString(&FileName);
    }
};

typedef enum
{
    Display,
	Dump,
} ACTION;

typedef struct _VOLUME_TYPE_STRING
{
	PWSTR NtDevicePath;
	PWSTR Drive;
	PWSTR DeviceName;
	PWSTR Guid;
} VOLUME_TYPE_STRING;

typedef struct _COMMAND_RUNTIME_STRUCT
{
    ACTION action;
    CFileList FileList;

	int OutputType;
	PWSTR OutputVolumeString;
	VOLUME_TYPE_STRING VolTypeString;
	NTSTATUS Status;

	union {
		struct {
			ULONG64 ShowHelp  : 1;
			ULONG64 Recursive : 1; 
			ULONG64 ReplaceSlash : 1;
			ULONG64 ShortPathName : 1; 
			ULONG64 LongPathName : 1; 
		};
		ULONG64 Flags;
	};
} COMMAND_RUNTIME_STRUCT;

struct CCommandRunTimeStruct : public COMMAND_RUNTIME_STRUCT
{
    CCommandRunTimeStruct()
    {
        RtlZeroMemory(this,sizeof(COMMAND_RUNTIME_STRUCT));
    }

    ~CCommandRunTimeStruct()
    {
    }

	void AppendFileName(PWSTR psz)
	{
		FileList.Add( new CFileItem(psz) );
	}
};

#define _SafeFreeMemory(p) if( p ) { FreeMemory(p); p = NULL; }
