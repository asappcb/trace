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

#include <jobs/job_pcb_ratsnest.h>
#include <i18n_utility.h>

JOB_PCB_RATSNEST::JOB_PCB_RATSNEST() :
        JOB_RC( "ratsnest" )
{
    // A connectivity report has no violations to grade by severity, so unlike DRC the CLI does
    // not gate the exit code on it by default; --exit-code-violations opts in.
    m_exitCodeViolations = false;
}


wxString JOB_PCB_RATSNEST::GetDefaultDescription() const
{
    return _( "Report unconnected connections (ratsnest)" );
}


wxString JOB_PCB_RATSNEST::GetSettingsDialogTitle() const
{
    return _( "Ratsnest Report Job Settings" );
}
