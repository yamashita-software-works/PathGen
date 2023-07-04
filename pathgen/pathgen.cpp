//***************************************************************************
//*                                                                         *
//*  PathGen.cpp                                                            *
//*                                                                         *
//*  Create: 2023-06-03                                                     *
//*                                                                         *
//*  Author: YAMASHITA Katsuhiro                                            *
//*                                                                         *
//*  Copyright (C) YAMASHITA Katsuhiro. All rights reserved.                *
//*  Licensed under the MIT License.                                        *
//*                                                                         *
//***************************************************************************
#include <ntifs.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <ntstrsafe.h>
#include <locale.h>
#include <conio.h>
#include <winerror.h>
#include <crtdbg.h>
#include "pathgen.h"

extern int PrintError( LONG ErrorCode, PCWSTR pszParam = NULL );
extern void PrintHelp();

inline void _ReplaceBackslashToSlash(UNICODE_STRING& us)
{
	for(int i = 0; i < (int)WCHAR_LENGTH(us.Length); i++ )
	{
		if( us.Buffer[i] == L'\\' )
			us.Buffer[i] = L'/';
	}
}

enum {
	OUTPUT_UNKNOWN=0,
	OUTPUT_NT_DEVICE,
	OUTPUT_NT_DOSDRIVE,
	OUTPUT_NT_DOSGUID,
	OUTPUT_NT_DOSDEVICE,
	OUTPUT_DOS_DRIVE,
	OUTPUT_WIN32_DRIVE,
	OUTPUT_WIN32_GUID,
	OUTPUT_WIN32_DEVICE, // not "\\.\" prefix
	OUTPUT_ROOT,
	OUTPUT_DUMP,
};

enum {
	DOS_UNKNOWN=0,
	DOS_DRIVE,
	DOS_GUID,
	DOS_DEVICE,
	DOS_PARTITION,
	DOS_STORAGE,
};

//----------------------------------------------------------------------------
//
//  _GetDosDeviceType()
//
//----------------------------------------------------------------------------
ULONG _GetDosDeviceType(PCWSTR psz)
{
	// "Volume{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}"
	if( IsStringVolumeGuid(psz) )
		return DOS_GUID;
	// "C:"
	if( _DOS_DRIVE_CHAR(psz[0]) && psz[1] == L':' )
		return DOS_DRIVE;
	// "STORAGE#..."
	if( _wcsnicmp(psz,L"STORAGE",7) == 0 )
		return DOS_STORAGE;
	if( wcsstr(psz,L"Partition") )
		return DOS_PARTITION;
	// "HarddiskVolumeX","CdromX",
	// "SystemPartition","BootPartition"
	return DOS_DEVICE;
}

//----------------------------------------------------------------------------
//
//  _QueryDosDeviceName()
//
//----------------------------------------------------------------------------
PWSTR _QueryDosDeviceName(PCWSTR pszDosDevice)
{
	WCHAR buf[WIN32_MAX_PATH];
	if( QuerySymbolicLinkObjectName( L"\\??",pszDosDevice,buf,WIN32_MAX_PATH,NULL) == STATUS_SUCCESS )
	{
		return DuplicateString(buf);
	}
	return NULL;
}

//----------------------------------------------------------------------------
//
//  _PrepareVolumeNameString()
//
//----------------------------------------------------------------------------
BOOLEAN _PrepareVolumeNameString(ULONG OutputType,PCWSTR pszNtVolumePath,PCWSTR pszDeviceName,VOLUME_TYPE_STRING *pVolTypeString)
{
	HANDLE hspa = NULL;
	PCWSTR psz;
	ULONG DosDevType;
	ULONG Location;

	for( Location = 0; Location <= 1; Location++)
	{
		if( EnumDosDeviceTargetNames(&hspa, pszNtVolumePath, Location) == STATUS_SUCCESS )
		{
			int i,c = GetDosDeviceTargetNamesCount(hspa);

			for(i = 0; i < c; i++)
			{
				psz = GetDosDeviceTargetNamesItem(hspa,i,NULL);

				DosDevType = _GetDosDeviceType(psz);
				switch( DosDevType )
				{
					case DOS_DRIVE:
						ASSERT( pVolTypeString->Drive == NULL );
						if( pVolTypeString->Drive == NULL )
							pVolTypeString->Drive = DuplicateString(psz);
						break;
					case DOS_GUID:
						ASSERT( pVolTypeString->Guid == NULL );
						if( pVolTypeString->Guid == NULL )
							pVolTypeString->Guid = DuplicateString(psz);
						break;
					case DOS_DEVICE:
						ASSERT( pVolTypeString->DeviceName == NULL );
						if( pVolTypeString->DeviceName == NULL )
							pVolTypeString->DeviceName = DuplicateString(psz);
						break;
				}
			}

			FreeDosDeviceTargetNames(hspa);
		}
	}

	return TRUE;
}

