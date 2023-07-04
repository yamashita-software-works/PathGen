//***************************************************************************
//*                                                                         *
//*  ntfilehelp.cpp                                                         *
//*                                                                         *
//*  Create: 2022-03-29                                                     *
//*                                                                         *
//*  Author: YAMASHITA Katsuhiro                                            *
//*                                                                         *
//*  Copyright (C) YAMASHITA Katsuhiro. All rights reserved.                *
//*  Licensed under the MIT License.                                        *
//*                                                                         *
//***************************************************************************
#include <ntifs.h>
#include <stdio.h>
#include <malloc.h>
#include <strsafe.h>
#include <ntstrsafe.h>
#include <locale.h>
#include <conio.h>
#include <winerror.h>
#include "ntnativeapi.h"
#include "ntnativehelp.h"
#include "ntfileid.h"

HRESULT GetNtPath(PCWSTR DosPathName,PWSTR *NtPath,PCWSTR *NtFileNamePart)
{
    UNICODE_STRING NtPathName = {0};
    if( RtlDosPathNameToNtPathName_U(DosPathName,&NtPathName,(PWSTR*)NtFileNamePart,NULL) )
    {
        *NtPath = NtPathName.Buffer;
        return S_OK;
    }
    return E_FAIL;
}

HRESULT GetNtPath_U(PCWSTR DosPathName,UNICODE_STRING *NtPath,PCWSTR *NtFileNamePart)
{
    if( RtlDosPathNameToNtPathName_U(DosPathName,NtPath,(PWSTR*)NtFileNamePart,NULL) )
    {
        return S_OK;
    }
    return E_FAIL;
}

ULONG
GetPathType(
	PCWSTR pszPath
	)
{
	if( HasPrefix(L"\\Device\\",pszPath) )
	{
		// NT Device Path
		// "\Device\Xxxx"
		// "\Device\HarddiskVolumeX"
		// "\Device\CdRomX"
		// "\Device\LanmanRedirector;\..."
		return PATHTYPE_NT_DEVICE;
	}

	if( HasPrefix(L"\\??\\",pszPath) )
	{
		// DOS Device (NT Object Namespace)
		// "\??\C:"
		// "\??\C:\"
		// "\??\C:\foo"
		// "\??\Volume{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}"
		// "\??\HarddiskVolumeX"
		// "\??\unc\"
		// "\??\unc\Server\Share"
		return PATHTYPE_DOS_DEVICE;
	}

	if( HasPrefix(L"\\\\?\\",pszPath) )
	{
		// Win32 Path
		// "\\?\C:"
		// "\\?\C:\"
		// "\\?\C:\foo"
		// "\\?\Volume{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}"
		return PATHTYPE_WIN32;
	}

	if( HasPrefix(L"\\\\.\\",pszPath) )
	{
		// Win32 Device
		// "\\.\d:
		// "\\.\PhysicalDriveN
		return 0; // not support
	}

	if( HasPrefix(L"\\\\",pszPath) )
	{
		// Win32 UNC 
		// "\\Server\Share"
		// "\\Server"
		return 0; // not support
	}

	if( _DOS_DRIVE_CHAR(pszPath[0]) && pszPath[1] == L':' )
	{
		// DOS Drive
		// "d:\<Path>"
		// "C:\Windows"
		return PATHTYPE_DOS_DRIVE;
	}

	return 0;
}

ULONG
GetPathType_U(
	UNICODE_STRING *pusPath
	)
{
	ULONG Type = 0;
	PWSTR psz = AllocateSzFromUnicodeString(pusPath);
	if( psz ) {
		Type = GetPathType(psz);
		FreeMemory(psz);
	}
	return Type;
}

