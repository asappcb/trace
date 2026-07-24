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

#ifndef JOB_EXPORT_PCB_JSON_H
#define JOB_EXPORT_PCB_JSON_H

#include <kicommon.h>
#include "job.h"

/**
 * Export a board's structure to a stable, machine-readable JSON model: layers, nets, footprints
 * (absolute position + rotation), pads (with net), tracks, vias, and zones (with filled polygons).
 *
 * Meant as the parse-free board model for tooling/agents -- so they read this instead of scraping
 * the s-expression file, whose layout drifts across format versions.  All geometry is in mm.
 */
class KICOMMON_API JOB_EXPORT_PCB_JSON : public JOB
{
public:
    JOB_EXPORT_PCB_JSON();

    wxString GetDefaultDescription() const override;
    wxString GetSettingsDialogTitle() const override;

    wxString m_filename;
};

#endif
