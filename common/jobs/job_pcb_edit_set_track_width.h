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

#ifndef JOB_PCB_EDIT_SET_TRACK_WIDTH_H
#define JOB_PCB_EDIT_SET_TRACK_WIDTH_H

#include <kicommon.h>
#include "job.h"

/**
 * Headless board edit: set the width of copper tracks/arcs, optionally restricted to one net, and
 * write the board back. The API-level equivalent of selecting tracks in pcbnew and setting a width,
 * without opening the GUI or hand-editing s-expressions.
 */
class KICOMMON_API JOB_PCB_EDIT_SET_TRACK_WIDTH : public JOB
{
public:
    JOB_PCB_EDIT_SET_TRACK_WIDTH();

    wxString GetDefaultDescription() const override;
    wxString GetSettingsDialogTitle() const override;

    wxString m_filename;

    /// New track width in millimetres (> 0).
    double m_widthMM = 0.0;

    /// If non-empty, only tracks on this net are changed; otherwise every track is changed.
    wxString m_net;
};

#endif
