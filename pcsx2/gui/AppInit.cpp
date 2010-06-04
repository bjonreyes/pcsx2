/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"
#include "IniInterface.h"
#include "MainFrame.h"
#include "ConsoleLogger.h"
#include "MSWstuff.h"

#include "DebugTools/Debug.h"
#include "Dialogs/ModalPopups.h"

#include <wx/cmdline.h>
#include <wx/intl.h>
#include <wx/stdpaths.h>

static bool m_ForceWizard = false;

static void CpuCheckSSE2()
{
	if( x86caps.hasStreamingSIMD2Extensions ) return;

	// Only check once per process session:
	static bool checked = false;
	if( checked ) return;
	checked = true;

	wxDialogWithHelpers exconf( NULL, _("PCSX2 - SSE2 Recommended") );

	exconf += exconf.Heading( pxE( ".Popup:Startup:NoSSE2",
		L"Warning: Your computer does not support SSE2, which is required by many PCSX2 recompilers and plugins. "
		L"Your options will be limited and emulation will be *very* slow." )
	);

	pxIssueConfirmation( exconf, MsgButtons().OK(), L"Error:Startup:NoSSE2" );

	// Auto-disable anything that needs SSE2:

	g_Conf->EmuOptions.Cpu.Recompiler.EnableEE	= false;
	g_Conf->EmuOptions.Cpu.Recompiler.EnableVU0	= false;
	g_Conf->EmuOptions.Cpu.Recompiler.EnableVU1	= false;
}

void Pcsx2App::WipeUserModeSettings()
{
	wxDirName usrlocaldir( wxStandardPaths::Get().GetUserLocalDataDir() );
	if( !usrlocaldir.Exists() ) return;

	wxString cwd( Path::Normalize( wxGetCwd() ) );
	u32 hashres = HashTools::Hash( (char*)cwd.c_str(), cwd.Length() );

	wxFileName usermodefile( FilenameDefs::GetUsermodeConfig() );
	usermodefile.SetPath( usrlocaldir.ToString() );
	ScopedPtr<wxFileConfig> conf_usermode( OpenFileConfig( usermodefile.GetFullPath() ) );

	wxString groupname( wxsFormat( L"CWD.%08x", hashres ) );
	Console.WriteLn( "(UserModeSettings) Removing entry:" );
	Console.Indent().WriteLn( L"Path: %s\nHash:%s", cwd.c_str(), groupname.c_str() );
	conf_usermode->DeleteGroup( groupname );
}