//----------------------------------------------------------------------------
//
//  OpenFile_W()
//
//----------------------------------------------------------------------------
EXTERN_C
NTSTATUS
NTAPI
OpenFile_W(
	PHANDLE phFile,
	HANDLE hRoot,
	PCWSTR PathName,
	ULONG DesiredAccess,
	ULONG ShareAccess,
	ULONG OpenOptions
	)
{
	OBJECT_ATTRIBUTES ObjectAttributes;
	IO_STATUS_BLOCK IoStatus = {0};
	NTSTATUS Status;
	HANDLE hFile = INVALID_HANDLE_VALUE;
	UNICODE_STRING usPath;

	RtlInitUnicodeString(&usPath,PathName);

	InitializeObjectAttributes(&ObjectAttributes,&usPath,0,hRoot,NULL);

	Status = NtOpenFile(&hFile,
					DesiredAccess,
					&ObjectAttributes,
					&IoStatus,
					ShareAccess,
					OpenOptions);

	if( Status != STATUS_SUCCESS )
	{
		;//todo:
	}

	*phFile = hFile;

	return Status;
}

//----------------------------------------------------------------------------
//
//  OpenFile_U()
//
//----------------------------------------------------------------------------
EXTERN_C
NTSTATUS
NTAPI
OpenFile_U(
	PHANDLE phFile,
	HANDLE hRoot,
	UNICODE_STRING *PathName,
	ULONG DesiredAccess,
	ULONG ShareAccess,
	ULONG OpenOptions
	)
{
	OBJECT_ATTRIBUTES ObjectAttributes;
	IO_STATUS_BLOCK IoStatus = {0};
	NTSTATUS Status;
	HANDLE hFile = INVALID_HANDLE_VALUE;

	InitializeObjectAttributes(&ObjectAttributes,PathName,0,hRoot,NULL);

	Status = NtOpenFile(&hFile,
					DesiredAccess,
					&ObjectAttributes,
					&IoStatus,
					ShareAccess,
					OpenOptions);

	if( Status != STATUS_SUCCESS )
	{
		;//todo:
	}

	*phFile = hFile;

	return Status;
}

//----------------------------------------------------------------------------
//
//  OpenFile_ID()
//
//----------------------------------------------------------------------------
EXTERN_C
NTSTATUS
NTAPI
OpenFile_ID(
	PHANDLE phFile,
	HANDLE hVolume,
	PFS_FILE_ID_DESCRIPTOR FileIdDesc,
	ULONG DesiredAccess,
	ULONG ShareAccess,
	ULONG OpenOptions
	)
{
	OBJECT_ATTRIBUTES ObjectAttributes;
	IO_STATUS_BLOCK IoStatus = {0};
	NTSTATUS Status;
	HANDLE hFile = INVALID_HANDLE_VALUE;
	UNICODE_STRING NtPathName;

	switch( FileIdDesc->Type )
	{
		case FsFileIdType:
			NtPathName.Length = NtPathName.MaximumLength = (USHORT)sizeof(FileIdDesc->FileId);
			NtPathName.Buffer = (PWCH)&FileIdDesc->FileId;
			break;
		case FsObjectIdType:
			NtPathName.Length = NtPathName.MaximumLength = (USHORT)sizeof(FileIdDesc->ObjectId);
			NtPathName.Buffer = (PWCH)&FileIdDesc->ObjectId;
			break;
		case FsExtendedFileIdType:
			NtPathName.Length = NtPathName.MaximumLength = (USHORT)sizeof(FileIdDesc->ExtendedFileId);
			NtPathName.Buffer = (PWCH)&FileIdDesc->ExtendedFileId;
			break;
		default:
			Status = STATUS_INVALID_PARAMETER;
			RtlSetLastWin32Error( RtlNtStatusToDosError(Status) );
			return Status;
	}

	InitializeObjectAttributes(&ObjectAttributes,&NtPathName,0,hVolume,NULL);

	OpenOptions |= FILE_OPEN_BY_FILE_ID;

	Status = NtOpenFile(&hFile,
					DesiredAccess,
					&ObjectAttributes,
					&IoStatus,
					ShareAccess,
					OpenOptions);

	*phFile = hFile;

	RtlSetLastWin32Error( RtlNtStatusToDosError(Status) );

	return Status;
}

