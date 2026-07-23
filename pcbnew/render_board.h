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

#ifndef PCBNEW_RENDER_BOARD_H
#define PCBNEW_RENDER_BOARD_H

#include <wx/string.h>
#include <lseq.h>

class BOARD;
class REPORTER;

/**
 * Render @p aBoard to an SVG document and return it in memory (headless — no GUI, no output file
 * the caller has to manage).
 *
 * This is the groundwork for the "render query" (#117, KiCad-as-a-service): the same reusable core
 * a `kicad-cli pcb render` command and the IPC `RenderDocument` API can call to hand a design's
 * rendered bytes to a web viewer, CI, or an agent. It reuses the exact plot path that
 * `kicad-cli pcb export svg` uses (PCB_PLOTTER), so output matches the existing SVG export.
 *
 * @param aBoard    the board to render (must be non-null).
 * @param aLayers   the layers to plot, in order (must be non-empty).
 * @param aReporter receives any plot warnings/errors.
 * @return the SVG document, or an empty string on failure.
 */
wxString RenderBoardToSvg( BOARD* aBoard, const LSEQ& aLayers, REPORTER& aReporter );

#endif // PCBNEW_RENDER_BOARD_H
