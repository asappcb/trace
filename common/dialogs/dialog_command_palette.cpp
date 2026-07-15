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
#include <eda_draw_frame.h>
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


/// Common alternate terms -> a word that actually appears in the matching command's name, so a
/// user who types the word they have in mind still finds the command KiCad happens to call
/// something else. Applied in addition to a direct match, never instead of it.
struct ALIAS
{
    const wxChar* m_typed;
    const wxChar* m_canonical;
};

const ALIAS ALIASES[] = {
    // General
    { wxT( "delete" ), wxT( "remove" ) },
    { wxT( "measure" ), wxT( "ruler" ) },
    { wxT( "find" ), wxT( "search" ) },
    { wxT( "shortcut" ), wxT( "hotkey" ) },
    { wxT( "shortcuts" ), wxT( "hotkey" ) },
    { wxT( "settings" ), wxT( "preferences" ) },
    { wxT( "config" ), wxT( "preferences" ) },
    { wxT( "props" ), wxT( "properties" ) },
    { wxT( "duplicate" ), wxT( "copy" ) },
    { wxT( "flip" ), wxT( "mirror" ) },
    // Copper / geometry (KiCad's own vocabulary)
    { wxT( "trace" ), wxT( "track" ) },
    { wxT( "wire" ), wxT( "track" ) },
    { wxT( "pour" ), wxT( "zone" ) },
    { wxT( "fill" ), wxT( "zone" ) },
    { wxT( "keepout" ), wxT( "rule area" ) },
    { wxT( "silk" ), wxT( "silkscreen" ) },
    { wxT( "rats" ), wxT( "ratsnest" ) },
    { wxT( "push" ), wxT( "route" ) },
    { wxT( "shove" ), wxT( "route" ) },
    { wxT( "tune" ), wxT( "length" ) },
    { wxT( "via" ), wxT( "track" ) },
    // Parts / nets
    { wxT( "component" ), wxT( "footprint" ) },
    { wxT( "part" ), wxT( "footprint" ) },
    { wxT( "label" ), wxT( "net" ) },
    { wxT( "netlist" ), wxT( "net" ) },
    { wxT( "renumber" ), wxT( "annotate" ) },
    { wxT( "reference" ), wxT( "annotate" ) },
    // Rules / setup
    { wxT( "constraints" ), wxT( "rules" ) },
    { wxT( "drc" ), wxT( "design rules" ) },
    { wxT( "erc" ), wxT( "electrical rules" ) },
    { wxT( "stackup" ), wxT( "board setup" ) },
    // Output
    { wxT( "gerber" ), wxT( "fabrication" ) },
    { wxT( "print" ), wxT( "plot" ) },
    { wxT( "centroid" ), wxT( "placement" ) },
    { wxT( "pick" ), wxT( "placement" ) },
    { wxT( "3d" ), wxT( "3d viewer" ) },
    { wxT( "bom" ), wxT( "bill of materials" ) },
};


/// Expand a query into the terms to match against: the query itself, plus the canonical word for
/// any alias the query is a prefix of. The first element is always the original query, so its match
/// positions drive highlighting when it is what actually hit.
std::vector<wxString> expandQuery( const wxString& aQuery )
{
    std::vector<wxString> terms;
    terms.push_back( aQuery );

    if( aQuery.IsEmpty() )
        return terms;

    const wxString lower = aQuery.Lower();

    for( const ALIAS& alias : ALIASES )
    {
        const wxString typed = wxString( alias.m_typed );

        if( typed.StartsWith( lower ) )
            terms.push_back( wxString( alias.m_canonical ) );
    }

    return terms;
}
}


/**
 * Owner-drawn results list: each row is an icon, the item name with its matched characters
 * highlighted, and a right-aligned qualifier (a hotkey for commands, a type for navigation).
 * Context-invalid items are drawn greyed.
 */
class COMMAND_PALETTE_LIST : public wxVListBox
{
public:
    struct ROW
    {
        wxString         m_name;
        wxString         m_right; ///< Right-aligned text (hotkey or detail).
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