//----------------------------------------------------------------------------
//
//  CreateFile_U()
//
//----------------------------------------------------------------------------
EXTERN_C
NTSTATUS
NTAPI
CreateFile_U(
	PHANDLE FileHandle,
	HANDLE hRoot,
	UNICODE_STRING *NtFilePath,
	PVOID SecurityDescriptor,
	PLARGE_INTEGER AllocationSize,
	ULONG DesiredAccess,
	ULONG FileAttributes,
	ULONG ShareAccess,
	ULONG CreateDisposition,
	ULONG CreateOptions,
	PVOID EaBuffer,
	ULONG EaLength
    )
{
	NTSTATUS Status;
	IO_STATUS_BLOCK IoStatus;
	OBJECT_ATTRIBUTES ObjectAttributes;

	InitializeObjectAttributes(&ObjectAttributes,NtFilePath,0,hRoot,SecurityDescriptor);

	Status = NtCreateFile(FileHandle,
						DesiredAccess,
						&ObjectAttributes,
						&IoStatus,
						AllocationSize,
						FileAttributes,
						ShareAccess,
						CreateDisposition,
						CreateOptions,
						EaBuffer,
						EaLength);

	if( Status != STATUS_SUCCESS )
	{
		;//todo:
	}
	return Status;
}

//----------------------------------------------------------------------------
//
//  CreateDirectory_W()
//
//----------------------------------------------------------------------------
EXTERN_C
LONG
NTAPI
CreateDirectory_W(
	HANDLE hRoot,
	LPCWSTR NewDirectory,
	SECURITY_ATTRIBUTES *SecurityAttributes
	)
{
	HANDLE hDir = NULL;
	IO_STATUS_BLOCK IoStatus = {0};
	UNICODE_STRING NtPathName = {0,0,NULL};
	NTSTATUS Status;

	RtlInitUnicodeString( &NtPathName, NewDirectory );

	Status = CreateFile_U(
				&hDir,
				hRoot,
				&NtPathName,
				SecurityAttributes != NULL ? SecurityAttributes->lpSecurityDescriptor : NULL,
				NULL,
				SYNCHRONIZE|FILE_LIST_DIRECTORY,
				FILE_ATTRIBUTE_NORMAL,
				FILE_SHARE_READ|FILE_SHARE_WRITE,
				FILE_CREATE,
				FILE_OPEN_REPARSE_POINT|FILE_OPEN_FOR_BACKUP_INTENT|
				FILE_SYNCHRONOUS_IO_NONALERT|FILE_DIRECTORY_FILE,
				NULL,0);

	if( hDir != NULL && hDir != INVALID_HANDLE_VALUE )
		NtClose(hDir);

	return Status;
}

//----------------------------------------------------------------------------
//
//  GetFileDateTime_U()
//
//----------------------------------------------------------------------------
NTSTATUS GetFileDateTime_U( HANDLE RootHandle, UNICODE_STRING *FilePath, FILE_BASIC_INFORMATION *pbi )
{
    if( FilePath == NULL || pbi == NULL )
        return STATUS_INVALID_PARAMETER;

    OBJECT_ATTRIBUTES ObjectAttributes;
    InitializeObjectAttributes(&ObjectAttributes,FilePath,0,RootHandle,NULL);

    HANDLE Handle;
    NTSTATUS Status;
    IO_STATUS_BLOCK IoStatus = {0};

    Status = NtOpenFile(&Handle,FILE_READ_ATTRIBUTES|FILE_WRITE_ATTRIBUTES,
                    &ObjectAttributes,&IoStatus,FILE_SHARE_READ|FILE_SHARE_WRITE,0);

    if( Status == STATUS_SUCCESS )
    {
        FILE_BASIC_INFORMATION fbi = {0};

        Status = NtQueryInformationFile(Handle,&IoStatus,&fbi,sizeof(fbi),FileBasicInformation);

        if( Status == STATUS_SUCCESS )
        {
            *pbi = fbi;
        }

        NtClose(Handle);
    }

    return Status;
}