//----------------------------------------------------------------------------
//
//  _AddCurrentDirectory()
//
//----------------------------------------------------------------------------
void _AddCurrentDirectory(CCommandRunTimeStruct *pcs)
{
	WCHAR szCurDir[WIN32_MAX_PATH];
	RtlGetCurrentDirectory_U(WIN32_MAX_PATH,szCurDir);
	CombinePathBuffer(szCurDir,WIN32_MAX_PATH,szCurDir,L"*");
	pcs->AppendFileName( szCurDir );
}

//----------------------------------------------------------------------------
//
//  _MakeShortPathName()
//
//----------------------------------------------------------------------------
void _MakeShortPathName(CCommandRunTimeStruct *pcs,UNICODE_STRING *pusRelativePath,UNICODE_STRING *pusPath)
{
	PWSTR pszNtFullPath;

	int cch = (int)(wcslen(pcs->VolTypeString.NtDevicePath) + WCHAR_LENGTH(pusRelativePath->Length) + 1);
	pszNtFullPath = AllocStringBuffer( cch );

	if( pszNtFullPath )
	{
		RtlStringCchCopyW(pszNtFullPath,cch,pcs->VolTypeString.NtDevicePath);
		RtlStringCchCatW(pszNtFullPath,cch,pusRelativePath->Buffer);

		ULONG cchShortPathName = (ULONG)wcslen(pszNtFullPath) + 1;
		PWSTR pszShortPathName = AllocStringBuffer( cchShortPathName );

		if( pszShortPathName )
		{
			if( GetShortPath_W( pszNtFullPath, pszShortPathName , cchShortPathName ) == STATUS_SUCCESS )
			{
				UNICODE_STRING us;
				SplitVolumeRelativePath(pszShortPathName,NULL,&us);
				DuplicateUnicodeString(pusPath,&us);
			}
			else
			{
				DuplicateUnicodeString(pusPath,pusRelativePath);
			}
			FreeMemory(pszShortPathName);
		}
		FreeMemory(pszNtFullPath);
	}
}