// User mode settings can't be stored in the CWD for two reasons:
//   (a) the user may not have permission to do so (most obvious)
//   (b) it would result in sloppy usermode.ini found all over a hard drive if people runs the
//       exe from many locations (ugh).
//
// So better to use the registry on Win32 and a "default ini location" config file under Linux,
// and store the usermode settings for the CWD based on the CWD's hash.
//
void Pcsx2App::ReadUserModeSettings()
{
	wxDirName usrlocaldir( wxStandardPaths::Get().GetUserLocalDataDir() );
	if( !usrlocaldir.Exists() )
	{
		Console.WriteLn( L"Creating UserLocalData folder: " + usrlocaldir.ToString() );
		usrlocaldir.Mkdir();
	}

	wxString cwd( Path::Normalize( wxGetCwd() ) );
	u32 hashres = HashTools::Hash( (char*)cwd.c_str(), cwd.Length() );

	wxFileName usermodefile( FilenameDefs::GetUsermodeConfig() );
	usermodefile.SetPath( usrlocaldir.ToString() );
	ScopedPtr<wxFileConfig> conf_usermode( OpenFileConfig( usermodefile.GetFullPath() ) );

	wxString groupname( wxsFormat( L"CWD.%08x", hashres ) );

	if (IOP_ENABLE_SIF_HACK == 1)
	{
		wxDialogWithHelpers hackedVersion( NULL, _("It will devour your young! - PCSX2 Shub-Niggurath edition") );
		hackedVersion.SetMinWidth( 520 );
		hackedVersion += hackedVersion.Text(
			L"NOTICE!! This is a version of Pcsx2 with hacks enabled meant for developers only. "
			L"It will likely crash on all games, devour your young, and make you an object of shame and disgrace among your family and friends. "
			L"Do not report any bugs with this version if you received this popup. \n\nYou have been warned. "
		);
		
		hackedVersion += new wxButton( &hackedVersion, wxID_OK ) | pxSizerFlags::StdCenter();
		hackedVersion.ShowModal();
	}

	bool hasGroup = conf_usermode->HasGroup( groupname );
	bool forceWiz = m_ForceWizard || !hasGroup;
	
	if( !forceWiz )
	{
		conf_usermode->SetPath( groupname );
		forceWiz = !conf_usermode->HasEntry( L"DocumentsFolderMode" );
		conf_usermode->SetPath( L".." );
	}

	if( forceWiz )
	{
		// Beta Warning!

		if( !hasGroup )
		{
			wxDialogWithHelpers beta( NULL, _("PCSX2 0.9.7 Beta") );

			beta += new pxStaticText( &beta,
				L"This is a *Beta* build of PCSX2 0.9.7.  We are in the middle of major rewrites of the " 
				L"user interface, and some parts of the program have *NOT* been implemented yet.  Options will be missing.  "
				L"Some things may crash or hang without warning.", wxALIGN_CENTER
			);
			
			beta += new wxButton( &beta, wxID_OK ) | pxSizerFlags::StdCenter();
			beta.ShowModal();
		}
	
		// first time startup, so give the user the choice of user mode:
		FirstTimeWizard wiz( NULL );
		if( !wiz.RunWizard( wiz.GetUsermodePage() ) )
			throw Exception::StartupAborted( L"Startup aborted: User canceled FirstTime Wizard." );

		// Save user's new settings
		IniSaver saver( *conf_usermode );
		g_Conf->LoadSaveUserMode( saver, groupname );
		AppConfig_OnChangedSettingsFolder( true );
		AppSaveSettings();
	}
	else
	{
		// usermode.ini exists and is populated with valid data -- assume User Documents mode,
		// unless the ini explicitly specifies otherwise.

		DocsFolderMode = DocsFolder_User;

		IniLoader loader( *conf_usermode );
		g_Conf->LoadSaveUserMode( loader, groupname );

		if( !wxFile::Exists( GetSettingsFilename() ) )
		{
			// user wiped their pcsx2.ini -- needs a reconfiguration via wizard!
			// (we skip the first page since it's a usermode.ini thing)
			
			// Fixme : Skipping the first page is a bad idea, as it does a lot of file / directory checks on hitting Apply.
			// If anything is missing, the first page prompts to fix it.
			// If we skip this check, it's very likely that actions like creating Memory Cards will fail.
			FirstTimeWizard wiz( NULL );
			if( !wiz.RunWizard( /*wiz.GetPostUsermodePage()*/ wiz.GetUsermodePage() ) )
				throw Exception::StartupAborted( L"Startup aborted: User canceled Configuration Wizard." );

			// Save user's new settings
			IniSaver saver( *conf_usermode );
			g_Conf->LoadSaveUserMode( saver, groupname );
			AppConfig_OnChangedSettingsFolder( true );
			AppSaveSettings();
		}
	}
	
	// force unload plugins loaded by the wizard.  If we don't do this the recompilers might
	// fail to allocate the memory they need to function.
	UnloadPlugins();
}

void Pcsx2App::DetectCpuAndUserMode()
{
	x86caps.Identify();
	x86caps.CountCores();
	x86caps.SIMD_EstablishMXCSRmask();

	if( !x86caps.hasMultimediaExtensions )
	{
		// Note: due to memcpy_fast, we need minimum MMX even for interpreters.  This will
		// hopefully change later once we have a dynamically recompiled memcpy.
		throw Exception::HardwareDeficiency(L"MMX Extensions not available.", _("PCSX2 requires cpu with MMX instruction to run."));
	}

	ReadUserModeSettings();
	AppConfig_OnChangedSettingsFolder();

	PostAppMethod( &Pcsx2App::OpenMainFrame );
	PostAppMethod( &Pcsx2App::OpenProgramLog );
	PostAppMethod( &Pcsx2App::AllocateCoreStuffs );
}

void Pcsx2App::OpenMainFrame()
{
	if( GetMainFramePtr() != NULL ) return;

	MainEmuFrame* mainFrame = new MainEmuFrame( NULL, L"PCSX2" );
	m_id_MainFrame = mainFrame->GetId();

	if( wxWindow* deleteme = GetProgramLog() )
	{
		deleteme->Destroy();
		g_Conf->ProgLogBox.Visible = true;
		m_id_ProgramLogBox = wxID_ANY;
		PostIdleAppMethod( &Pcsx2App::OpenProgramLog );
	}

	SetTopWindow( mainFrame );		// not really needed...
	SetExitOnFrameDelete( true );	// but being explicit doesn't hurt...
	mainFrame->Show();
}

