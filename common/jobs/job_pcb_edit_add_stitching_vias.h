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

#ifndef JOB_PCB_EDIT_ADD_STITCHING_VIAS_H
#define JOB_PCB_EDIT_ADD_STITCHING_VIAS_H

#include <kicommon.h>
#include "job.h"

/**
 * Headless board edit: flood a net's copper zones with a grid of stitching vias and write the
 * board back -- the ground-stitching a fabricator or SI review wants, without opening pcbnew.
 */
class KICOMMON_API JOB_PCB_EDIT_ADD_STITCHING_VIAS : public JOB
{
public:
    JOB_PCB_EDIT_ADD_STITCHING_VIAS();

    wxString GetDefaultDescription() const override;
    wxString GetSettingsDialogTitle() const override;

    wxString m_filename;

    /// Net whose copper zones get stitched (required, by name).
    wxString m_net;

    /// Grid pitch between vias in millimetres (required, > 0).
    double m_spacingMM = 0.0;

    /// Via pad diameter in millimetres; <= 0 means "take it from the net's netclass".
    double m_sizeMM = 0.0;

    /// Via drill diameter in millimetres; <= 0 means "take it from the net's netclass".
    double m_drillMM = 0.0;
};

#endif