//----------------------------------------------------------------------------
//
//  _PrintPathString()
//
//----------------------------------------------------------------------------
BOOLEAN _PrintPathString(CCommandRunTimeStruct *pcs,UNICODE_STRING *pusPath)
{
	UNICODE_STRING usPath = *pusPath;
	UNICODE_STRING usVolumeRelativePath;
	UNICODE_STRING usVolume;
	PWSTR pszVolumeName = NULL;
	PWSTR pszObjectNamespaceVolumeName = NULL;
	BOOLEAN Success = FALSE;

	__try
	{
		SplitVolumeRelativePath_U(&usPath,&usVolume,&usVolumeRelativePath);

		pszVolumeName = AllocateSzFromUnicodeString(&usVolume);
		if( pszVolumeName == NULL )
			__leave;

		//
		// Get source device type
		//
		ULONG PathType = GetPathType(pszVolumeName);

		//
		// Create source NT device volume name
		//
		if( PathType != PATHTYPE_NT_DEVICE )
		{
			pszObjectNamespaceVolumeName = _QueryDosDeviceName(&pszVolumeName[4]);
		}
		else
		{
			WCHAR *psz = wcsrchr(pszVolumeName,L'\\');
			if( psz )
			{
				pszObjectNamespaceVolumeName = _QueryDosDeviceName( psz + 1 ); // Normalized Device Path
			}
			else
			{
				pszObjectNamespaceVolumeName = DuplicateString(pszVolumeName);
			}
		}

		if( pszObjectNamespaceVolumeName == NULL )
			__leave;

		//
		// NT device name
		//
		WCHAR *pszDeviceName = wcsrchr(pszObjectNamespaceVolumeName,L'\\');
		if( pszDeviceName )
		{
			pszDeviceName++;
		}

		//
		// As the volume prefixes is uninitialized at the first time call, 
		// to initialize this, and use the same in subsequent calls.
		// 
		if( pcs->OutputVolumeString == NULL )
		{
			pcs->VolTypeString.NtDevicePath = DuplicateString(pszObjectNamespaceVolumeName);
			_PrepareVolumeNameString(0,pszObjectNamespaceVolumeName,pszDeviceName,&pcs->VolTypeString);

			PWSTR pszPrefix;
			PWSTR pszVolume;
			switch( pcs->OutputType )
			{
				case OUTPUT_NT_DEVICE:
					pszPrefix = NULL;
					pszVolume = pcs->VolTypeString.NtDevicePath;
					break;
				case OUTPUT_NT_DOSDRIVE:
					pszPrefix = L"\\??\\";
					pszVolume = pcs->VolTypeString.Drive;
					break;
				case OUTPUT_NT_DOSGUID:
					pszPrefix = L"\\??\\";
					pszVolume = pcs->VolTypeString.Guid;
					break;
				case OUTPUT_NT_DOSDEVICE:
					pszPrefix = L"\\??\\";
					pszVolume = pcs->VolTypeString.DeviceName;
					break;
				case OUTPUT_WIN32_DRIVE:
					pszPrefix = L"\\\\?\\";
					pszVolume = pcs->VolTypeString.Drive;
					break;
				case OUTPUT_WIN32_GUID:
					pszPrefix = L"\\\\?\\";
					pszVolume = pcs->VolTypeString.Guid;
					break;
				case OUTPUT_WIN32_DEVICE:
					pszPrefix = L"\\\\?\\";
					pszVolume = pcs->VolTypeString.DeviceName;
					break;
				case OUTPUT_ROOT:
					pszPrefix = NULL;
					pszVolume = NULL;
					break;
				case OUTPUT_DOS_DRIVE:
				default:
					pszPrefix = NULL;
					pszVolume = pcs->VolTypeString.Drive;
					break;
			}

			WCHAR buf[WIN32_MAX_PATH];

			if( pszPrefix != NULL && pszVolume != NULL )
			{
				RtlStringCchPrintfW(buf,ARRAYSIZE(buf),L"%s%s",pszPrefix,pszVolume);
			}
			else if( pszPrefix == NULL && pszVolume != NULL )
			{
				RtlStringCchCopyW(buf,ARRAYSIZE(buf),pszVolume);
			}
			else if( pszPrefix != NULL && pszVolume == NULL )
			{
				Success = FALSE; // Not possible. failed.
				__leave;
			}
			else
			{
				buf[0] = UNICODE_NULL;
			}

			if( buf[0] != UNICODE_NULL )
				pcs->OutputVolumeString = DuplicateString(buf);
		}

		//
		// Print Path
		//
		UNICODE_STRING usPrintRelativePath;

		if( pcs->ShortPathName )
		{
			_MakeShortPathName(pcs,&usVolumeRelativePath,&usPrintRelativePath);

			if( pcs->ReplaceSlash )
				_ReplaceBackslashToSlash(usPrintRelativePath);
		}
		else
		{
			if( pcs->ReplaceSlash )
			{
				DuplicateUnicodeString(&usPrintRelativePath,&usVolumeRelativePath);

				_ReplaceBackslashToSlash(usPrintRelativePath);
			}
			else
			{
				usPrintRelativePath = usVolumeRelativePath;
			}
		}

		if( pcs->OutputVolumeString )
			wprintf(L"%s%wZ\n",pcs->OutputVolumeString,&usPrintRelativePath);
		else
			wprintf(L"%wZ\n",&usPrintRelativePath);

		if( pcs->ReplaceSlash || pcs->ShortPathName )
			FreeUnicodeString( &usPrintRelativePath );

		Success = TRUE;
	}
	__finally
	{
		if( pszVolumeName )
			FreeMemory(pszVolumeName);
		if( pszObjectNamespaceVolumeName )
			FreeMemory(pszObjectNamespaceVolumeName);
	}

	return Success;
}

