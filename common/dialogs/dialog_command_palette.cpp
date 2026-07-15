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
#include <map>
#include <set>

#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>
#include <wx/utils.h>
#include <wx/vlbox.h>

#include <bitmaps/bitmap_types.h>
#include <bitmaps/bitmaps_list.h>
#include <hotkeys_basic.h>
#include <kicad_fuzzy_match.h>
#include <pgm_base.h>
#include <settings/common_settings.h>
#include <tool/action_manager.h>
#include <tool/actions.h>
#include <tool/selection.h>
#include <tool/tool_action.h>
#include <tool/tool_manager.h>

namespace
{
/// Cap on how many results are shown at once (a palette is for finding, not browsing everything).
constexpr int MAX_RESULTS = 75;

/// Small ranking nudge given to recently-run commands on a non-empty query.
constexpr int MRU_BONUS = 8;

/// How many recent commands to remember.
constexpr size_t MRU_LIMIT = 25;
}


/**
 * Owner-drawn results list: each row is an icon, the command name with its matched characters
 * highlighted, and a right-aligned hotkey. Context-invalid commands are drawn greyed.
 */
class COMMAND_PALETTE_LIST : public wxVListBox
{
public:
    struct ROW
    {
        wxString         m_name;
        wxString         m_hotkey;
        wxBitmapBundle   m_icon;
        std::vector<int> m_matched; ///< Indices into m_name to highlight.
        bool             m_enabled;
    };

    COMMAND_PALETTE_LIST( wxWindow* aParent ) :
            wxVListBox( aParent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE )
    {
    }

    void SetRows( std::vector<ROW>&& aRows )
    {
        m_rows = std::move( aRows );
        SetItemCount( m_rows.size() );

        if( !m_rows.empty() )
            SetSelection( 0 );

        RefreshAll();
    }

protected:
    wxCoord OnMeasureItem( size_t ) const override
    {
        return std::max<wxCoord>( GetCharHeight() + FromDIP( 10 ), FromDIP( 24 ) );
    }

    void OnDrawItem( wxDC& aDC, const wxRect& aRect, size_t aItem ) const override
    {
        const ROW& row = m_rows[aItem];
        const bool selected = ( GetSelection() == static_cast<int>( aItem ) );

        const int pad = FromDIP( 6 );
        const int iconSize = FromDIP( 16 );

        wxColour text = selected ? wxSystemSettings::GetColour( wxSYS_COLOUR_HIGHLIGHTTEXT )
                                 : wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOWTEXT );
        wxColour subtle = selected ? text : wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT );
        wxColour accent = selected ? text : wxSystemSettings::GetColour( wxSYS_COLOUR_HOTLIGHT );

        if( !row.m_enabled )
        {
            text = subtle;
            accent = subtle;
        }

        int x = aRect.x + pad;

        if( row.m_icon.IsOk() )
        {
            wxBitmap bmp = row.m_icon.GetBitmap( wxSize( iconSize, iconSize ) );

            if( bmp.IsOk() )
                aDC.DrawBitmap( bmp, x, aRect.y + ( aRect.height - iconSize ) / 2, true );
        }

        x += iconSize + pad;

        const int ty = aRect.y + ( aRect.height - aDC.GetCharHeight() ) / 2;

        // Name, drawn as alternating matched / unmatched runs so matched characters stand out.
        std::vector<bool> hit( row.m_name.length(), false );

        for( int p : row.m_matched )
        {
            if( p >= 0 && p < static_cast<int>( row.m_name.length() ) )
                hit[p] = true;
        }

        size_t i = 0;

        while( i < row.m_name.length() )
        {
            const bool runHit = hit[i];
            size_t     j = i;

            while( j < row.m_name.length() && hit[j] == runHit )
                ++j;

            wxString frag = row.m_name.SubString( i, j - 1 );
            aDC.SetTextForeground( runHit ? accent : text );
            aDC.DrawText( frag, x, ty );
            x += aDC.GetTextExtent( frag ).GetWidth();
            i = j;
        }

        if( !row.m_hotkey.IsEmpty() )
        {
            aDC.SetTextForeground( subtle );
            wxSize hs = aDC.GetTextExtent( row.m_hotkey );
            aDC.DrawText( row.m_hotkey, aRect.GetRight() - hs.GetWidth() - pad, ty );
        }
    }

private:
    std::vector<ROW> m_rows;
};