void Pcsx2App::OpenProgramLog()
{
	if( GetProgramLog() != NULL ) return;
	wxWindow* m_current_focus = wxGetActiveWindow();
	m_id_ProgramLogBox	= (new ConsoleLogFrame( GetMainFramePtr(), L"PCSX2 Program Log", g_Conf->ProgLogBox ))->GetId();
	EnableAllLogging();
	
	if( m_current_focus ) m_current_focus->SetFocus();
}

void Pcsx2App::AllocateCoreStuffs()
{
	CpuCheckSSE2();
	SysLogMachineCaps();
	AppApplySettings();

	if( !m_CoreAllocs )
	{
		m_CoreAllocs = new SysCoreAllocations();

		if( m_CoreAllocs->HadSomeFailures( g_Conf->EmuOptions.Cpu.Recompiler ) )
		{
			// HadSomeFailures only returns 'true' if an *enabled* cpu type fails to init.  If
			// the user already has all interps configured, for example, then no point in
			// popping up this dialog.
			
			wxDialogWithHelpers exconf( NULL, _("PCSX2 Recompiler Error(s)") );

			exconf += 12;
			exconf += exconf.Heading( pxE( ".Popup:RecompilerInit",
				L"Warning: Some of the configured PS2 recompilers failed to initialize and will not be available for this session:\n" )
			);

			wxTextCtrl* scrollableTextArea = new wxTextCtrl(
				&exconf, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
				wxTE_READONLY | wxTE_MULTILINE | wxTE_WORDWRAP
			);

			exconf += scrollableTextArea	| pxSizerFlags::StdExpand();
			
			if( !m_CoreAllocs->IsRecAvailable_EE() )
			{
				scrollableTextArea->AppendText( L"* R5900 (EE)\n\n" );

				g_Conf->EmuOptions.Recompiler.EnableEE = false;
			}

			if( !m_CoreAllocs->IsRecAvailable_IOP() )
			{
				scrollableTextArea->AppendText( L"* R3000A (IOP)\n\n" );
				g_Conf->EmuOptions.Recompiler.EnableIOP = false;
			}

			if( !m_CoreAllocs->IsRecAvailable_MicroVU0() )
			{
				scrollableTextArea->AppendText( L"* microVU0\n\n" );
				g_Conf->EmuOptions.Recompiler.UseMicroVU0	= false;
				g_Conf->EmuOptions.Recompiler.EnableVU0		= g_Conf->EmuOptions.Recompiler.EnableVU0 && m_CoreAllocs->IsRecAvailable_SuperVU0();
			}

			if( !m_CoreAllocs->IsRecAvailable_MicroVU1() )
			{
				scrollableTextArea->AppendText( L"* microVU1\n\n" );
				g_Conf->EmuOptions.Recompiler.UseMicroVU1	= false;
				g_Conf->EmuOptions.Recompiler.EnableVU1		= g_Conf->EmuOptions.Recompiler.EnableVU1 && m_CoreAllocs->IsRecAvailable_SuperVU1();
			}

			if( !m_CoreAllocs->IsRecAvailable_SuperVU0() )
			{
				scrollableTextArea->AppendText( L"* SuperVU0\n\n" );
				g_Conf->EmuOptions.Recompiler.UseMicroVU0	= m_CoreAllocs->IsRecAvailable_MicroVU0();
				g_Conf->EmuOptions.Recompiler.EnableVU0		= g_Conf->EmuOptions.Recompiler.EnableVU0 && g_Conf->EmuOptions.Recompiler.UseMicroVU0;
			}

			if( !m_CoreAllocs->IsRecAvailable_SuperVU1() )
			{
				scrollableTextArea->AppendText( L"* SuperVU1\n\n" );
				g_Conf->EmuOptions.Recompiler.UseMicroVU1	= m_CoreAllocs->IsRecAvailable_MicroVU1();
				g_Conf->EmuOptions.Recompiler.EnableVU1		= g_Conf->EmuOptions.Recompiler.EnableVU1 && g_Conf->EmuOptions.Recompiler.UseMicroVU1;
			}

			exconf += new ModalButtonPanel( &exconf, MsgButtons().OK() ) | pxSizerFlags::StdCenter();

			exconf.ShowModal();

			// Failures can be SSE-related OR memory related.  Should do per-cpu error reports instead...

			/*message += pxE( ".Popup Error:EmuCore:MemoryForRecs",
				L"These errors are the result of memory allocation failures (see the program log for details). "
				L"Closing out some memory hogging background tasks may resolve this error.\n\n"
				L"These recompilers have been disabled and interpreters will be used in their place.  "
				L"Interpreters can be very slow, so don't get too excited.  Press OK to continue or CANCEL to close PCSX2."
			);*/

			//if( !Msgbox::OkCancel( message, _("PCSX2 Initialization Error"), wxICON_ERROR ) )
			//	return false;
		}
	}

	LoadPluginsPassive();
}