//----------------------------------------------------------------------------
//
//  GetFileDateTime()
//
//----------------------------------------------------------------------------
NTSTATUS GetFileDateTime( HANDLE RootHandle, PCWSTR FilePath, FILE_BASIC_INFORMATION *pfbi )
{
    if( FilePath == NULL || pfbi == NULL )
        return STATUS_INVALID_PARAMETER;

	UNICODE_STRING usPath;
	RtlInitUnicodeString(&usPath,FilePath);
	return GetFileDateTime_U(RootHandle, &usPath, pfbi);
}

//----------------------------------------------------------------------------
//
//  SetFileDateTime_U()
//
//----------------------------------------------------------------------------
NTSTATUS SetFileDateTime_U( HANDLE RootHandle, UNICODE_STRING *FilePath, FILE_BASIC_INFORMATION *pBasicInfo )
{
    OBJECT_ATTRIBUTES ObjectAttributes;

    InitializeObjectAttributes(&ObjectAttributes,FilePath,0,RootHandle,NULL);

    HANDLE Handle;
    NTSTATUS Status;
    IO_STATUS_BLOCK IoStatus = {0};

    Status = NtOpenFile(&Handle,FILE_WRITE_ATTRIBUTES,
                    &ObjectAttributes,&IoStatus,FILE_SHARE_READ|FILE_SHARE_WRITE,0);

    if( Status == STATUS_SUCCESS )
    {
        FILE_BASIC_INFORMATION fbi = {0};

        fbi = *pBasicInfo;
//		fbi.FileAttributes = (ULONG)-1; // no change file attributes
		fbi.FileAttributes = (ULONG)0;  // no change file attributes

        Status = NtSetInformationFile(Handle,&IoStatus,&fbi,sizeof(fbi),FileBasicInformation);

        NtClose(Handle);
    }

	return Status;
}

//----------------------------------------------------------------------------
//
//  SetFileDateTime()
//
//----------------------------------------------------------------------------
NTSTATUS SetFileDateTime( HANDLE RootHandle, PCWSTR FilePath, FILE_BASIC_INFORMATION *pfbi )
{
    if( FilePath == NULL || pfbi == NULL )
        return STATUS_INVALID_PARAMETER;

	UNICODE_STRING usPath;
	RtlInitUnicodeString(&usPath,FilePath);
	return SetFileDateTime_U(RootHandle, &usPath, pfbi);
}

//----------------------------------------------------------------------------
//
//  GetFileStandardInformation_U()
//
//----------------------------------------------------------------------------
NTSTATUS GetFileStandardInformation_U( HANDLE RootHandle, UNICODE_STRING *FilePath, FILE_STANDARD_INFORMATION *pfsi )
{
    if( FilePath == NULL || pfsi == NULL )
        return STATUS_INVALID_PARAMETER;

    OBJECT_ATTRIBUTES ObjectAttributes;
    InitializeObjectAttributes(&ObjectAttributes,FilePath,0,RootHandle,NULL);

    HANDLE Handle;
    NTSTATUS Status;
    IO_STATUS_BLOCK IoStatus = {0};

    Status = NtOpenFile(&Handle,FILE_READ_ATTRIBUTES|FILE_WRITE_ATTRIBUTES,
                    &ObjectAttributes,&IoStatus,FILE_SHARE_READ|FILE_SHARE_WRITE,0);

    if( Status == STATUS_SUCCESS )
    {
        FILE_STANDARD_INFORMATION fsi = {0};

        Status = NtQueryInformationFile(Handle,&IoStatus,&fsi,sizeof(fsi),FileStandardInformation);

        if( Status == STATUS_SUCCESS )
        {
            *pfsi = fsi;
        }

        NtClose(Handle);
    }

    return Status;
}

