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

#ifndef COMMAND_PALETTE_ITEM_H
#define COMMAND_PALETTE_ITEM_H

#include <functional>
#include <wx/bmpbndl.h>
#include <wx/string.h>

/**
 * A single entry offered by the command palette. Both runnable commands (from the action registry)
 * and navigation targets (nets, footprints, sheets supplied by an editor) are expressed as items,
 * so the palette can rank and render them uniformly.
 *
 * Editors contribute their own navigation items by overriding
 * EDA_DRAW_FRAME::GetCommandPaletteItems().
 */
struct COMMAND_PALETTE_ITEM
{
    enum class CATEGORY
    {
        COMMAND,    ///< A runnable action.
        NAVIGATE    ///< A "go to" target (net, footprint, sheet, …).
    };

    wxString              m_name;       ///< Primary label and fuzzy-match target.
    wxString              m_detail;     ///< Subtle right-side qualifier (e.g. "net", "footprint").
    wxString              m_hotkey;     ///< Rendered shortcut, or empty.
    wxBitmapBundle        m_icon;       ///< Row icon, or empty bundle.
    bool                  m_enabled = true;
    CATEGORY              m_category = CATEGORY::COMMAND;

    /// Stable key for the recently-used list; empty means "do not remember" (e.g. navigation items,
    /// whose targets are board-specific and transient).
    wxString              m_mruKey;

    /// What to do when the user picks this item. Run on the frame's event loop after the palette
    /// popup has torn down.
    std::function<void()> m_run;
};

#endif // COMMAND_PALETTE_ITEM_H
