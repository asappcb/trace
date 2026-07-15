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
#include <command_palette_item.h>
#include <dialog_shim.h>

class TOOL_MANAGER;
class COMMAND_PALETTE_LIST;
class wxTextCtrl;

/**
 * A searchable "command palette" (Ctrl/Cmd-K): type part of a name, fuzzy-ranked results appear,
 * Enter runs the top hit. Its items come from two sources merged into one list:
 *   - every command in the action registry (ACTION_MANAGER), and
 *   - editor-supplied "go to" targets (EDA_DRAW_FRAME::GetCommandPaletteItems() — nets, footprints,
 *     sheets, …).
 *
 * A leading sigil scopes the search: `>` to commands only, `@` to navigation only.
 *
 * Results show an icon and hotkey, highlight the matched characters, and grey out items that are
 * not valid in the current context (which also rank below valid ones). Recently-run commands are
 * remembered (persisted in COMMON_SETTINGS) and surfaced first.
 *
 * Shown modeless and chromeless; dismisses on Escape or focus loss, runs the chosen item itself,
 * and self-destroys.
 */
class DIALOG_COMMAND_PALETTE : public DIALOG_SHIM
{
public:
    DIALOG_COMMAND_PALETTE( wxWindow* aParent, TOOL_MANAGER* aToolManager );
    ~DIALOG_COMMAND_PALETTE() override = default;

private:
    void collectItems();
    void loadMru();
    void recordMru( const wxString& aMruKey );
    void rebuildList();
    void acceptSelection();
    void dismiss();

    void onQueryChanged( wxCommandEvent& aEvent );
    void onListActivate( wxCommandEvent& aEvent );
    void onCharHook( wxKeyEvent& aEvent );
    void onActivate( wxActivateEvent& aEvent );

    TOOL_MANAGER* m_toolMgr;
    bool          m_ready;     ///< True once shown; gates the close-on-deactivate.
    bool          m_dismissed; ///< Guards against dismissing/destroying twice.

    wxTextCtrl*           m_queryCtrl;
    COMMAND_PALETTE_LIST* m_resultsList;

    std::vector<COMMAND_PALETTE_ITEM>        m_items; ///< All eligible items (commands + nav).
    std::vector<const COMMAND_PALETTE_ITEM*> m_shown; ///< Displayed items, parallel to the rows.
    std::vector<wxString>                    m_mru;   ///< MRU keys, most-recent first.
};

#endif // DIALOG_COMMAND_PALETTE_H