void Pcsx2App::OnInitCmdLine( wxCmdLineParser& parser )
{
	parser.SetLogo( (wxString)L" >>  PCSX2  --  A Playstation2 Emulator for the PC  <<\n\n" +
		_("All options are for the current session only and will not be saved.\n")
	);

	parser.AddParam( _("IsoFile"), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL );

	parser.AddSwitch( L"h",			L"help",	_("displays this list of command line options"), wxCMD_LINE_OPTION_HELP );

	parser.AddSwitch( wxEmptyString,L"nogui",	_("disables display of the gui while running games") );
	parser.AddSwitch( wxEmptyString,L"skipbios",_("skips standard BIOS splash screens and software checks") );
	parser.AddOption( wxEmptyString,L"elf",		_("executes an ELF image"), wxCMD_LINE_VAL_STRING );
	parser.AddSwitch( wxEmptyString,L"nodisc",	_("boots an empty dvd tray; use to enter the PS2 system menu") );
	parser.AddSwitch( wxEmptyString,L"usecd",	_("boots from the configured CDVD plugin (ignores IsoFile parameter)") );

	parser.AddOption( wxEmptyString,L"cfgpath",	_("changes the configuration file path"), wxCMD_LINE_VAL_STRING );
	parser.AddOption( wxEmptyString,L"cfg",		_("specifies the PCSX2 configuration file to use [not implemented]"), wxCMD_LINE_VAL_STRING );

	parser.AddSwitch( wxEmptyString,L"forcewiz",_("Forces PCSX2 to start the First-time Wizard") );

	const PluginInfo* pi = tbl_PluginInfo; do {
		parser.AddOption( wxEmptyString, pi->GetShortname().Lower(),
			wxsFormat( _("specify the file to use as the %s plugin"), pi->GetShortname().c_str() )
		);
	} while( ++pi, pi->shortname != NULL );

	parser.SetSwitchChars( L"-" );
}

bool Pcsx2App::OnCmdLineError( wxCmdLineParser& parser )
{
	wxApp::OnCmdLineError( parser );
	return false;
}

bool Pcsx2App::OnCmdLineParsed( wxCmdLineParser& parser )
{
	if( parser.GetParamCount() >= 1 )
	{
		// [TODO] : Unnamed parameter is taken as an "autorun" option for a cdvd/iso.
		parser.GetParam( 0 );
	}

	// Suppress wxWidgets automatic options parsing since none of them pertain to PCSX2 needs.
	//wxApp::OnCmdLineParsed( parser );

	//bool yay = parser.Found(L"nogui");
	m_ForceWizard = parser.Found( L"forcewiz" );

	const PluginInfo* pi = tbl_PluginInfo; do
	{
		wxString dest;
		if( !parser.Found( pi->GetShortname().Lower(), &dest ) ) continue;

		OverrideOptions.Filenames.Plugins[pi->id] = dest;

		if( wxFileExists( dest ) )
			Console.Warning( pi->GetShortname() + L" override: " + dest );
		else
		{
			bool result = Msgbox::OkCancel(
				wxsFormat( _("Plugin Override Error!  Specified %s plugin does not exist:\n\n"), pi->GetShortname().c_str() ) +
				dest +
				_("Press OK to use the default configured plugin, or Cancel to close."),
				_("Plugin Override Error - PCSX2"), wxICON_ERROR
			);

			if( !result ) return false;
		}
	} while( ++pi, pi->shortname != NULL );

	parser.Found( L"cfgpath", &OverrideOptions.SettingsFolder );

	return true;
}

