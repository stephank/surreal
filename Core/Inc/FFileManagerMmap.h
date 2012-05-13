/*=============================================================================
	FFileManagerMmap.h: Unreal mmap based file manager.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Brandon Reinhart
=============================================================================*/

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <glob.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "FFileManagerGeneric.h"

/*-----------------------------------------------------------------------------
	File Manager.
-----------------------------------------------------------------------------*/

#define MAX_PRECACHE 8192

// File manager.
class FArchiveFileReader : public FArchive
{
public:
	FArchiveFileReader( INT InFile, BYTE* InMap, FOutputDevice* InError, INT InSize )
	:	File			( InFile )
	,	Map				( InMap )
	,	Error			( InError )
	,	Size			( InSize )
	,	Pos				( 0 )
	{
		guard(FArchiveFileReader::FArchiveFileReader);
		ArIsLoading = ArIsPersistent = 1;
		unguard;
	}
	~FArchiveFileReader()
	{
		guard(FArchiveFileReader::~FArchiveFileReader);
		if( File != -1 )
			Close();
		unguard;
	}
	void Precache( INT HintCount )
	{
		guardSlow(FArchiveFileReader::Precache);
		BYTE* StartAddr = Map+Pos;
		INT Length = Min( Min( HintCount, Size-Pos ), MAX_PRECACHE );
		if( mlock( StartAddr, Length ) == -1 )
		{
			ArIsError = 1;
			Error->Logf( TEXT("Failed to precache (%i bytes): %s"), Length, appFromAnsi(strerror(errno)) );
			return;
		}
		munlock( StartAddr, Length );
		unguardSlow;
	}
	void Seek( INT InPos )
	{
		guard(FArchiveFileReader::Seek);
		check(InPos>=0);
		check(InPos<=Size);
		Pos = InPos;
		unguard;
	}
	INT Tell()
	{
		return Pos;
	}
	INT TotalSize()
	{
		return Size;
	}
	UBOOL Close()
	{
		guardSlow(FArchiveFileReader::Close);
		if( File != -1 )
		{
			munmap( Map, Size );
			close( File );
		}
		File = -1;
		return !ArIsError;
		unguardSlow;
	}
	void Serialize( void* V, INT Length )
	{
		guardSlow(FArchiveFileReader::Serialize);
		if( Length > Size-Pos )
		{
			ArIsError = 1;
			Error->Logf( TEXT("ReadFile beyond EOF %i+%i/%i"), Pos, Length, Size );
			return;
		}
		appMemcpy( V, Map+Pos, Length );
		Pos += Length;
		unguardSlow;
	}
protected:
	INT				File;
	FOutputDevice*	Error;
	INT				Size;
	INT				Pos;
	BYTE*			Map;
};
class FArchiveFileWriter : public FArchive
{
public:
	FArchiveFileWriter( FILE* InFile, FOutputDevice* InError )
	:	File		(InFile)
	,	Error		( InError )
	,	Pos			(0)
	,	BufferCount	(0)
	{
		ArIsSaving = ArIsPersistent = 1;
	}
	~FArchiveFileWriter()
	{
		guard(FArchiveFileWriter::~FArchiveFileWriter);
		if( File )
			Close();
		File = NULL;
		unguard;
	}
	void Seek( INT InPos )
	{
		Flush();
		if( fseek(File,InPos,SEEK_SET) )
		{
			ArIsError = 1;
			Error->Logf( LocalizeError("SeekFailed",TEXT("Core")) );
		}
		Pos = InPos;
	}
	INT Tell()
	{
		return Pos;
	}
	UBOOL Close()
	{
		guardSlow(FArchiveFileWriter::Close);
		Flush();
		if( File && fclose( File ) )
		{
			ArIsError = 1;
			Error->Logf( LocalizeError("WriteFailed",TEXT("Core")) );
		}
		File = NULL;
		return !ArIsError;
		unguardSlow;
	}
	void Serialize( void* V, INT Length )
	{
		Pos += Length;
		INT Copy;
		while( Length > (Copy=ARRAY_COUNT(Buffer)-BufferCount) )
		{
			appMemcpy( Buffer+BufferCount, V, Copy );
			BufferCount += Copy;
			Length      -= Copy;
			V            = (BYTE*)V + Copy;
			Flush();
		}
		if( Length )
		{
			appMemcpy( Buffer+BufferCount, V, Length );
			BufferCount += Length;
		}
	}
	void Flush()
	{
		if( BufferCount && fwrite( Buffer, BufferCount, 1, File )!=1 )
		{
			ArIsError = 1;
			Error->Logf( LocalizeError("WriteFailed",TEXT("Core")) );
		}
		BufferCount=0;
	}
protected:
	FILE*			File;
	FOutputDevice*	Error;
	INT				Pos;
	INT				BufferCount;
	BYTE			Buffer[4096];
};

