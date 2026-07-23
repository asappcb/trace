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

#include "command_pcb_ratsnest.h"
#include <cli/exit_codes.h>
#include "jobs/job_pcb_ratsnest.h"
#include <string_utils.h>
#include <wx/crt.h>

#define ARG_FORMAT "--format"
#define ARG_EXIT_CODE_VIOLATIONS "--exit-code-violations"

CLI::PCB_RATSNEST_COMMAND::PCB_RATSNEST_COMMAND() : COMMAND( "ratsnest" )
{
    // Optional output: written to a file when given, otherwise emitted on stdout.
    addCommonArgs( true, true, IO_TYPE::FILE, IO_TYPE::FILE );

    m_argParser.add_description(
            UTF8STDSTR( _( "Report the board's unconnected connections (ratsnest), in total and "
                          "per net" ) ) );

    m_argParser.add_argument( ARG_FORMAT )
            .default_value( std::string( "json" ) )
            .help( UTF8STDSTR( _( "Output format, options: json, report" ) ) )
            .metavar( "FORMAT" );

    m_argParser.add_argument( ARG_EXIT_CODE_VIOLATIONS )
            .help( UTF8STDSTR( _( "Return a nonzero exit code if any net is unconnected" ) ) )
            .flag();
}


int CLI::PCB_RATSNEST_COMMAND::doPerform( KIWAY& aKiway )
{
    std::unique_ptr<JOB_PCB_RATSNEST> ratsnestJob( new JOB_PCB_RATSNEST() );

    ratsnestJob->SetConfiguredOutputPath( m_argOutput );
    ratsnestJob->m_filename = m_argInput;
    ratsnestJob->m_exitCodeViolations = m_argParser.get<bool>( ARG_EXIT_CODE_VIOLATIONS );

    wxString format = From_UTF8( m_argParser.get<std::string>( ARG_FORMAT ).c_str() );

    if( format == wxS( "json" ) )
    {
        ratsnestJob->m_format = JOB_RC::OUTPUT_FORMAT::JSON;
    }
    else if( format == wxS( "report" ) )
    {
        ratsnestJob->m_format = JOB_RC::OUTPUT_FORMAT::REPORT;
    }
    else
    {
        wxFprintf( stderr, _( "Invalid report format\n" ) );
        return EXIT_CODES::ERR_ARGS;
    }

    return aKiway.ProcessJob( KIWAY::FACE_PCB, ratsnestJob.get() );
}