typedef void (wxEvtHandler::*pxInvokeAppMethodEventFunction)(Pcsx2AppMethodEvent&);
typedef void (wxEvtHandler::*pxStuckThreadEventHandler)(pxMessageBoxEvent&);

bool Pcsx2App::OnInit()
{
	InitCPUTicks();

#define pxAppMethodEventHandler(func) \
	(wxObjectEventFunction)(wxEventFunction)wxStaticCastEvent(pxInvokeAppMethodEventFunction, &func )

	pxDoAssert = AppDoAssert;

	g_Conf = new AppConfig();
	EnableAllLogging();

    wxInitAllImageHandlers();
	if( !_parent::OnInit() ) return false;

	m_StdoutRedirHandle = NewPipeRedir(stdout);
	m_StderrRedirHandle = NewPipeRedir(stderr);
	wxLocale::AddCatalogLookupPathPrefix( wxGetCwd() );

	Connect( pxID_PadHandler_Keydown,	wxEVT_KEY_DOWN,		wxKeyEventHandler		(Pcsx2App::OnEmuKeyDown) );

	Connect( wxEVT_DESTROY,		wxWindowDestroyEventHandler	(Pcsx2App::OnDestroyWindow) );

	// User/Admin Mode Dual Setup:
	//   PCSX2 now supports two fundamental modes of operation.  The default is Classic mode,
	//   which uses the Current Working Directory (CWD) for all user data files, and requires
	//   Admin access on Vista (and some Linux as well).  The second mode is the Vista-
	//   compatible \documents folder usage.  The mode is determined by the presence and
	//   contents of a usermode.ini file in the CWD.  If the ini file is missing, we assume
	//   the user is setting up a classic install.  If the ini is present, we read the value of
	//   the UserMode and SettingsPath vars.
	//
	//   Conveniently this dual mode setup applies equally well to most modern Linux distros.

	try
	{
		InitDefaultGlobalAccelerators();
		delete wxLog::SetActiveTarget( new pxLogConsole() );

		m_RecentIsoList = new RecentIsoList();

#ifdef __WXMSW__
		pxDwm_Load();
#endif

		SysExecutorThread.Start();
		DetectCpuAndUserMode();
	}
	// ----------------------------------------------------------------------------
	catch( Exception::StartupAborted& ex )		// user-aborted, no popups needed.
	{
		Console.Warning( ex.FormatDiagnosticMessage() );
		CleanupOnExit();
		return false;
	}
	catch( Exception::HardwareDeficiency& ex )
	{
		Msgbox::Alert( ex.FormatDisplayMessage() + _("\n\nPress OK to close PCSX2."), _("PCSX2 Error: Hardware Deficiency") );
		CleanupOnExit();
		return false;
	}
	// ----------------------------------------------------------------------------
	// Failures on the core initialization procedure (typically OutOfMemory errors) are bad,
	// since it means the emulator is completely non-functional.  Let's pop up an error and
	// exit gracefully-ish.
	//
	catch( Exception::RuntimeError& ex )
	{
		Console.Error( ex.FormatDiagnosticMessage() );
		Msgbox::Alert( ex.FormatDisplayMessage() + _("\n\nPress OK to close PCSX2."),
			_("PCSX2 Critical Error"), wxICON_ERROR );
		CleanupOnExit();
		return false;
	}
    return true;
}

// This cleanup procedure can only be called when the App message pump is still active.
// OnExit() must use CleanupOnExit instead.
void Pcsx2App::CleanupRestartable()
{
	AffinityAssert_AllowFrom_MainUI();

	ShutdownPlugins();
	SysExecutorThread.ShutdownQueue();

	//PingDispatcher( "Cleanup" );
	//DeletionDispatcher();
	IdleEventDispatcher( L"Cleanup" );

	if( g_Conf )
		AppSaveSettings();
}