//----------------------------------------------------------------------------
//
//  _DumpDosDevicePath()
//
//----------------------------------------------------------------------------
void _DumpDosDevicePath(PCWSTR pszNtVolume,BOOL Local,UNICODE_STRING *pusVolumeRelativePath)
{
	HANDLE hspa = NULL;
	PCWSTR psz;

	if( EnumDosDeviceTargetNames(&hspa, pszNtVolume, Local ? 0x1 : 0x0) == STATUS_SUCCESS )
	{
		int i,c = GetDosDeviceTargetNamesCount(hspa);
		for(i = 0; i < c; i++)
		{
			psz = GetDosDeviceTargetNamesItem(hspa,i,NULL);
			wprintf(L"  > %s%wZ\n",psz,pusVolumeRelativePath);
		}
		FreeDosDeviceTargetNames(hspa);
	}
}

//----------------------------------------------------------------------------
//
//  _DumpPath()
//
//----------------------------------------------------------------------------
BOOLEAN _DumpPath(CCommandRunTimeStruct *pcs,UNICODE_STRING *pusPath)
{
	UNICODE_STRING usPath = *pusPath;
	UNICODE_STRING usVolumeRelativePath;
	UNICODE_STRING usVolume;
	PWSTR pszVolumeName;
	PWSTR pszObjectNamespaceVolumeName = NULL;
	BOOLEAN Success = FALSE;

	SplitVolumeRelativePath_U(&usPath,&usVolume,&usVolumeRelativePath);

	pszVolumeName = AllocateSzFromUnicodeString(&usVolume);

	//
	// Get source device type
	//
	ULONG PathType = GetPathType(pszVolumeName);

	__try
	{
		//
		// Create source NT device volume name
		//
		if( PathType != PATHTYPE_NT_DEVICE )
		{
			pszObjectNamespaceVolumeName = _QueryDosDeviceName(&pszVolumeName[4]);
		}
		else
		{
			WCHAR *psz = wcsrchr(pszObjectNamespaceVolumeName,L'\\');
			if( psz )
			{
				pszObjectNamespaceVolumeName = _QueryDosDeviceName( psz + 1 ); // Normalized Device Path
				FreeMemory(psz);
			}
			else
			{
				pszObjectNamespaceVolumeName = DuplicateString(pszVolumeName);
			}
		}

		if( pszObjectNamespaceVolumeName == NULL )
		{
			Success = FALSE;
			__leave;
		}

		//
		// NT device name
		//
		WCHAR *pszDeviceName = wcsrchr(pszObjectNamespaceVolumeName,L'\\');
		if( pszDeviceName )
		{
			pszDeviceName++;
		}

		if( pcs->OutputVolumeString == NULL )
		{
			pcs->VolTypeString.NtDevicePath = DuplicateString(pszObjectNamespaceVolumeName);
			pcs->OutputVolumeString = DuplicateString(pcs->VolTypeString.NtDevicePath);
		}

		wprintf(L"%wZ\n",&usVolumeRelativePath);
		wprintf(L"  > %s%wZ\n",pcs->OutputVolumeString,&usVolumeRelativePath);
		_DumpDosDevicePath(pcs->OutputVolumeString,0,&usVolumeRelativePath);
		_DumpDosDevicePath(pcs->OutputVolumeString,1,&usVolumeRelativePath);

		Success = TRUE;
	}
	__finally
	{
		FreeMemory(pszVolumeName);
		FreeMemory(pszObjectNamespaceVolumeName);
	}

	return Success;
}

//----------------------------------------------------------------------------
//
//  ActionFilePath()
//
//----------------------------------------------------------------------------
BOOLEAN ActionFilePath(CCommandRunTimeStruct *pcs, UNICODE_STRING *FileNameWithFullPath)
{
	switch( pcs->action )
	{
		case Display:
			return _PrintPathString(pcs,FileNameWithFullPath);
		case Dump:
			return _DumpPath(pcs,FileNameWithFullPath);
	}
	return FALSE;
}