class FFileManagerMmap : public FFileManagerGeneric
{
private:
	TCHAR	ConfigDir[PATH_MAX];

public:
	void Init( UBOOL Startup )
	{
		guard(FFileManagerMmap::Init);

		const ANSICHAR* XdgConfigHome = getenv( "XDG_CONFIG_HOME" );
		if( XdgConfigHome )
			appSprintf( ConfigDir, TEXT("%s/%s/System/"), appFromAnsi(XdgConfigHome), appPackage() );
		else
			appSprintf( ConfigDir, TEXT("%s/.config/%s/System/"), appFromAnsi(getenv("HOME")), appPackage() );

		if( !MakeDirectory( ConfigDir, 1 ) )
			appErrorf( TEXT("Failed to create configuration directory %s"), ConfigDir );

		unguard;
	}

	FArchive* CreateFileReader( const TCHAR* OrigFilename, DWORD Flags, FOutputDevice* Error )
	{
		guard(FFileManagerMmap::CreateFileReader);

		TCHAR FixedFilename[PATH_MAX], Filename[PATH_MAX];
		PathSeparatorFixup( FixedFilename, OrigFilename );

		INT File = -1;
		if( RewriteToConfigPath( Filename, FixedFilename ) )
			File = open(TCHAR_TO_ANSI(Filename), O_RDONLY);
		if( File == -1 )
		{
			File = open(TCHAR_TO_ANSI(FixedFilename), O_RDONLY);
			if( File == -1 )
			{
				if( Flags & FILEREAD_NoFail )
					appErrorf(TEXT("Failed to read file: %s"),Filename);
				return NULL;
			}
		}

		INT Size = lseek( File, 0, SEEK_END );
		lseek( File, 0, SEEK_SET );

		BYTE* Map = (BYTE*)mmap( NULL, Size, PROT_READ, MAP_PRIVATE, File, 0 );
		if( Map == MAP_FAILED )
		{
			close( File );
			if( Flags & FILEREAD_NoFail )
				appErrorf(TEXT("Failed to read file: %s"),Filename);
			return NULL;
		}

		return new(TEXT("MmapFileReader"))FArchiveFileReader(File,Map,Error,Size);

		unguard;
	}
	FArchive* CreateFileWriter( const TCHAR* OrigFilename, DWORD Flags, FOutputDevice* Error )
	{
		guard(FFileManagerMmap::CreateFileWriter);

		TCHAR FixedFilename[PATH_MAX], Filename[PATH_MAX];
		PathSeparatorFixup( FixedFilename, OrigFilename );

		if( Flags & FILEWRITE_EvenIfReadOnly )
			chmod(TCHAR_TO_ANSI(Filename), S_IRUSR | S_IWUSR);
		if( (Flags & FILEWRITE_NoReplaceExisting) && FileSize(Filename)>=0 )
			return NULL;

		const ANSICHAR* Mode = (Flags & FILEWRITE_Append) ? "ab" : "wb"; 
		FILE* File = NULL;
		if( RewriteToConfigPath( Filename, FixedFilename ) )
		{
			// If appending, copy the file from the application directory
			// as a template for the user's ConfigDir.
			if( (Flags & FILEWRITE_Append) && FileSize(Filename) == -1 )
			{
				if( !Copy( Filename, FixedFilename, 0, 0, 0, NULL ) )
				{
					if( Flags & FILEWRITE_NoFail )
						appErrorf( TEXT("Failed to write: %s"), Filename );
					return NULL;
				}
			}
			File = fopen(TCHAR_TO_ANSI(Filename),Mode);
		}
		else
		{
			File = fopen(TCHAR_TO_ANSI(FixedFilename),Mode);
		}
		if( !File )
		{
			if( Flags & FILEWRITE_NoFail )
				appErrorf( TEXT("Failed to write: %s"), Filename );
			return NULL;
		}
		if( Flags & FILEWRITE_Unbuffered )
			setvbuf( File, 0, _IONBF, 0 );

		return new(TEXT("MmapFileWriter"))FArchiveFileWriter(File,Error);

		unguard;
	}
	UBOOL Delete( const TCHAR* OrigFilename, UBOOL RequireExists=0, UBOOL EvenReadOnly=0 )
	{
		guard(FFileManagerMmap::Delete);

		TCHAR Filename[PATH_MAX];
		PathSeparatorFixup( Filename, OrigFilename );

		if( EvenReadOnly )
			chmod(TCHAR_TO_ANSI(Filename), S_IRUSR | S_IWUSR);
		return unlink(TCHAR_TO_ANSI(Filename))==0 || (errno==ENOENT && !RequireExists);

		unguard;
	}
	SQWORD GetGlobalTime( const TCHAR* Filename )
	{
		guard(FFileManagerMmap::GetGlobalTime);

		return 0;
		
		unguard;
	}
	UBOOL SetGlobalTime( const TCHAR* Filename )
	{
		guard(FFileManagerMmap::SetGlobalTime);

		return 0;

		unguard;
	}
	UBOOL MakeDirectory( const TCHAR* OrigPath, UBOOL Tree=0 )
	{
		guard(FFileManagerMmap::MakeDirectory);

		TCHAR Path[PATH_MAX];
		PathSeparatorFixup( Path, OrigPath );

		if( Tree )
			return FFileManagerGeneric::MakeDirectory( Path, Tree );

		return mkdir(TCHAR_TO_ANSI(Path), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)==0 || errno==EEXIST;

		unguard;
	}
	UBOOL DeleteDirectory( const TCHAR* OrigPath, UBOOL RequireExists=0, UBOOL Tree=0 )
	{
		guard(FFileManagerMmap::DeleteDirectory);

		TCHAR Path[PATH_MAX];
		PathSeparatorFixup( Path, OrigPath );

		if( Tree )
			return FFileManagerGeneric::DeleteDirectory( Path, RequireExists, Tree );

		return rmdir(TCHAR_TO_ANSI(Path))==0 || (errno==ENOENT && !RequireExists);

		unguard;
	}
	TArray<FString> FindFiles( const TCHAR* OrigPattern, UBOOL Files, UBOOL Directories )
	{
		guard(FFileManagerMmap::FindFiles);

		TArray<FString> Result;
		TCHAR FixedPattern[PATH_MAX], Pattern[PATH_MAX];
		glob_t GlobBuf;

		PathSeparatorFixup( FixedPattern, OrigPattern );

		// Look in both ConfigDir and the application directory.
		INT GlobFlags = GLOB_NOSORT | GLOB_MARK;
		if( RewriteToConfigPath( Pattern, FixedPattern ) )
		{
			glob( TCHAR_TO_ANSI(Pattern), GlobFlags, NULL, &GlobBuf );
			GlobFlags |= GLOB_APPEND;
		}
		glob( TCHAR_TO_ANSI(FixedPattern), GlobFlags, NULL, &GlobBuf );

		for( size_t i = 0; i < GlobBuf.gl_pathc; i++ )
		{
			const TCHAR* Match = appFromAnsi(GlobBuf.gl_pathv[i]);
			// Strip the filename from the match
			for( const TCHAR* Ptr = Match; *Ptr != TEXT('\0'); Ptr++ )
				if( *Ptr == TEXT('/') )
					Match = Ptr + 1;
			// Directories are GLOB_MARK'd with a trailing slash,
			// so this prevents directories from being added to the result.
			if( appStrlen(Match) != 0 )
				new(Result)FString( Match );
		}

		globfree( &GlobBuf );

		return Result;
		unguard;
	}
	UBOOL SetDefaultDirectory( const TCHAR* Filename )
	{
		guard(FFileManagerMmap::SetDefaultDirectory);
		return chdir(TCHAR_TO_ANSI(Filename))==0;
		unguard;
	}
	FString GetDefaultDirectory()
	{
		guard(FFileManagerMmap::GetDefaultDirectory);
		ANSICHAR Buffer[PATH_MAX]="";
		if (getcwd( Buffer, ARRAY_COUNT(Buffer) ) == NULL)
			Buffer[0] = '\0';
		return appFromAnsi( Buffer );
		unguard;
	}
private:
	void PathSeparatorFixup( TCHAR* Dest, const TCHAR* Src )
	{
		appStrcpy( Dest, Src );
		for( TCHAR *Cur = Dest; *Cur != TEXT('\0'); Cur++ )
			if( *Cur == TEXT('\\') )
				*Cur = TEXT('/');
	}
	UBOOL RewriteToConfigPath( TCHAR* Result, const TCHAR* Path )
	{
		// Don't rewrite absolute paths
		if( Path[0] == TEXT('/') )
			return 0;

		appStrcpy( Result, ConfigDir );
		appStrcat( Result, Path );

		return 1;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