// This cleanup handler can be called from OnExit (it doesn't need a running message pump),
// but should not be called from the App destructor.  It's needed because wxWidgets doesn't
// always call OnExit(), so I had to make CleanupRestartable, and then encapsulate it here
// to be friendly to the OnExit scenario (no message pump).
void Pcsx2App::CleanupOnExit()
{
	AffinityAssert_AllowFrom_MainUI();

	try
	{
		CleanupRestartable();
		CleanupResources();
	}
	catch( Exception::CancelEvent& )		{ throw; }
	catch( Exception::RuntimeError& ex )
	{
		// Handle runtime errors gracefully during shutdown.  Mostly these are things
		// that we just don't care about by now, and just want to "get 'er done!" so
		// we can exit the app. ;)

		Console.Error( L"Runtime exception handled during CleanupOnExit:\n" );
		Console.Indent().Error( ex.FormatDiagnosticMessage() );
	}

#ifdef __WXMSW__
	pxDwm_Unload();
#endif
	
	// Notice: deleting the plugin manager (unloading plugins) here causes Lilypad to crash,
	// likely due to some pending message in the queue that references lilypad procs.
	// We don't need to unload plugins anyway tho -- shutdown is plenty safe enough for
	// closing out all the windows.  So just leave it be and let the plugins get unloaded
	// during the wxApp destructor. -- air
	
	// FIXME: performing a wxYield() here may fix that problem. -- air

	pxDoAssert = pxAssertImpl_LogIt;
	Console_SetActiveHandler( ConsoleWriter_Stdout );
}

void Pcsx2App::CleanupResources()
{
	delete wxConfigBase::Set( NULL );

	while( wxGetLocale() != NULL )
		delete wxGetLocale();

	m_Resources = NULL;
}

int Pcsx2App::OnExit()
{
	CleanupOnExit();

	return wxApp::OnExit();
}

void Pcsx2App::OnDestroyWindow( wxWindowDestroyEvent& evt )
{
	// Precautions:
	//  * Whenever windows are destroyed, make sure to check if it matches our "active"
	//    console logger.  If so, we need to disable logging to the console window, or else
	//    it'll crash.  (this is because the console log system uses a cached window handle
	//    instead of looking the window up via it's ID -- fast but potentially unsafe).
	//
	//  * The virtual machine's plugins usually depend on the GS window handle being valid,
	//    so if the GS window is the one being shut down then we need to make sure to close
	//    out the Corethread before it vanishes completely from existence.
	

	OnProgramLogClosed( evt.GetId() );
	OnGsFrameClosed( evt.GetId() );
	evt.Skip();
}

// --------------------------------------------------------------------------------------
//  SysEventHandler
// --------------------------------------------------------------------------------------
class SysEvtHandler : public pxEvtHandler
{
public:
	wxString GetEvtHandlerName() const { return L"SysExecutor"; }

protected:
	// When the SysExec message queue is finally empty, we should check the state of
	// the menus and make sure they're all consistent to the current emulation states.
	void _DoIdle()
	{
		UI_UpdateSysControls();
	}
};


Pcsx2App::Pcsx2App() 
	: SysExecutorThread( new SysEvtHandler() )
{
	m_PendingSaves			= 0;
	m_ScheduledTermination	= false;

	m_id_MainFrame		= wxID_ANY;
	m_id_GsFrame		= wxID_ANY;
	m_id_ProgramLogBox	= wxID_ANY;

	SetAppName( L"pcsx2" );
	BuildCommandHash();
}

Pcsx2App::~Pcsx2App()
{
	pxDoAssert = pxAssertImpl_LogIt;
}

void Pcsx2App::CleanUp()
{
	CleanupResources();
	m_Resources		= NULL;
	m_RecentIsoList	= NULL;

	DisableDiskLogging();

	if( emuLog != NULL )
	{
		fclose( emuLog );
		emuLog = NULL;
	}

	_parent::CleanUp();
}


// ------------------------------------------------------------------------------------------
//  Using the MSVCRT to track memory leaks:
// ------------------------------------------------------------------------------------------
// When exiting PCSX2 normally, the CRT will make a list of all memory that's leaked.  The
// number inside {} can be pasted into the line below to cause MSVC to breakpoint on that
// allocation at the time it's made.  And then using a stacktrace you can figure out what
// leaked! :D
//
// Limitations: Unfortunately, wxWidgets gui uses a lot of heap allocations while handling
// messages, and so any mouse movements will pretty much screw up the leak value.  So to use
// this feature you need to execute pcsx in no-gui mode, and then not move the mouse or use
// the keyboard until you get to the leak. >_<
//
// (but this tool is still better than nothing!)

#ifdef PCSX2_DEBUG
struct CrtDebugBreak
{
	CrtDebugBreak( int spot )
	{
#ifdef __WXMSW__
		_CrtSetBreakAlloc( spot );
#endif
	}
};

//CrtDebugBreak breakAt( 8564 );

#endif
