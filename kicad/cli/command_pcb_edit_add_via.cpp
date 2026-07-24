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

#include "command_pcb_edit_add_via.h"

#include <cli/exit_codes.h>
#include <jobs/job_pcb_edit_add_via.h>
#include <wx/crt.h>
#include <wx/file.h>

#include <cstdlib>
#include <string>

#define ARG_NET "--net"
#define ARG_AT "--at"
#define ARG_SIZE "--size"
#define ARG_DRILL "--drill"

CLI::PCB_EDIT_ADD_VIA_COMMAND::PCB_EDIT_ADD_VIA_COMMAND() : COMMAND( "add-via" )
{
    // --output optional: when omitted the edited board is written back over the input.
    addCommonArgs( true, true, IO_TYPE::FILE, IO_TYPE::FILE );

    m_argParser.add_description(
            UTF8STDSTR( _( "Add a through via on a net at a point and write the board back "
                           "(overwrites the input when --output is omitted)" ) ) );

    m_argParser.add_argument( ARG_NET )
            .default_value( std::string() )
            .help( UTF8STDSTR( _( "Net the via connects to (required)" ) ) )
            .metavar( "NET" );

    m_argParser.add_argument( ARG_AT )
            .default_value( std::string() )
            .help( UTF8STDSTR( _( "Via centre as X,Y in millimetres (required), e.g. 25.4,10" ) ) )
            .metavar( "X,Y" );

    m_argParser.add_argument( ARG_SIZE )
            .default_value( 0.0 )
            .scan<'g', double>()
            .help( UTF8STDSTR( _( "Via pad diameter in mm; defaults to the net's netclass" ) ) )
            .metavar( "SIZE_MM" );

    m_argParser.add_argument( ARG_DRILL )
            .default_value( 0.0 )
            .scan<'g', double>()
            .help( UTF8STDSTR( _( "Via drill diameter in mm; defaults to the net's netclass" ) ) )
            .metavar( "DRILL_MM" );
}


int CLI::PCB_EDIT_ADD_VIA_COMMAND::doPerform( KIWAY& aKiway )
{
    std::unique_ptr<JOB_PCB_EDIT_ADD_VIA> job( new JOB_PCB_EDIT_ADD_VIA() );

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

    if( job->m_net.IsEmpty() )
    {
        wxFprintf( stderr, _( "--net is required\n" ) );
        return CLI::EXIT_CODES::ERR_ARGS;
    }

    // Parse "X,Y" (millimetres). Kept deliberately strict so a typo fails loudly rather than
    // silently dropping a via at the origin.
    std::string at = m_argParser.get<std::string>( ARG_AT );
    std::size_t comma = at.find( ',' );

    if( comma == std::string::npos )
    {
        wxFprintf( stderr, _( "--at must be given as X,Y in millimetres, e.g. 25.4,10\n" ) );
        return CLI::EXIT_CODES::ERR_ARGS;
    }

    try
    {
        std::size_t xEnd = 0, yEnd = 0;
        job->m_x = std::stod( at.substr( 0, comma ), &xEnd );
        job->m_y = std::stod( at.substr( comma + 1 ), &yEnd );

        // Reject trailing junk ("1,2x") that stod would otherwise silently accept.
        if( xEnd != at.substr( 0, comma ).size() || yEnd != at.substr( comma + 1 ).size() )
            throw std::invalid_argument( "trailing characters" );
    }
    catch( ... )
    {
        wxFprintf( stderr, _( "--at must be two numbers X,Y in millimetres, e.g. 25.4,10\n" ) );
        return CLI::EXIT_CODES::ERR_ARGS;
    }

    return aKiway.ProcessJob( KIWAY::FACE_PCB, job.get() );
}
