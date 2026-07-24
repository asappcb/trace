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

#include <jobs/job_export_pcb_json.h>
#include <i18n_utility.h>

JOB_EXPORT_PCB_JSON::JOB_EXPORT_PCB_JSON() :
        JOB( "json", false ),
        m_filename()
{
}


wxString JOB_EXPORT_PCB_JSON::GetDefaultDescription() const
{
    return _( "Export board model as JSON" );
}


wxString JOB_EXPORT_PCB_JSON::GetSettingsDialogTitle() const
{
    return _( "Export JSON Job Settings" );
}