        if( !row.m_right.IsEmpty() )
        {
            aDC.SetTextForeground( subtle );
            wxSize hs = aDC.GetTextExtent( row.m_right );
            aDC.DrawText( row.m_right, aRect.GetRight() - hs.GetWidth() - pad, ty );
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
    m_queryCtrl->SetHint( _( "Type a command…  (> commands, @ go to)" ) );
    mainSizer->Add( m_queryCtrl, 0, wxEXPAND | wxALL, 6 );

    m_resultsList = new COMMAND_PALETTE_LIST( this );
    mainSizer->Add( m_resultsList, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6 );

    SetSizer( mainSizer );

    loadMru();
    collectItems();
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


void DIALOG_COMMAND_PALETTE::recordMru( const wxString& aMruKey )
{
    if( aMruKey.IsEmpty() )
        return;

    COMMON_SETTINGS* cfg = Pgm().GetCommonSettings();

    if( !cfg )
        return;

    auto& mru = cfg->m_Session.command_palette_mru;

    mru.erase( std::remove( mru.begin(), mru.end(), aMruKey ), mru.end() );
    mru.insert( mru.begin(), aMruKey );

    if( mru.size() > MRU_LIMIT )
        mru.resize( MRU_LIMIT );
}


void DIALOG_COMMAND_PALETTE::collectItems()
{
    ACTION_MANAGER* actionMgr = m_toolMgr->GetActionManager();
    SELECTION&      selection = m_toolMgr->GetToolHolder()->GetCurrentSelection();
    TOOL_MANAGER*   toolMgr = m_toolMgr;

    std::set<const TOOL_ACTION*> seen;

    for( const auto& [name, action] : actionMgr->GetActions() )
    {
        if( !action || action == &ACTIONS::commandPalette )
            continue;

        // Skip commands that no tool in this editor handles -- the action registry is process-wide,
        // so e.g. the PCB-only "3D Viewer" would otherwise appear (and do nothing) in eeschema.
        if( !m_toolMgr->HasHandlerForAction( *action ) )
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

        COMMAND_PALETTE_ITEM item;
        item.m_name = friendly;
        item.m_category = COMMAND_PALETTE_ITEM::CATEGORY::COMMAND;
        item.m_mruKey = wxString( action->GetName() );

        if( int hotkey = actionMgr->GetHotKey( *action ) )
            item.m_hotkey = KeyNameFromKeyCode( hotkey );

        if( const ACTION_CONDITIONS* cond = actionMgr->GetCondition( *action ) )
            item.m_enabled = cond->GetHotkeyCondition()( selection );

        if( action->GetIcon() != BITMAPS::INVALID_BITMAP )
            item.m_icon = KiBitmapBundle( action->GetIcon(), 16 );

        const TOOL_ACTION* act = action;
        item.m_run = [toolMgr, act]()
        {
            toolMgr->RunAction( *act );
        };

        m_items.push_back( std::move( item ) );
    }

    // Editor-supplied navigation targets (nets, footprints, sheets, …).
    if( EDA_DRAW_FRAME* frame = dynamic_cast<EDA_DRAW_FRAME*>( GetParent() ) )
        frame->GetCommandPaletteItems( m_items );

    // Stable alphabetical baseline so ordering is deterministic before ranking.
    std::stable_sort( m_items.begin(), m_items.end(),
                      []( const COMMAND_PALETTE_ITEM& a, const COMMAND_PALETTE_ITEM& b )
                      {
                          return a.m_name.CmpNoCase( b.m_name ) < 0;
                      } );
}


void DIALOG_COMMAND_PALETTE::rebuildList()
{
    wxString query = m_queryCtrl->GetValue();

    // Leading sigil scopes the search to one category.
    bool                           scoped = false;
    COMMAND_PALETTE_ITEM::CATEGORY only = COMMAND_PALETTE_ITEM::CATEGORY::COMMAND;

    if( query.StartsWith( wxT( ">" ) ) )
    {
        scoped = true;
        only = COMMAND_PALETTE_ITEM::CATEGORY::COMMAND;
        query = query.Mid( 1 );
    }
    else if( query.StartsWith( wxT( "@" ) ) )
    {
        scoped = true;
        only = COMMAND_PALETTE_ITEM::CATEGORY::NAVIGATE;
        query = query.Mid( 1 );
    }

    query.Trim( false );
    const bool empty = query.IsEmpty();

    // The query plus any alias canonicals ("delete" -> "remove", …); each item takes its best.
    const std::vector<wxString> terms = expandQuery( query );

    std::map<wxString, int> mruRank;

    for( int i = 0; i < static_cast<int>( m_mru.size() ); ++i )
        mruRank[m_mru[i]] = i;

    struct SCORED
    {
        const COMMAND_PALETTE_ITEM* m_item;
        int                         m_score;
        int                         m_mru;
        std::vector<int>            m_matched;
    };

    std::vector<SCORED> scored;

    for( const COMMAND_PALETTE_ITEM& item : m_items )
    {
        if( scoped && item.m_category != only )
            continue;

        // In the default (no-sigil) empty view, don't flood the list with every navigation target;
        // they surface once the user types (or scopes with '@').
        if( !scoped && empty && item.m_category == COMMAND_PALETTE_ITEM::CATEGORY::NAVIGATE )
            continue;

        // Score against the query and any alias canonicals; keep the best. Alias matches (terms
        // past the first) are penalised slightly so a direct hit always outranks an alias hit.
        int              score = KIFUZZY::NO_MATCH;
        std::vector<int> matched;

        for( size_t t = 0; t < terms.size(); ++t )
        {
            std::vector<int> m;
            int              s = KIFUZZY::FuzzyScore( terms[t], item.m_name, m );

            if( s == KIFUZZY::NO_MATCH )
                continue;

            if( t > 0 )
                s -= 6;

            if( s > score )
            {
                score = s;
                matched = std::move( m );
            }
        }

        if( score == KIFUZZY::NO_MATCH )
            continue;

        int mru = -1;

        if( !item.m_mruKey.IsEmpty() )
        {
            auto it = mruRank.find( item.m_mruKey );

            if( it != mruRank.end() )
                mru = it->second;
        }

        scored.push_back( { &item, score, mru, std::move( matched ) } );
    }

    // Ranking: valid-in-context items first. With no query, most-recent then alphabetical.
    // With a query, by match score (recent commands get a small nudge), then alphabetical.
    std::stable_sort( scored.begin(), scored.end(),
                      [empty]( const SCORED& a, const SCORED& b )
                      {
                          if( a.m_item->m_enabled != b.m_item->m_enabled )
                              return a.m_item->m_enabled;

                          if( empty )
                          {
                              const bool am = a.m_mru >= 0;
                              const bool bm = b.m_mru >= 0;

                              if( am != bm )
                                  return am;

                              if( am && bm && a.m_mru != b.m_mru )
                                  return a.m_mru < b.m_mru;

                              return a.m_item->m_name.CmpNoCase( b.m_item->m_name ) < 0;
                          }

                          const int as = a.m_score + ( a.m_mru >= 0 ? MRU_BONUS : 0 );
                          const int bs = b.m_score + ( b.m_mru >= 0 ? MRU_BONUS : 0 );

                          if( as != bs )
                              return as > bs;

                          return a.m_item->m_name.CmpNoCase( b.m_item->m_name ) < 0;
                      } );

    m_shown.clear();
    std::vector<COMMAND_PALETTE_LIST::ROW> rows;

    for( SCORED& sc : scored )
    {
        if( static_cast<int>( m_shown.size() ) >= MAX_RESULTS )
            break;

        const wxString right = sc.m_item->m_hotkey.IsEmpty() ? sc.m_item->m_detail : sc.m_item->m_hotkey;

        m_shown.push_back( sc.m_item );
        rows.push_back(
                { sc.m_item->m_name, right, sc.m_item->m_icon, std::move( sc.m_matched ), sc.m_item->m_enabled } );
    }

    // Nothing matched a non-empty query: show a non-selectable hint rather than a blank void.
    // m_shown stays empty, so keyboard navigation and Enter are inert on this placeholder.
    if( rows.empty() && !empty )
        rows.push_back( { _( "No matching commands" ), wxEmptyString, wxBitmapBundle(), {}, false } );

    m_resultsList->SetRows( std::move( rows ) );
}


void DIALOG_COMMAND_PALETTE::acceptSelection()
{
    const int idx = m_resultsList->GetSelection();

    if( idx == wxNOT_FOUND || idx < 0 || idx >= static_cast<int>( m_shown.size() ) )
        return;

    const COMMAND_PALETTE_ITEM* item = m_shown[idx];

    // Refuse to launch something invalid in the current context (it would no-op anyway).
    if( !item->m_enabled || !item->m_run )
    {
        wxBell();
        return;
    }

    std::function<void()> run = item->m_run; // copy: the item is destroyed with the dialog
    wxWindow*             parent = GetParent();

    recordMru( item->m_mruKey );
    dismiss();

    // Run on the parent frame's next event-loop turn -- after this popup has torn down, but
    // promptly (the wx event loop drains CallAfter continuously). This mirrors how a menu/toolbar
    // click dispatches, and is safe when the item starts an interactive tool.
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