//----------------------------------------------------------------------------
//
//  GetFileStandardInformation()
//
//----------------------------------------------------------------------------
NTSTATUS GetFileStandardInformation( HANDLE RootHandle, PCWSTR FilePath, FILE_STANDARD_INFORMATION *pfsi )
{
    if( FilePath == NULL || pfsi == NULL )
        return STATUS_INVALID_PARAMETER;

	UNICODE_STRING usPath;
	RtlInitUnicodeString(&usPath,FilePath);
	return GetFileStandardInformation_U(RootHandle, &usPath, pfsi);
}

//----------------------------------------------------------------------------
//
//  GetDirectoryFileInformation_U()
//
//----------------------------------------------------------------------------
EXTERN_C
NTSTATUS
NTAPI
GetDirectoryFileInformation_U(
	HANDLE hDirectory,
	UNICODE_STRING *pusFileName,
	FS_FILE_DIRECTORY_INFORMATION *pInfoBuffer
	)
{
	FILE_ID_BOTH_DIR_INFORMATION *pBuffer;
	ULONG cbBuffer;
	IO_STATUS_BLOCK IoStatus;
	NTSTATUS Status;

	cbBuffer = sizeof(FILE_ID_BOTH_DIR_INFORMATION) + DOS_MAX_COMPONENT_BYTES;
	pBuffer = (FILE_ID_BOTH_DIR_INFORMATION *)AllocMemory(cbBuffer);

	Status = NtQueryDirectoryFile(hDirectory,NULL,NULL,NULL,&IoStatus,
					pBuffer,cbBuffer,
					FileIdBothDirectoryInformation,
					TRUE,
					pusFileName,
					TRUE);

	if( Status == STATUS_SUCCESS && pInfoBuffer )
	{
		pInfoBuffer->EaSize          = sizeof(FS_FILE_DIRECTORY_INFORMATION);
#if 0
		pInfoBuffer->CreationTime    = pBuffer->CreationTime;
		pInfoBuffer->LastAccessTime  = pBuffer->LastAccessTime;
		pInfoBuffer->LastWriteTime   = pBuffer->LastWriteTime;
		pInfoBuffer->ChangeTime      = pBuffer->ChangeTime;
		pInfoBuffer->EndOfFile       = pBuffer->EndOfFile;
		pInfoBuffer->AllocationSize  = pBuffer->AllocationSize;
		pInfoBuffer->FileAttributes  = pBuffer->FileAttributes;
#else
		memcpy(&pInfoBuffer->CreationTime,&pBuffer->FileIndex,FS_DIRINFO_COMMON_COPY_SIZE);
#endif
		pInfoBuffer->EaSize          = pBuffer->EaSize;
		pInfoBuffer->ShortNameLength = pBuffer->ShortNameLength;
		pInfoBuffer->FileId          = pBuffer->FileId;
		memcpy(pInfoBuffer->ShortName,pBuffer->ShortName,pBuffer->ShortNameLength);
		pInfoBuffer->ShortName[WCHAR_LENGTH(pBuffer->ShortNameLength)] = UNICODE_NULL;
	}

	return Status;
}

//----------------------------------------------------------------------------
//
//  GetDirectoryFileInformation()
//
//----------------------------------------------------------------------------
EXTERN_C
NTSTATUS
NTAPI
GetDirectoryFileInformation(
	HANDLE hDirectory,
	PCWSTR pszFileName,
	FS_FILE_DIRECTORY_INFORMATION *pInfoBuffer
	)
{
	UNICODE_STRING usFileName;

	RtlInitUnicodeString(&usFileName,pszFileName);

	return GetDirectoryFileInformation_U(hDirectory,&usFileName,pInfoBuffer);
}

