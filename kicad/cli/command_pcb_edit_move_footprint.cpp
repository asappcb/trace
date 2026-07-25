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

#include "command_pcb_edit_move_footprint.h"

#include <cli/exit_codes.h>
#include <jobs/job_pcb_edit_move_footprint.h>
#include <cli/cli_xy.h>
#include <wx/crt.h>
#include <wx/file.h>

#include <string>

#define ARG_REF "--ref"
#define ARG_AT "--at"
#define ARG_ROTATE "--rotate"
#define ARG_FLIP "--flip"

CLI::PCB_EDIT_MOVE_FOOTPRINT_COMMAND::PCB_EDIT_MOVE_FOOTPRINT_COMMAND() :
        COMMAND( "move-footprint" )
{
    addCommonArgs( true, true, IO_TYPE::FILE, IO_TYPE::FILE );

    m_argParser.add_description( UTF8STDSTR( _( "Move, rotate and/or flip a footprint and write the board back "
                                                "(overwrites the input when --output is omitted)" ) ) );

    m_argParser.add_argument( ARG_REF )
            .default_value( std::string() )
            .help( UTF8STDSTR( _( "Reference of the footprint to move (required)" ) ) )
            .metavar( "REF" );

    m_argParser.add_argument( ARG_AT )
            .default_value( std::string() )
            .help( UTF8STDSTR( _( "New position as X,Y in millimetres" ) ) )
            .metavar( "X,Y" );

    m_argParser.add_argument( ARG_ROTATE )
            .default_value( std::string() )
            .help( UTF8STDSTR( _( "Absolute orientation in degrees" ) ) )
            .metavar( "DEG" );

    m_argParser.add_argument( ARG_FLIP )
            .help( UTF8STDSTR( _( "Flip the footprint to the opposite board side" ) ) )
            .flag();
}


int CLI::PCB_EDIT_MOVE_FOOTPRINT_COMMAND::doPerform( KIWAY& aKiway )
{
    std::unique_ptr<JOB_PCB_EDIT_MOVE_FOOTPRINT> job( new JOB_PCB_EDIT_MOVE_FOOTPRINT() );

    job->m_filename = m_argInput;
    job->SetConfiguredOutputPath( m_argOutput );
    job->m_ref = wxString::FromUTF8( m_argParser.get<std::string>( ARG_REF ).c_str() );
    job->m_flip = m_argParser.get<bool>( ARG_FLIP );

    if( !wxFile::Exists( job->m_filename ) )
    {
        wxFprintf( stderr, _( "Board file does not exist or is not accessible\n" ) );
        return CLI::EXIT_CODES::ERR_INVALID_INPUT_FILE;
    }

    if( job->m_ref.IsEmpty() )
    {
        wxFprintf( stderr, _( "--ref is required\n" ) );
        return CLI::EXIT_CODES::ERR_ARGS;
    }

    std::string at = m_argParser.get<std::string>( ARG_AT );

    if( !at.empty() )
    {
        if( !CLI::ParseXY( at, job->m_x, job->m_y ) )
        {
            wxFprintf( stderr, _( "--at must be given as X,Y in millimetres, e.g. 25.4,10\n" ) );
            return CLI::EXIT_CODES::ERR_ARGS;
        }

        job->m_hasAt = true;
    }

    std::string rot = m_argParser.get<std::string>( ARG_ROTATE );

    if( !rot.empty() )
    {
        try
        {
            std::size_t used = 0;
            job->m_rotateDeg = std::stod( rot, &used );

            if( used != rot.size() )
                throw std::invalid_argument( "trailing characters" );
        }
        catch( ... )
        {
            wxFprintf( stderr, _( "--rotate must be a number of degrees\n" ) );
            return CLI::EXIT_CODES::ERR_ARGS;
        }

        job->m_hasRotate = true;
    }

    if( !job->m_hasAt && !job->m_hasRotate && !job->m_flip )
    {
        wxFprintf( stderr, _( "Nothing to do: pass at least one of --at, --rotate or --flip\n" ) );
        return CLI::EXIT_CODES::ERR_ARGS;
    }

    return aKiway.ProcessJob( KIWAY::FACE_PCB, job.get() );
}