//----------------------------------------------------------------------------
//
//  EnumFileCallback()
//
//----------------------------------------------------------------------------
BOOLEAN CALLBACK EnumFileCallback(HANDLE hDirectory,PCWSTR DirectoryName,
								  PVOID pInfoBuffer,ULONG_PTR Context)
{
	BOOLEAN Continue = FALSE;

	FILE_ID_BOTH_DIR_INFORMATION *pFileInfo = (FILE_ID_BOTH_DIR_INFORMATION *)pInfoBuffer;

	if( IS_RELATIVE_DIR_NAME_WITH_UNICODE_SIZE(pFileInfo->FileName,pFileInfo->FileNameLength) )
		return TRUE;

	CCommandRunTimeStruct *pcs = (CCommandRunTimeStruct *)Context;

	UNICODE_STRING usFileName;
	usFileName.Buffer = pFileInfo->FileName;
	usFileName.Length = usFileName.MaximumLength = (USHORT)pFileInfo->FileNameLength;

	PWSTR pszFullPath;
	pszFullPath = CombinePath_U(DirectoryName,&usFileName);

	if( pszFullPath )
	{
		UNICODE_STRING usFullyQualifiedFilePath;
		RtlInitUnicodeString(&usFullyQualifiedFilePath,pszFullPath);

		Continue = ActionFilePath(pcs,&usFullyQualifiedFilePath);

		FreeMemory(pszFullPath);
	}

	return Continue;
}

//----------------------------------------------------------------------------
//
//  EnumDirectoryFiles()
//
//----------------------------------------------------------------------------
VOID EnumDirectoryFiles(CCommandRunTimeStruct *pcs, UNICODE_STRING *Path, UNICODE_STRING *FileName)
{
	PWSTR pszPath = AllocateSzFromUnicodeString(Path);
	PWSTR pszName = AllocateSzFromUnicodeString(FileName);

	NTSTATUS Status;
	Status = EnumFiles(NULL,pszPath,pszName,&EnumFileCallback,(ULONG_PTR)pcs);

	if( NT_ERROR(Status) )
	{
		PrintError(Status, pszName);
	}

	FreeMemory(pszPath);
	FreeMemory(pszName);
}

//----------------------------------------------------------------------------
//
//  RecursiveEnumDirectoryFilesCallback()
//
//----------------------------------------------------------------------------
NTSTATUS CALLBACK RecursiveEnumDirectoryFilesCallback(
	ULONG CallbackReason,
	PCWSTR Path,
	PCWSTR RelativePath,
	UNICODE_STRING *FileName,
	NTSTATUS Status,
	ULONG FileInfoType,  // Reserved always zero
	PVOID FileInfo,      // FILE_ID_BOTH_DIR_INFORMATION
	ULONG_PTR CallbackContext
	)
{
	CCommandRunTimeStruct *pcs = (CCommandRunTimeStruct *)CallbackContext;

	switch( CallbackReason )
	{
		case FFCBR_FINDFILE: // process file
		{
			UNICODE_STRING usDirPath;
			UNICODE_STRING usFullFilePath;

			RtlInitUnicodeString(&usDirPath,Path);
			CombineUnicodeStringPath(&usFullFilePath,&usDirPath,FileName);

			ActionFilePath(pcs,&usFullFilePath);

			FreeUnicodeString(&usFullFilePath);

			break;
		}
		case FFCBR_DIRECTORYSTART: // open directory
		{
			break;
		}
		case FFCBR_DIRECTORYEND:   // close directory
		{
			break;
		}
	}

	return 0;
}

//----------------------------------------------------------------------------
//
//  RecursiveEnumDirectoryFiles()
//
//----------------------------------------------------------------------------
VOID RecursiveEnumDirectoryFiles(CCommandRunTimeStruct *pcs, UNICODE_STRING *Path, UNICODE_STRING *FileName)
{
	TraverseDirectory(*Path,*FileName,TRUE,0,&RecursiveEnumDirectoryFilesCallback,(ULONG_PTR)pcs);
}