//----------------------------------------------------------------------------
//
//  MakeSureDirectoryPathExists_W()
//
//----------------------------------------------------------------------------
EXTERN_C
NTSTATUS
NTAPI
MakeSureDirectoryPathExists_W(
	PCWSTR DirPath
	)
{
	UNICODE_STRING RootDirectory;
	UNICODE_STRING RootRelativePath;
	NTSTATUS Status;
	PWSTR pszFullPath;

	pszFullPath = DuplicateString(DirPath);

	if( pszFullPath == NULL )
	{
		return STATUS_NO_MEMORY;
	}

	if( !SplitRootRelativePath(pszFullPath,&RootDirectory,&RootRelativePath) )
	{
		FreeMemory(pszFullPath);
		return STATUS_INVALID_PARAMETER;
	}

	HANDLE hParentDir;
	Status = OpenFile_U(&hParentDir,NULL,&RootDirectory,FILE_GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,FILE_DIRECTORY_FILE);

	if( Status == STATUS_SUCCESS )
	{
		WCHAR seps[] = L"\\";
		WCHAR *token = NULL;
		WCHAR *next_token = NULL;
		HANDLE hCreatedDir;

	    token = wcstok_s((PWSTR)RootRelativePath.Buffer,seps,&next_token);

	    while( token != NULL )
		{
			UNICODE_STRING token_u;
			RtlInitUnicodeString(&token_u,token);

			if( !PathFileExists_UEx(hParentDir,&token_u,NULL) )
			{
				Status = CreateDirectory(hParentDir,token,NULL);
				if( Status != STATUS_SUCCESS && Status != STATUS_OBJECT_NAME_COLLISION )
					break;
			}

			Status = OpenFile_U(&hCreatedDir,hParentDir,&token_u,
						FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,
						FILE_DIRECTORY_FILE);

			if( Status != STATUS_SUCCESS )
				break;

			NtClose(hParentDir);
			hParentDir = hCreatedDir;

			token = wcstok_s(NULL,seps,&next_token);
		}

		NtClose(hParentDir);
	}

	FreeMemory(pszFullPath);

	return Status;
}

//----------------------------------------------------------------------------
//
//  RenameDirectoryEntry_U()
//
//----------------------------------------------------------------------------
EXTERN_C
NTSTATUS
NTAPI
RenameDirectoryEntry_U(
	HANDLE hExistingDirectory,
	UNICODE_STRING *pusExistingFilePath,
	HANDLE hDestinationDirectory,
	UNICODE_STRING *pusNewFileName,
	BOOLEAN ReplaceIfExists
	)
{
	NTSTATUS Status;
	HANDLE hFile;

	if( pusExistingFilePath == NULL || pusNewFileName == NULL )
	{
		_SetLastStatusDos( STATUS_INVALID_PARAMETER );
		return STATUS_INVALID_PARAMETER;
	}

	ULONG cbMoveBufferLength = sizeof(FILE_RENAME_INFORMATION) + pusNewFileName->MaximumLength;
	FILE_RENAME_INFORMATION *pMoveBuffer = (FILE_RENAME_INFORMATION *)AllocMemory( cbMoveBufferLength );
	if( pMoveBuffer == NULL )
	{
		_SetLastStatusDos( STATUS_NO_MEMORY );
		return STATUS_NO_MEMORY;
	}

	//
	// Target source file handle is must be source directory relative open.
	//
	Status = OpenFile_U(&hFile,hExistingDirectory,
					pusExistingFilePath,
					FILE_GENERIC_READ|FILE_GENERIC_WRITE|DELETE,
					FILE_SHARE_READ|FILE_SHARE_WRITE,
					FILE_OPEN_FOR_BACKUP_INTENT);

	if( Status == STATUS_SUCCESS )
	{
		IO_STATUS_BLOCK IoStatus;

		//
		// A handle that the target directory.
		//
		// If the file is not being moved to a different directory, or if the FileName member
		// contains the full pathname, this member is NULL. Otherwise, it is a handle 
		// for the root directory under which the file will reside after it is renamed.
		//
		pMoveBuffer->RootDirectory   = hDestinationDirectory;

		//
		//If the RootDirectory member is NULL, and the file is being moved to a different directory,
		// this member specifies the full pathname to be assigned to the file. Otherwise, it specifies
		// only the file name or a relative pathname.
		//
		pMoveBuffer->FileNameLength  = pusNewFileName->Length;
		RtlCopyMemory(pMoveBuffer->FileName,pusNewFileName->Buffer,pusNewFileName->Length);

		//
		// Set to TRUE to specify that if a file with the given name already exists, 
		// it should be replaced with the given file. Set to FALSE if the rename operation 
		// should fail if a file with the given name already exists.
		//
		pMoveBuffer->ReplaceIfExists = ReplaceIfExists;

		Status = NtSetInformationFile(hFile,&IoStatus,pMoveBuffer,cbMoveBufferLength,FileRenameInformation);

		NtClose(hFile);
	}

	FreeMemory( pMoveBuffer );

	_SetLastStatusDos( Status );

	return Status;
}

