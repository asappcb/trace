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

#include "command_pcb_optimize_swaps.h"
#include "jobs/job_pcb_optimize_swaps.h"
#include "cli/exit_codes.h"

#include <wx/crt.h>
#include <wx/file.h>

#include <memory>

CLI::PCB_OPTIMIZE_SWAPS_COMMAND::PCB_OPTIMIZE_SWAPS_COMMAND() :
        COMMAND( "optimize-swaps" )
{
    addCommonArgs( true, true, IO_TYPE::FILE, IO_TYPE::FILE );
    m_argParser.add_description( UTF8STDSTR(
            _( "Reassign interchangeable gates across the board to shorten the ratsnest" ) ) );
}

int CLI::PCB_OPTIMIZE_SWAPS_COMMAND::doPerform( KIWAY& aKiway )
{
    std::unique_ptr<JOB_PCB_OPTIMIZE_SWAPS> job = std::make_unique<JOB_PCB_OPTIMIZE_SWAPS>();

    job->m_filename = m_argInput;
    job->m_outputFile = m_argOutput;

    if( !wxFile::Exists( job->m_filename ) )
    {
        wxFprintf( stderr, _( "Board file does not exist or is not accessible\n" ) );
        return EXIT_CODES::ERR_INVALID_INPUT_FILE;
    }

    // Require an explicit output: this rewrites net assignments (an optimization, not an idempotent
    // format bump), and the swap is not routability-validated, so never silently overwrite the
    // source. A caller who wants in-place can pass the same path.
    if( job->m_outputFile.IsEmpty() )
    {
        wxFprintf( stderr, _( "An output file is required (--output); refusing to overwrite the "
                              "input board in place.\n" ) );
        return EXIT_CODES::ERR_ARGS;
    }

    return aKiway.ProcessJob( KIWAY::FACE_PCB, job.get() );
}