//----------------------------------------------------------------------------
//
//  DeterminePathType()
//
//----------------------------------------------------------------------------
BOOLEAN DeterminePathType(CCommandRunTimeStruct *,FILEITEM& fi,COMMAND_RUN_PATH *prunpath)
{
	BOOLEAN PathExists = FALSE;
	BOOLEAN Success = FALSE;

	__try
	{
		if( IsNtDevicePath(fi.pszFilename) )
		{
			//
			// NT device name space path
			// In this case absolute path only.
			//
			UNICODE_STRING Path;
			RtlInitUnicodeString(&Path,fi.pszFilename);
			NTSTATUS Status = FindRootDirectory_U(&Path,NULL);
			if( STATUS_OBJECT_PATH_NOT_FOUND == Status )
			{
				DuplicateUnicodeString(&prunpath->FullPath,&Path);

				prunpath->FileName.Length = 0;
				prunpath->FileName.MaximumLength = 0;
				prunpath->FileName.Buffer = NULL;
			}
			else
			{
				if( AllocateUnicodeString(&prunpath->FullPath,fi.pszFilename) == STATUS_SUCCESS )
				{
					if( NT_ERROR(GetFileNamePart_U(&prunpath->FullPath,&prunpath->FileName)) )
					{
						Success = FALSE;
						__leave;
					}
					DuplicateUnicodeString(&prunpath->FileName,&prunpath->FileName);
				}
				else
				{
					Success = FALSE;
					__leave;
				}
			}
		}
		else
		{
			//
			// DOS drive path
			//
			PCWSTR FileNamePart = NULL;

			if( IsRelativePath(fi.pszFilename) )
			{
				//
				// If path string is a relative format, to determine as a DOS path.
				//
				RtlDosPathNameToRelativeNtPathName_U(fi.pszFilename,&prunpath->FullPath,(PWSTR*)&FileNamePart,&prunpath->RelativeDirPart);
			}
			else
			{
				RtlDosPathNameToNtPathName_U(fi.pszFilename,&prunpath->FullPath,(PWSTR*)&FileNamePart,NULL);
			}

			if( FileNamePart != NULL )
				RtlCreateUnicodeString(&prunpath->FileName,FileNamePart);
		}

		if( prunpath->FullPath.Buffer == NULL )
		{
			Success = FALSE;
			__leave; // fatal error, abort
		}

		prunpath->EnumDirectoryFiles = FALSE;

		if( IsRootDirectory_U(&prunpath->FullPath) )
		{
			// todo: root directory is spacial pattern.
		}
		else if( IsLastCharacterBackslash_U(&prunpath->FullPath) )
		{
			RtlCreateUnicodeString(&prunpath->FileName,L"*"); 
			prunpath->EnumDirectoryFiles = TRUE;
		}
		else
		{
			//
			// If filename part includes wildcard character, split the path 
			// the directory-part and filename-part.
			//
			if( HasWildCardChar_U(&prunpath->FileName) )
			{
				// Remove filename spec
				SplitPathFileName_U(&prunpath->FullPath,NULL);
				prunpath->EnumDirectoryFiles = TRUE;
			}
		}

		Success = TRUE;
	}
	__finally
	{
#ifdef _DEBUG
		PWSTR p1,p2;
		p1 = AllocateSzFromUnicodeString(&prunpath->FullPath);
		p2 = AllocateSzFromUnicodeString(&prunpath->FileName);
		FreeMemory(p1);
		FreeMemory(p2);
#endif
	}

	return Success;
}

//----------------------------------------------------------------------------
//
//  ClearParameters()
//
//----------------------------------------------------------------------------
VOID ClearParameters(CCommandRunTimeStruct *pcs)
{
	_SafeFreeMemory( pcs->OutputVolumeString );
	_SafeFreeMemory( pcs->VolTypeString.NtDevicePath );
	_SafeFreeMemory( pcs->VolTypeString.DeviceName );
	_SafeFreeMemory( pcs->VolTypeString.Guid );
	_SafeFreeMemory( pcs->VolTypeString.Drive );
}