DIALOG_COMMAND_PALETTE::DIALOG_COMMAND_PALETTE( wxWindow* aParent, TOOL_MANAGER* aToolManager ) :
        DIALOG_SHIM( aParent, wxID_ANY, _( "Command Palette" ), wxDefaultPosition, wxSize( 560, 460 ),
                     wxBORDER_SIMPLE ),
        m_toolMgr( aToolManager ),
        m_ready( false ),
        m_dismissed( false ),
        m_queryCtrl( nullptr ),
        m_resultsList( nullptr )
{
    wxBoxSizer* mainSizer = new wxBoxSizer( wxVERTICAL );

    m_queryCtrl = new wxTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                  wxTE_PROCESS_ENTER );
    m_queryCtrl->SetHint( _( "Type a command…" ) );
    mainSizer->Add( m_queryCtrl, 0, wxEXPAND | wxALL, 6 );

    m_resultsList = new COMMAND_PALETTE_LIST( this );
    mainSizer->Add( m_resultsList, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6 );

    SetSizer( mainSizer );

    loadMru();
    collectActions();
    rebuildList();

    m_queryCtrl->Bind( wxEVT_TEXT, &DIALOG_COMMAND_PALETTE::onQueryChanged, this );
    m_resultsList->Bind( wxEVT_LISTBOX_DCLICK, &DIALOG_COMMAND_PALETTE::onListActivate, this );
    Bind( wxEVT_CHAR_HOOK, &DIALOG_COMMAND_PALETTE::onCharHook, this );
    Bind( wxEVT_ACTIVATE, &DIALOG_COMMAND_PALETTE::onActivate, this );

    Centre();

    // On macOS (and some wx builds) SetFocus() in the constructor runs before the dialog is shown
    // and does not stick, so defer it until the event loop has presented the window. Enabling the
    // close-on-deactivate only here avoids the initial show/activation churn closing us instantly.
    CallAfter(
            [this]()
            {
                m_queryCtrl->SetFocus();
                m_queryCtrl->SelectAll();
                m_ready = true;
            } );
}


void DIALOG_COMMAND_PALETTE::dismiss()
{
    if( m_dismissed )
        return;

    m_dismissed = true;
    Hide();
    Destroy();
}


void DIALOG_COMMAND_PALETTE::onActivate( wxActivateEvent& aEvent )
{
    // Clicking anywhere outside the palette deactivates it -> dismiss, like Spotlight.
    if( m_ready && !aEvent.GetActive() )
        dismiss();

    aEvent.Skip();
}


void DIALOG_COMMAND_PALETTE::loadMru()
{
    if( COMMON_SETTINGS* cfg = Pgm().GetCommonSettings() )
        m_mru = cfg->m_Session.command_palette_mru;
}


void DIALOG_COMMAND_PALETTE::recordMru( const TOOL_ACTION* aAction )
{
    if( !aAction )
        return;

    COMMON_SETTINGS* cfg = Pgm().GetCommonSettings();

    if( !cfg )
        return;

    const wxString name = aAction->GetName();
    auto&          mru = cfg->m_Session.command_palette_mru;

    mru.erase( std::remove( mru.begin(), mru.end(), name ), mru.end() );
    mru.insert( mru.begin(), name );

    if( mru.size() > MRU_LIMIT )
        mru.resize( MRU_LIMIT );
}


void DIALOG_COMMAND_PALETTE::collectActions()
{
    ACTION_MANAGER* actionMgr = m_toolMgr->GetActionManager();
    SELECTION&      selection = m_toolMgr->GetToolHolder()->GetCurrentSelection();

    std::set<const TOOL_ACTION*> seen;

    for( const auto& [name, action] : actionMgr->GetActions() )
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

        if( !seen.insert( action ).second )
            continue;

        friendly.Replace( wxT( "&" ), wxT( "" ) ); // strip menu-accelerator markers for display

        wxString hotkeyText;
        int      hotkey = actionMgr->GetHotKey( *action );

        if( hotkey != 0 )
            hotkeyText = KeyNameFromKeyCode( hotkey );

        bool enabled = true;

        if( const ACTION_CONDITIONS* cond = actionMgr->GetCondition( *action ) )
            enabled = cond->GetHotkeyCondition()( selection );

        wxBitmapBundle icon;

        if( action->GetIcon() != BITMAPS::INVALID_BITMAP )
            icon = KiBitmapBundle( action->GetIcon(), 16 );

        m_entries.push_back( { action, friendly, hotkeyText, icon, enabled } );
    }

    // Stable alphabetical baseline so ordering is deterministic before ranking.
    std::stable_sort( m_entries.begin(), m_entries.end(),
                      []( const ENTRY& a, const ENTRY& b )
                      {
                          return a.m_name.CmpNoCase( b.m_name ) < 0;
                      } );
}


