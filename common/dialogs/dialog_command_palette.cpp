/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <dialogs/dialog_command_palette.h>

#include <algorithm>
#include <set>

#include <wx/listbox.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>

#include <hotkeys_basic.h>
#include <kicad_fuzzy_match.h>
#include <tool/action_manager.h>
#include <tool/actions.h>
#include <tool/tool_action.h>
#include <tool/tool_manager.h>

namespace
{
/// Cap on how many results are shown at once (a palette is for finding, not browsing everything).
constexpr int MAX_RESULTS = 75;
}


DIALOG_COMMAND_PALETTE::DIALOG_COMMAND_PALETTE( wxWindow* aParent, TOOL_MANAGER* aToolManager ) :
        DIALOG_SHIM( aParent, wxID_ANY, _( "Command Palette" ), wxDefaultPosition,
                     wxSize( 520, 440 ), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER ),
        m_toolMgr( aToolManager ),
        m_selectedAction( nullptr ),
        m_queryCtrl( nullptr ),
        m_resultsList( nullptr )
{
    wxBoxSizer* mainSizer = new wxBoxSizer( wxVERTICAL );

    m_queryCtrl = new wxTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                  wxTE_PROCESS_ENTER );
    m_queryCtrl->SetHint( _( "Type a command…" ) );
    mainSizer->Add( m_queryCtrl, 0, wxEXPAND | wxALL, 6 );

    m_resultsList = new wxListBox( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, nullptr,
                                   wxLB_SINGLE | wxLB_NEEDED_SB );
    mainSizer->Add( m_resultsList, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6 );

    SetSizer( mainSizer );

    collectActions();
    rebuildList();

    m_queryCtrl->Bind( wxEVT_TEXT, &DIALOG_COMMAND_PALETTE::onQueryChanged, this );
    m_resultsList->Bind( wxEVT_LISTBOX_DCLICK, &DIALOG_COMMAND_PALETTE::onListActivate, this );
    Bind( wxEVT_CHAR_HOOK, &DIALOG_COMMAND_PALETTE::onCharHook, this );

    Centre();

    // On macOS (and some wx builds) SetFocus() in the constructor runs before the dialog is shown
    // and does not stick, so defer it until the event loop has presented the window.
    CallAfter(
            [this]()
            {
                m_queryCtrl->SetFocus();
                m_queryCtrl->SelectAll();
            } );
}


void DIALOG_COMMAND_PALETTE::collectActions()
{
    std::set<const TOOL_ACTION*> seen;

    for( const auto& [name, action] : m_toolMgr->GetActionManager()->GetActions() )
    {
        if( !action || action == &ACTIONS::commandPalette )
            continue;

        // Prefer the friendly name; fall back to the menu label so commands defined with only
        // MenuText (no FriendlyName) are still reachable. Skip anything with no user-facing label.
        wxString friendly = action->GetFriendlyName();

        if( friendly.IsEmpty() )
            friendly = action->GetMenuLabel();

        if( friendly.IsEmpty() )
            continue;

        friendly.Replace( wxT( "&" ), wxT( "" ) );  // strip menu-accelerator markers for display

        if( !seen.insert( action ).second )
            continue;

        wxString hotkeyText;
        int      hotkey = m_toolMgr->GetActionManager()->GetHotKey( *action );

        if( hotkey != 0 )
            hotkeyText = KeyNameFromKeyCode( hotkey );

        m_entries.push_back( { action, friendly, hotkeyText } );
    }

    // Stable alphabetical baseline so an empty query is deterministic and browsable.
    std::stable_sort( m_entries.begin(), m_entries.end(),
                      []( const ENTRY& a, const ENTRY& b )
                      {
                          return a.m_name.CmpNoCase( b.m_name ) < 0;
                      } );
}


void DIALOG_COMMAND_PALETTE::rebuildList()
{
    const wxString query = m_queryCtrl->GetValue();

    std::vector<std::pair<int, const ENTRY*>> scored;

    for( const ENTRY& entry : m_entries )
    {
        const int score = KIFUZZY::FuzzyScore( query, entry.m_name );

        if( score != KIFUZZY::NO_MATCH )
            scored.push_back( { score, &entry } );
    }

    // Highest score first; ties fall back to the alphabetical order established in collectActions().
    std::stable_sort( scored.begin(), scored.end(),
                      []( const auto& a, const auto& b )
                      {
                          return a.first > b.first;
                      } );

    m_shown.clear();
    wxArrayString rows;

    for( const auto& [score, entry] : scored )
    {
        if( static_cast<int>( m_shown.size() ) >= MAX_RESULTS )
            break;

        wxString row = entry->m_name;

        if( !entry->m_hotkey.IsEmpty() )
            row << wxT( "\t" ) << entry->m_hotkey;

        rows.Add( row );
        m_shown.push_back( entry );
    }

    m_resultsList->Set( rows );

    if( !m_shown.empty() )
        m_resultsList->SetSelection( 0 );
}


void DIALOG_COMMAND_PALETTE::acceptSelection()
{
    const int idx = m_resultsList->GetSelection();

    if( idx == wxNOT_FOUND || idx < 0 || idx >= static_cast<int>( m_shown.size() ) )
        return;

    m_selectedAction = m_shown[idx]->m_action;
    EndModal( wxID_OK );
}


void DIALOG_COMMAND_PALETTE::onQueryChanged( wxCommandEvent& aEvent )
{
    rebuildList();
}


void DIALOG_COMMAND_PALETTE::onListActivate( wxCommandEvent& aEvent )
{
    acceptSelection();
}


void DIALOG_COMMAND_PALETTE::onCharHook( wxKeyEvent& aEvent )
{
    // Keep focus in the search box while still driving the results list from the keyboard.
    switch( aEvent.GetKeyCode() )
    {
    case WXK_DOWN:
    case WXK_UP:
    case WXK_PAGEDOWN:
    case WXK_PAGEUP:
    {
        const int count = static_cast<int>( m_shown.size() );

        if( count == 0 )
            return;

        int sel  = m_resultsList->GetSelection();
        int step = ( aEvent.GetKeyCode() == WXK_PAGEDOWN || aEvent.GetKeyCode() == WXK_PAGEUP ) ? 10
                                                                                                : 1;

        if( aEvent.GetKeyCode() == WXK_UP || aEvent.GetKeyCode() == WXK_PAGEUP )
            step = -step;

        if( sel == wxNOT_FOUND )
            sel = 0;

        sel = std::clamp( sel + step, 0, count - 1 );
        m_resultsList->SetSelection( sel );
        return; // consumed
    }

    case WXK_RETURN:
    case WXK_NUMPAD_ENTER:
        acceptSelection();
        return; // consumed

    case WXK_ESCAPE:
        EndModal( wxID_CANCEL );
        return; // consumed

    default:
        aEvent.Skip(); // ordinary typing flows to the search box
        break;
    }
}
