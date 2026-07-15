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

#ifndef DIALOG_COMMAND_PALETTE_H
#define DIALOG_COMMAND_PALETTE_H

#include <vector>
#include <wx/bmpbndl.h>
#include <dialog_shim.h>

class TOOL_MANAGER;
class TOOL_ACTION;
class COMMAND_PALETTE_LIST;
class wxTextCtrl;

/**
 * A searchable "command palette" (Ctrl/Cmd-K): type part of a command name, fuzzy-ranked results
 * appear, Enter runs the top hit. It is a search-and-run view over the existing action registry
 * (ACTION_MANAGER), so it inherits every editor's commands with no per-command wiring.
 *
 * Results show a per-command icon and hotkey, highlight the matched characters, and grey out
 * commands that are not valid in the current context (which are also ranked below valid ones).
 * Recently-run commands are remembered (persisted in COMMON_SETTINGS) and surfaced first.
 *
 * It is shown modeless and chromeless (no title bar) and dismisses itself on Escape or when it
 * loses focus (a click anywhere outside), like a Spotlight/VS-Code command palette. It runs the
 * chosen action itself and then self-destroys, so the caller only needs to construct and Show() it.
 */
class DIALOG_COMMAND_PALETTE : public DIALOG_SHIM
{
public:
    DIALOG_COMMAND_PALETTE( wxWindow* aParent, TOOL_MANAGER* aToolManager );
    ~DIALOG_COMMAND_PALETTE() override = default;

private:
    /// One selectable command plus its precomputed display data.
    struct ENTRY
    {
        const TOOL_ACTION* m_action;
        wxString           m_name;      ///< Friendly name (search target + primary label).
        wxString           m_hotkey;    ///< Rendered shortcut, or empty.
        wxBitmapBundle     m_icon;      ///< Command icon, or empty bundle.
        bool               m_enabled;   ///< Valid in the current context.
    };

    void collectActions();
    void loadMru();
    void recordMru( const TOOL_ACTION* aAction );
    void rebuildList();
    void acceptSelection();
    void dismiss();

    void onQueryChanged( wxCommandEvent& aEvent );
    void onListActivate( wxCommandEvent& aEvent );
    void onCharHook( wxKeyEvent& aEvent );
    void onActivate( wxActivateEvent& aEvent );

    TOOL_MANAGER*      m_toolMgr;
    bool               m_ready;      ///< True once shown; gates the close-on-deactivate.
    bool               m_dismissed;  ///< Guards against dismissing/destroying twice.

    wxTextCtrl*          m_queryCtrl;
    COMMAND_PALETTE_LIST* m_resultsList;

    std::vector<ENTRY>        m_entries;   ///< All eligible commands.
    std::vector<const ENTRY*> m_shown;     ///< Entries currently displayed, parallel to the list rows.
    std::vector<wxString>     m_mru;       ///< Action names, most-recent first (from COMMON_SETTINGS).
};

#endif // DIALOG_COMMAND_PALETTE_H
