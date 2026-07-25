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

#include "command_pcb_edit_set_via_size.h"

#include <cli/exit_codes.h>
#include <jobs/job_pcb_edit_set_via_size.h>
#include <wx/crt.h>
#include <wx/file.h>

#include <string>

#define ARG_NET "--net"
#define ARG_SIZE "--size"
#define ARG_DRILL "--drill"

CLI::PCB_EDIT_SET_VIA_SIZE_COMMAND::PCB_EDIT_SET_VIA_SIZE_COMMAND() :
        COMMAND( "set-via-size" )
{
    addCommonArgs( true, true, IO_TYPE::FILE, IO_TYPE::FILE );

    m_argParser.add_description( UTF8STDSTR( _( "Set via diameter and/or drill (optionally on one net) and write the "
                                                "board back (overwrites the input when --output is omitted)" ) ) );

    m_argParser.add_argument( ARG_NET )
            .default_value( std::string() )
            .help( UTF8STDSTR( _( "Restrict the change to vias on this net; if omitted, all vias "
                                  "are changed" ) ) )
            .metavar( "NET" );

    m_argParser.add_argument( ARG_SIZE )
            .default_value( 0.0 )
            .scan<'g', double>()
            .help( UTF8STDSTR( _( "New via diameter in millimetres" ) ) )
            .metavar( "SIZE_MM" );

    m_argParser.add_argument( ARG_DRILL )
            .default_value( 0.0 )
            .scan<'g', double>()
            .help( UTF8STDSTR( _( "New via drill diameter in millimetres" ) ) )
            .metavar( "DRILL_MM" );
}


int CLI::PCB_EDIT_SET_VIA_SIZE_COMMAND::doPerform( KIWAY& aKiway )
{
    std::unique_ptr<JOB_PCB_EDIT_SET_VIA_SIZE> job( new JOB_PCB_EDIT_SET_VIA_SIZE() );

    job->m_filename = m_argInput;
    job->SetConfiguredOutputPath( m_argOutput );
    job->m_net = wxString::FromUTF8( m_argParser.get<std::string>( ARG_NET ).c_str() );
    job->m_sizeMM = m_argParser.get<double>( ARG_SIZE );
    job->m_drillMM = m_argParser.get<double>( ARG_DRILL );

    if( !wxFile::Exists( job->m_filename ) )
    {
        wxFprintf( stderr, _( "Board file does not exist or is not accessible\n" ) );
        return CLI::EXIT_CODES::ERR_INVALID_INPUT_FILE;
    }

    if( job->m_sizeMM <= 0.0 && job->m_drillMM <= 0.0 )
    {
        wxFprintf( stderr, _( "Pass at least one of --size or --drill (positive millimetres)\n" ) );
        return CLI::EXIT_CODES::ERR_ARGS;
    }

    return aKiway.ProcessJob( KIWAY::FACE_PCB, job.get() );
}