//----------------------------------------------------------------------------
//
//  RenameDirectoryEntry()
//
//----------------------------------------------------------------------------
EXTERN_C
NTSTATUS
NTAPI
RenameDirectoryEntry(
	HANDLE hExistingDirectory,
	PCWSTR pszExistingFileName,
	HANDLE hDestinationDirectory,
	PCWSTR pszNewFileName,
	BOOLEAN ReplaceIfExists
	)
{
	UNICODE_STRING usExistingFileName;
	UNICODE_STRING usNewFileName;

	if( hDestinationDirectory == NULL || pszExistingFileName == NULL || pszNewFileName == NULL )
	{
		_SetLastStatusDos( STATUS_INVALID_PARAMETER );
		return STATUS_INVALID_PARAMETER;
	}

	RtlInitUnicodeString(&usExistingFileName,pszExistingFileName);
	RtlInitUnicodeString(&usNewFileName,pszNewFileName);

	return RenameDirectoryEntry_U(hExistingDirectory,&usExistingFileName,
				hDestinationDirectory,&usNewFileName,ReplaceIfExists);
}

//----------------------------------------------------------------------------
//
//  MoveDirectoryEntry()
//
//----------------------------------------------------------------------------
EXTERN_C
NTSTATUS
NTAPI
MoveDirectoryEntry(
	PCWSTR pszSourceFilePath,
	PCWSTR pszDestinationFilePath,
	BOOLEAN ReplaceIfExists
	)
{
	LONG Status = 0;
	HANDLE hSrcDir = NULL;
	HANDLE hDstDir = NULL;

	UNICODE_STRING SourceDirectory;
	UNICODE_STRING DestinationDirectory;
	UNICODE_STRING FileName;

	RtlInitUnicodeString(&SourceDirectory,pszSourceFilePath);
	SplitPathFileName_U(&SourceDirectory,&FileName);

	RtlInitUnicodeString(&DestinationDirectory,pszDestinationFilePath);
	SplitPathFileName_U(&DestinationDirectory,NULL);

#ifdef _DEBUG
	UNICODE_STRING us1,us2;
	RtlDuplicateUnicodeString(0x3,&SourceDirectory,&us1);
	RtlDuplicateUnicodeString(0x3,&DestinationDirectory,&us2);
	RtlFreeUnicodeString(&us1);
	RtlFreeUnicodeString(&us2);
#endif

	__try
	{
		Status = OpenFile_U(&hSrcDir,NULL,&SourceDirectory,
							FILE_GENERIC_READ,
							FILE_SHARE_READ|FILE_SHARE_WRITE,
							FILE_DIRECTORY_FILE|FILE_OPEN_FOR_BACKUP_INTENT);

		if( Status != STATUS_SUCCESS )
		{
			__leave;
		}

		Status = OpenFile_U(&hDstDir,NULL,&DestinationDirectory,
							FILE_GENERIC_READ,
							FILE_SHARE_READ|FILE_SHARE_WRITE,
							FILE_DIRECTORY_FILE|FILE_OPEN_FOR_BACKUP_INTENT);

		if( Status != STATUS_SUCCESS )
		{
			__leave;
		}

		Status = RenameDirectoryEntry(hSrcDir,FileName.Buffer,hDstDir,FileName.Buffer,ReplaceIfExists);

	}
	__finally
	{
		if( hSrcDir )
			NtClose(hSrcDir);
		if( hDstDir )
			NtClose(hDstDir);
	}

	return Status;
}

