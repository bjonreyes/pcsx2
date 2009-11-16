/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2009  PCSX2 Dev Team
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
#include "MainFrame.h"

RecentIsoManager::RecentIsoManager( wxMenu* menu ) :
	m_Menu( menu )
,	m_MaxLength( g_Conf->RecentFileCount )
,	m_cursel( 0 )
,	m_Separator( NULL )
,	m_Listener_SettingsLoadSave( wxGetApp().Source_SettingsLoadSave(), EventListener<IniInterface>( this, OnSettingsLoadSave ) )
{
	Connect( wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(RecentIsoManager::OnChangedSelection) );
}

RecentIsoManager::~RecentIsoManager() throw()
{
	Disconnect( wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(RecentIsoManager::OnChangedSelection) );
}

void RecentIsoManager::OnChangedSelection( wxCommandEvent& evt )
{
	uint cnt = m_Items.size();
	uint i=0;
	for( ; i<cnt; ++i )
	{
		if( (m_Items[i].ItemPtr != NULL) && (m_Items[i].ItemPtr->GetId() == evt.GetId()) ) break;
	}
	
	if( i >= m_Items.size() )
	{
		evt.Skip();
		return;
	}

	m_cursel = i;
	
	bool resume = CoreThread.Suspend();
	SysUpdateIsoSrcFile( m_Items[i].Filename );
	if( resume ) CoreThread.Resume();
}

void RecentIsoManager::RemoveAllFromMenu()
{
	if( m_Menu == NULL ) return;

	int cnt = m_Items.size();
	for( int i=0; i<cnt; ++i )
	{
		RecentItem& curitem( m_Items[i] );
		if( curitem.ItemPtr == NULL ) continue;
		m_Menu->Destroy( curitem.ItemPtr );
		curitem.ItemPtr = NULL;
	}
	
	if( m_Separator != NULL )
	{
		m_Menu->Destroy( m_Separator );
		m_Separator = NULL;
	}
}

void RecentIsoManager::Repopulate()
{
	int cnt = m_Items.size();
	if( cnt <= 0 ) return;

	m_Separator = m_Menu->AppendSeparator();
	
	for( int i=0; i<cnt; ++i )
		InsertIntoMenu( i );
}

void RecentIsoManager::Add( const wxString& src )
{
	if( src.IsEmpty() ) return;

	wxFileName temp( src );
	temp.Normalize();
	wxString normalized( temp.GetFullPath() );

	int cnt = m_Items.size();

	if( cnt <= 0 )
	{
		pxAssert( m_Separator == NULL );
		m_Separator = m_Menu->AppendSeparator();
	}
	else
	{
		for( int i=0; i<cnt; ++i )
		{
			if( m_Items[i].Filename == normalized )
			{
				m_cursel = i;
				if( m_Items[i].ItemPtr != NULL )
					m_Items[i].ItemPtr->Check();
				return;
			}
		}
	}

	m_Items.push_back( RecentItem( normalized ) );
	InsertIntoMenu( m_cursel = (m_Items.size()-1) );

	while( m_Items.size() > m_MaxLength )
	{
		m_Items.erase( m_Items.begin() );
	}
}

void RecentIsoManager::InsertIntoMenu( int id )
{
	if( m_Menu == NULL ) return;
	RecentItem& curitem( m_Items[id] );
	curitem.ItemPtr = m_Menu->Append( wxID_ANY, Path::GetFilename(curitem.Filename), curitem.Filename, wxITEM_RADIO );

	if( m_cursel == id )
		curitem.ItemPtr->Check();
}

void RecentIsoManager::DoSettingsLoadSave( IniInterface& ini )
{
	ini.GetConfig().SetRecordDefaults( false );

	if( ini.IsSaving() )
	{
		// Wipe existing recent iso list if we're saving, because our size might have changed
		// and that could leave some residual entries in the config.

		ini.GetConfig().DeleteGroup( L"RecentIso" );
		IniScopedGroup groupie( ini, L"RecentIso" );

		int cnt = m_Items.size();
		for( int i=0; i<cnt; ++i )
		{
			ini.Entry( wxsFormat( L"Filename%02d", i ), m_Items[i].Filename );
		}
	}
	else
	{
		RemoveAllFromMenu();
		
		m_MaxLength = g_Conf->RecentFileCount;
		IniScopedGroup groupie( ini, L"RecentIso" );
		for( uint i=0; i<m_MaxLength; ++i )
		{
			wxString loadtmp;
			ini.Entry( wxsFormat( L"Filename%02d", i ), loadtmp );
			if( !loadtmp.IsEmpty() ) Add( loadtmp );
		}
		Add( g_Conf->CurrentIso );
	}

	ini.GetConfig().SetRecordDefaults( true );
}

void __evt_fastcall RecentIsoManager::OnSettingsLoadSave( void* obj, IniInterface& ini )
{
	if( obj == NULL ) return;
	((RecentIsoManager*)obj)->DoSettingsLoadSave( ini );
}
