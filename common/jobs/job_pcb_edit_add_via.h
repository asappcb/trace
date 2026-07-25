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

#ifndef JOB_PCB_EDIT_ADD_VIA_H
#define JOB_PCB_EDIT_ADD_VIA_H

#include <kicommon.h>
#include "job.h"

/**
 * Headless board edit: drop a through via on a net at a given point and write the board back. The
 * API-level equivalent of placing a via in pcbnew, without opening the GUI or hand-editing
 * s-expressions.
 */
class KICOMMON_API JOB_PCB_EDIT_ADD_VIA : public JOB
{
public:
    JOB_PCB_EDIT_ADD_VIA();

    wxString GetDefaultDescription() const override;
    wxString GetSettingsDialogTitle() const override;

    wxString m_filename;

    /// Net the via connects to (required, by name).
    wxString m_net;

    /// Via centre in millimetres.
    double m_x = 0.0;
    double m_y = 0.0;

    /// Via pad diameter in millimetres; <= 0 means "take it from the net's netclass".
    double m_sizeMM = 0.0;

    /// Via drill diameter in millimetres; <= 0 means "take it from the net's netclass".
    double m_drillMM = 0.0;
};

#endif
