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

#ifndef JOB_PCB_EDIT_MOVE_FOOTPRINT_H
#define JOB_PCB_EDIT_MOVE_FOOTPRINT_H

#include <kicommon.h>
#include "job.h"

/// Headless board edit: move / rotate / flip a footprint by reference and write the board back.
class KICOMMON_API JOB_PCB_EDIT_MOVE_FOOTPRINT : public JOB
{
public:
    JOB_PCB_EDIT_MOVE_FOOTPRINT();
    wxString GetDefaultDescription() const override;
    wxString GetSettingsDialogTitle() const override;

    wxString m_filename;
    wxString m_ref;           ///< footprint reference (required)
    bool     m_hasAt = false; ///< whether to reposition
    double   m_x = 0.0;       ///< target X (mm) when m_hasAt
    double   m_y = 0.0;       ///< target Y (mm) when m_hasAt
    bool     m_hasRotate = false;
    double   m_rotateDeg = 0.0; ///< absolute orientation in degrees when m_hasRotate
    bool     m_flip = false;    ///< flip to the opposite board side
};

#endif
