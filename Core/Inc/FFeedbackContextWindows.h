/*=============================================================================
	FFeedbackContextWindows.h: Unreal Windows user interface interaction.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

/*-----------------------------------------------------------------------------
	FFeedbackContextWindows.
-----------------------------------------------------------------------------*/

//
// Feedback context.
//
class FFeedbackContextWindows : public FFeedbackContext
{
public:
	// Variables.
	INT SlowTaskCount;
#if 1 //U2Ed
	DWORD hWndProgressBar, hWndProgressText, hWndProgressDlg;
#else
	DWORD hWndProgressBar, hWndProgressText;
#endif

	// Constructor.
	FFeedbackContextWindows()
	: SlowTaskCount( 0 )
	, hWndProgressBar( 0 )
	, hWndProgressText( 0 )
#if 1 //U2Ed
	, hWndProgressDlg( 0 )
#endif
	{}
	void Serialize( const TCHAR* V, EName Event )
	{
		guard(FFeedbackContextWindows::Serialize);
		if( Event==NAME_UserPrompt && (GIsClient || GIsEditor) )
			::MessageBox( NULL, V, LocalizeError("Warning",TEXT("Core")), MB_OK|MB_TASKMODAL );
		else
			debugf( NAME_Warning, TEXT("%s"), V );
		unguard;
	}
	UBOOL YesNof( const TCHAR* Fmt, ... )
	{
		TCHAR TempStr[4096];
		GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), Fmt );

		guard(FFeedbackContextWindows::YesNof);
		if( GIsClient || GIsEditor )
			return( ::MessageBox( NULL, TempStr, LocalizeError("Question",TEXT("Core")), MB_YESNO|MB_TASKMODAL ) == IDYES);
		else
			return 0;
		unguard;
	}
	void BeginSlowTask( const TCHAR* Task, UBOOL StatusWindow, UBOOL Cancelable )
	{
		guard(FFeedbackContextWindows::BeginSlowTask);
#if 1 //U2Ed
		::ShowWindow( (HWND)hWndProgressDlg, SW_SHOW );
		if( hWndProgressBar && hWndProgressText )
		{
			SendMessageLX( (HWND)hWndProgressText, WM_SETTEXT, (WPARAM)0, Task );
			SendMessageX( (HWND)hWndProgressBar, PBM_SETRANGE, (WPARAM)0, MAKELPARAM(0, 100) );

			UpdateWindow( (HWND)hWndProgressDlg );
			UpdateWindow( (HWND)hWndProgressText );
			UpdateWindow( (HWND)hWndProgressText );

			{	// flush all messages
				MSG mfm_msg;
				while(::PeekMessage(&mfm_msg, (HWND)hWndProgressDlg, 0, 0, PM_REMOVE))
				{
					TranslateMessage(&mfm_msg);
					DispatchMessage(&mfm_msg);
				}
			}
		}
#else
		if( hWndProgressBar && hWndProgressText )
		{
			SendMessageX( (HWND)hWndProgressBar, PBM_SETRANGE, (WPARAM)0, MAKELPARAM(0, 100) );
			SendMessageLX( (HWND)hWndProgressText, WM_SETTEXT, (WPARAM)0, Task );
			UpdateWindow( (HWND)hWndProgressText );
		}
#endif
		GIsSlowTask = ++SlowTaskCount>0;
		unguard;
	}
	void EndSlowTask()
	{
		guard(FFeedbackContextWindows::EndSlowTask);
		check(SlowTaskCount>0);
		GIsSlowTask = --SlowTaskCount>0;
#if 1 //U2Ed
		if( !GIsSlowTask )
			::ShowWindow( (HWND)hWndProgressDlg, SW_HIDE );
#endif
		unguard;
	}
	UBOOL VARARGS StatusUpdatef( INT Numerator, INT Denominator, const TCHAR* Fmt, ... )
	{
		guard(FFeedbackContextWindows::StatusUpdatef);
		TCHAR TempStr[4096];
		GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), Fmt );
		if( GIsSlowTask && hWndProgressBar && hWndProgressText )
		{
			SendMessageLX( (HWND)hWndProgressText, WM_SETTEXT, (WPARAM)0, TempStr );
			SendMessageX( (HWND)hWndProgressBar, PBM_SETPOS, (WPARAM)(Denominator ? 100*Numerator/Denominator : 0), (LPARAM)0 );
#if 1 //U2Ed
			UpdateWindow( (HWND)hWndProgressDlg );
			UpdateWindow( (HWND)hWndProgressText );
			UpdateWindow( (HWND)hWndProgressBar );

			{	// flush all messages
				MSG mfm_msg;
				while(::PeekMessage(&mfm_msg, (HWND)hWndProgressDlg, 0, 0, PM_REMOVE)) {
					TranslateMessage(&mfm_msg);
					DispatchMessage(&mfm_msg);
				}
			}
#endif
		}
		return 1;
		unguard;
	}
	void SetContext( FContextSupplier* InSupplier )
	{}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