void DIALOG_COMMAND_PALETTE::rebuildList()
{
    const wxString query = m_queryCtrl->GetValue();
    const bool     empty = query.IsEmpty();

    std::map<wxString, int> mruRank;

    for( int i = 0; i < static_cast<int>( m_mru.size() ); ++i )
        mruRank[m_mru[i]] = i;

    struct SCORED
    {
        const ENTRY*     m_entry;
        int              m_score;
        int              m_mru; ///< MRU index, or -1.
        std::vector<int> m_matched;
    };

    std::vector<SCORED> scored;

    for( const ENTRY& entry : m_entries )
    {
        std::vector<int> matched;
        const int        score = KIFUZZY::FuzzyScore( query, entry.m_name, matched );

        if( score == KIFUZZY::NO_MATCH )
            continue;

        auto      it = mruRank.find( wxString( entry.m_action->GetName() ) );
        const int mru = ( it != mruRank.end() ) ? it->second : -1;

        scored.push_back( { &entry, score, mru, std::move( matched ) } );
    }

    // Ranking: valid-in-context commands first. With no query, most-recent then alphabetical.
    // With a query, by match score (recent commands get a small nudge), then alphabetical.
    std::stable_sort( scored.begin(), scored.end(),
                      [empty]( const SCORED& a, const SCORED& b )
                      {
                          if( a.m_entry->m_enabled != b.m_entry->m_enabled )
                              return a.m_entry->m_enabled;

                          if( empty )
                          {
                              const bool am = a.m_mru >= 0;
                              const bool bm = b.m_mru >= 0;

                              if( am != bm )
                                  return am;

                              if( am && bm && a.m_mru != b.m_mru )
                                  return a.m_mru < b.m_mru;

                              return a.m_entry->m_name.CmpNoCase( b.m_entry->m_name ) < 0;
                          }

                          const int as = a.m_score + ( a.m_mru >= 0 ? MRU_BONUS : 0 );
                          const int bs = b.m_score + ( b.m_mru >= 0 ? MRU_BONUS : 0 );

                          if( as != bs )
                              return as > bs;

                          return a.m_entry->m_name.CmpNoCase( b.m_entry->m_name ) < 0;
                      } );

    m_shown.clear();
    std::vector<COMMAND_PALETTE_LIST::ROW> rows;

    for( SCORED& sc : scored )
    {
        if( static_cast<int>( m_shown.size() ) >= MAX_RESULTS )
            break;

        m_shown.push_back( sc.m_entry );
        rows.push_back( { sc.m_entry->m_name, sc.m_entry->m_hotkey, sc.m_entry->m_icon, std::move( sc.m_matched ),
                          sc.m_entry->m_enabled } );
    }

    m_resultsList->SetRows( std::move( rows ) );
}


void DIALOG_COMMAND_PALETTE::acceptSelection()
{
    const int idx = m_resultsList->GetSelection();

    if( idx == wxNOT_FOUND || idx < 0 || idx >= static_cast<int>( m_shown.size() ) )
        return;

    const ENTRY* entry = m_shown[idx];

    // Refuse to launch a command that is invalid in the current context (it would no-op anyway).
    if( !entry->m_enabled )
    {
        wxBell();
        return;
    }

    const TOOL_ACTION* action = entry->m_action;
    TOOL_MANAGER*      toolMgr = m_toolMgr;
    wxWindow*          parent = GetParent();

    recordMru( action );
    dismiss();

    // Run the action on the parent frame's next event-loop turn -- after this popup has torn down,
    // but promptly (the wx event loop drains CallAfter continuously). PostAction would instead sit
    // in the tool-manager queue until the next tool-dispatch event, i.e. until the user next
    // interacts; CallAfter here mirrors how a menu/toolbar click dispatches.
    auto run = [toolMgr, action]()
    {
        toolMgr->RunAction( *action );
    };

    if( parent )
        parent->CallAfter( run );
    else
        run();
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

    case WXK_ESCAPE: dismiss(); return; // consumed

    default:
        aEvent.Skip(); // ordinary typing flows to the search box
        break;
    }
}