//----------------------------------------------------------------------------
//
//  ProcessArgumentFiles()
//
//----------------------------------------------------------------------------
HRESULT ProcessArgumentFiles(CCommandRunTimeStruct& cmd)
{
	HRESULT hr = E_FAIL;

	CCommandRunPath Path;
	CFileItem **pp;

	//
	// Start processing
	//
	pp = cmd.FileList.FirstFile();

	while( pp != NULL )
	{
		DeterminePathType(&cmd,**pp,&Path);

		if( IsDirectory_U(&Path.FullPath) && Path.EnumDirectoryFiles == FALSE && cmd.Recursive )
		{
			// If the directory path last character in does not end with '\'
			// and if cmd.Recursive == true, force setting recursive mode.
			RtlFreeUnicodeString( &Path.FileName );
			RtlCreateUnicodeString(&Path.FileName,L"*"); 
			Path.EnumDirectoryFiles = TRUE;
		}

		BOOLEAN PathExists = PathFileExists_U(&Path.FullPath,NULL);

		if( PathExists )
		{
			if( IsDirectory_U(&Path.FullPath) )
			{
				if( cmd.Recursive && Path.EnumDirectoryFiles )
					RecursiveEnumDirectoryFiles(&cmd,&Path.FullPath,&Path.FileName);
				else if( Path.EnumDirectoryFiles )
					EnumDirectoryFiles(&cmd,&Path.FullPath,&Path.FileName);
				else
					ActionFilePath(&cmd,&Path.FullPath);
			}
			else
			{
				ActionFilePath(&cmd,&Path.FullPath);
			}
		}
		else
		{
			PrintError(STATUS_NO_SUCH_FILE, Path.FileName.Buffer);
		}

		ClearParameters(&cmd);

		pp = cmd.FileList.NextFile(pp);
	}

	return hr;
}

//----------------------------------------------------------------------------
//
//  Cleanup()
//
//----------------------------------------------------------------------------
void Cleanup(CCommandRunTimeStruct *pcs)
{
	int i,c = pcs->FileList.GetCount();
	for(i = 0; i < c; i++)
	{
		CFileItem *pi = pcs->FileList[i];
		delete pi;
	}
}

//----------------------------------------------------------------------------
//
//  PrintError()
//
//----------------------------------------------------------------------------
int PrintError( LONG ErrorCode, PCWSTR pszParam )
{
	switch( ErrorCode )
	{
		case 0:
			break;
		case STATUS_CANCELLED:
			printf("Operation aborted.\n");
			break;
		case STATUS_NO_SUCH_FILE:
			// The file %hs does not exist.
			if( pszParam )
				printf("The file '%ls' does not exist.\n",pszParam);
			else
				printf("The file does not exist.\n");
			break;
		case STATUS_NOT_A_DIRECTORY:
			if( pszParam )
				printf("The file '%ls' is not directory.\n",pszParam);
			else
				printf("The specifies path is not directory.\n");
			break;
		case STATUS_INVALID_PARAMETER:
			// An invalid combination of parameters was specified.
			printf("The parameter is incorrect.\n");
			break;
		default:
		{
			ULONG dosError;
			if( (ErrorCode & 0xC0000000) == 0xC0000000 )
				dosError = RtlNtStatusToDosError(ErrorCode);
			else
				dosError = ErrorCode;

			PWSTR pMessage = NULL;
			WinGetSystemErrorMessage(dosError,&pMessage);
			if( pMessage )
			{
				PWSTR p = wcspbrk(pMessage,L"\r\n");
				if( p )
					*p = L'\0';
			}

			printf(
				((ErrorCode & 0xC0000000) == 0xC0000000) ? "%S (0x%08X)\n" : "%S (%d)\n",
				pMessage != NULL ? pMessage : L"Unknown error" ,
				ErrorCode);

			WinFreeErrorMessage(pMessage);
			break;
		}
	}

	return (int)RtlNtStatusToDosError(ErrorCode);
}

//----------------------------------------------------------------------------
//
//  Help()
//
//----------------------------------------------------------------------------
void Help()
{
	PrintHelp();
}