//----------------------------------------------------------------------------
//
//  GetShortPath_W()
//
//----------------------------------------------------------------------------
//
//  - This parameter necessary to be NT object namespace path.
//
//  - Uses this function instead of GetShortPathName() function.
//
EXTERN_C
NTSTATUS
NTAPI
GetShortPath_W(
	PCWSTR pszFullPath,
	PWSTR pszShortPathBuffer,
	ULONG cchShortPathBuffer
	)
{
	UNICODE_STRING usRoot;
	UNICODE_STRING usRootRelativePath;
	PWSTR path,sep,part;
	NTSTATUS Status;

	SplitRootRelativePath(pszFullPath,&usRoot,&usRootRelativePath);

	if( usRootRelativePath.Length == 0 )
		return STATUS_INVALID_PARAMETER; // no path part

	//
	// Copy short name root prefix
	//
	if( cchShortPathBuffer < WCHAR_LENGTH(usRoot.Length) )
		return STATUS_BUFFER_TOO_SMALL;

	memcpy(pszShortPathBuffer,usRoot.Buffer,usRoot.Length);
	
	//
	// Open root directory
	//
	HANDLE hRoot;
	OpenFile_U(&hRoot,NULL,&usRoot,
			FILE_LIST_DIRECTORY|SYNCHRONIZE,
			FILE_SHARE_READ|FILE_SHARE_WRITE,
			FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT);

	path = part = AllocateSzFromUnicodeString( &usRootRelativePath );

	for(;;)
	{
		// Find separator. if found, replace to null character to terminate a path.
		sep = wcschr(part,L'\\');
		if( sep )
			*sep = L'\0';
		else
			sep = part;

		UNICODE_STRING usPath;
		UNICODE_STRING usFileName;
	
		RtlInitUnicodeString(&usPath,path);
		SplitPathFileName_U(&usPath,&usFileName);

		HANDLE hDir;
		Status = OpenFile_U(&hDir,hRoot,&usPath,
						FILE_LIST_DIRECTORY|SYNCHRONIZE,
						FILE_SHARE_READ|FILE_SHARE_WRITE,
						FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT);

		if( Status == STATUS_SUCCESS )
		{
			FS_FILE_DIRECTORY_INFORMATION di = {0};

			Status = GetDirectoryFileInformation_U(hDir,&usFileName,&di);

			NtClose(hDir);

			if( Status == STATUS_SUCCESS )
			{
				if( di.ShortNameLength > 0 )
					Status = RtlStringCchCatW(pszShortPathBuffer,cchShortPathBuffer,di.ShortName);
				else
					Status = RtlStringCchCatW(pszShortPathBuffer,cchShortPathBuffer,usFileName.Buffer);

				if( sep != part )
					Status = RtlStringCchCatW(pszShortPathBuffer,cchShortPathBuffer,L"\\");

				if( Status != STATUS_SUCCESS )
					break;
			}
		}
		else
		{
			break;
		}

		if( sep == part )
			break; // last part, loop out.

		*sep = L'\\';

		part = sep + 1; // to the next part.
	}

	FreeMemory(path);

	return Status;
}
