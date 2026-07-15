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
#include <dialog_shim.h>

class TOOL_MANAGER;
class TOOL_ACTION;
class wxTextCtrl;
class wxListBox;

/**
 * A searchable "command palette" (Ctrl/Cmd-K): type part of a command name, fuzzy-ranked results
 * appear, Enter runs the top hit. It is a search-and-run view over the existing action registry
 * (ACTION_MANAGER), so it inherits every editor's commands with no per-command wiring.
 *
 * The dialog does not itself invoke the action; on OK it exposes the chosen TOOL_ACTION via
 * GetSelectedAction() and the caller runs it after the modal closes.
 */
class DIALOG_COMMAND_PALETTE : public DIALOG_SHIM
{
public:
    DIALOG_COMMAND_PALETTE( wxWindow* aParent, TOOL_MANAGER* aToolManager );
    ~DIALOG_COMMAND_PALETTE() override = default;

    /// The action the user chose, or nullptr if the dialog was cancelled.
    const TOOL_ACTION* GetSelectedAction() const { return m_selectedAction; }

private:
    /// One selectable command plus its precomputed display strings.
    struct ENTRY
    {
        const TOOL_ACTION* m_action;
        wxString           m_name;      ///< Friendly name (search target + primary label).
        wxString           m_hotkey;    ///< Rendered shortcut, or empty.
    };

    void collectActions();
    void rebuildList();
    void acceptSelection();

    void onQueryChanged( wxCommandEvent& aEvent );
    void onListActivate( wxCommandEvent& aEvent );
    void onCharHook( wxKeyEvent& aEvent );

    TOOL_MANAGER*      m_toolMgr;
    const TOOL_ACTION* m_selectedAction;

    wxTextCtrl*        m_queryCtrl;
    wxListBox*         m_resultsList;

    std::vector<ENTRY>        m_entries;   ///< All eligible commands.
    std::vector<const ENTRY*> m_shown;     ///< Entries currently displayed, parallel to the listbox.
};

#endif // DIALOG_COMMAND_PALETTE_H