//----------------------------------------------------------------------------
//
//  ParseArguments()
//
//----------------------------------------------------------------------------
BOOLEAN ParseArguments(int argc, WCHAR *argv[],CCommandRunTimeStruct *pcs)
{
	int i;
	size_t len;

	// "-?","/?"
	if( argc == 2 && (argv[1][0] == L'/' || argv[1][0] == L'-') && (argv[1][1] == L'?') && (argv[1][2] == L'\0') )
	{
		pcs->ShowHelp = TRUE;
		return TRUE;
	}

	for(i = 1; i < argc; i++)
	{
#ifdef _DEBUG
		printf("=>%S\n",argv[i]);
#endif
		if( argv[i][0] == L'-' || argv[i][0] == L'/' )
		{
			wchar_t *args = &argv[i][1];

			len = wcslen(args);

			ULONG OutputType = OUTPUT_UNKNOWN;

			if( _wcsicmp(args,L"drive") == 0 || _wcsicmp(args,L"dosdrive") == 0 || _wcsicmp(args,L"dos") == 0 )
				OutputType = OUTPUT_DOS_DRIVE;
			else if( _wcsicmp(args,L"nt") == 0 || _wcsicmp(args,L"ntdevice") == 0 )
				OutputType = OUTPUT_NT_DEVICE;
			else if( _wcsicmp(args,L"ntdrive") == 0 )
				OutputType = OUTPUT_NT_DOSDRIVE;
			else if( _wcsicmp(args,L"ntguid") == 0 )
				OutputType = OUTPUT_NT_DOSGUID;
			else if( _wcsicmp(args,L"ntdosdevice") == 0 )
				OutputType = OUTPUT_NT_DOSDEVICE;
			else if( _wcsicmp(args,L"win32drive") == 0 )
				OutputType = OUTPUT_WIN32_DRIVE;
			else if( _wcsicmp(args,L"win32guid") == 0 || _wcsicmp(args,L"guid") == 0 )
				OutputType = OUTPUT_WIN32_GUID;
			else if( _wcsicmp(args,L"root") == 0 )
				OutputType = OUTPUT_ROOT;
			else if( _wcsicmp(args,L"dump") == 0 )
				OutputType = OUTPUT_DUMP;
			else
			{
				switch( args[0] )
				{
					case L's':
					case L'S':
						if( args[1] == L'\0' )
							pcs->ShortPathName = TRUE;
						else
							return FALSE; // invalid switch
						break;
					case L'r':
					case L'R':
						if( args[1] == L'\0' )
							pcs->Recursive = TRUE;
						else
							return FALSE; // invalid switch
						break;
					case L'x':
					case L'X':
						if( args[1] == L'\0' )
							pcs->ReplaceSlash = TRUE;
						else
							return FALSE; // invalid switch
						break;
					default:
						return FALSE; // invalid switch
				}
			}

			if( OutputType != OUTPUT_UNKNOWN )
			{
				if( pcs->OutputType != OUTPUT_UNKNOWN )
					return FALSE; // double specified.
				pcs->OutputType = OutputType;
			}
		}
		else
		{
			pcs->AppendFileName( argv[i] );
		}
	}

	if( pcs->OutputType == OUTPUT_DUMP )
		pcs->action = Dump;
	else
		pcs->action = Display;

	return TRUE;
}

//
// SourcePath's Specification:
//
// \??\c:\windows\file   ... Displays information a file "\??\c:\windows\file".
//
// \??\c:\windows        ... The directory "\??\c:\windows" itself is processed.  Not directory contents.
// \??\c:\windows\       ... Same as "\??\c:\windows\*". 
// \??\c:\windows\*      ... Enumerate files "*" under the "\??\c:\windows". 
//                           If specified -s option, include sub-directory.
// \??\c:\windows\*.exe  ... Enumerate files "*.exe" under the "\??\c:\windows". 
//                           If specified -s option, include sub-directory.
//
// If the end of the path is '\', it is determined that the contents of the directory
// are specified for processing, and FileName = L "*" is set.
// For example, if you specified argument "\??\C:Temp\" that to:
//      DirPath  = "\??\C:\Temp\"
//      FileName = "*"
//
// Valid path pattern:
//
// \Device\HarddiskVoume1
// \Device\HarddiskVoume1\
// \Device\HarddiskVoume1\*
//
// \??\C:
// \??\C:\
// \??\C:\*
//
// C:    ... Current directory of C: drive
// C:\   ... Root directory of C: drive
//

//----------------------------------------------------------------------------
//
//  wmain()
//
//----------------------------------------------------------------------------
int __cdecl wmain(int argc, WCHAR* argv[])
{
	setlocale(LC_ALL,"");

#if _USE_CRT_HEAP_MEMORY
	_MemInit();
#endif

	CCommandRunTimeStruct cmd;

	if( !ParseArguments(argc,argv,&cmd) )
	{
		printf("Invalid argument.\n");
		return 0;
	}

	if( cmd.ShowHelp )
	{
		Help();
		return 0;
	}

	if( cmd.FileList.GetCount() == 0 )
	{
		_AddCurrentDirectory(&cmd);
	}

	ProcessArgumentFiles(cmd);

	Cleanup(&cmd);

#if _USE_CRT_HEAP_MEMORY
	_MemEnd();
#endif

	return 0;
}
