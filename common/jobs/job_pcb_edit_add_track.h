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

#ifndef JOB_PCB_EDIT_ADD_TRACK_H
#define JOB_PCB_EDIT_ADD_TRACK_H

#include <kicommon.h>
#include "job.h"

/// Headless board edit: add a straight copper track segment on a net and write the board back.
class KICOMMON_API JOB_PCB_EDIT_ADD_TRACK : public JOB
{
public:
    JOB_PCB_EDIT_ADD_TRACK();
    wxString GetDefaultDescription() const override;
    wxString GetSettingsDialogTitle() const override;

    wxString m_filename;
    wxString m_net;           ///< net name (required)
    wxString m_layer;         ///< copper layer name (required), e.g. F.Cu
    double   m_startX = 0.0;  ///< mm
    double   m_startY = 0.0;  ///< mm
    double   m_endX = 0.0;    ///< mm
    double   m_endY = 0.0;    ///< mm
    double   m_widthMM = 0.0; ///< <= 0 => take from the net's netclass
};

#endif
