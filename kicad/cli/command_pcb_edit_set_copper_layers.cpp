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

#include "command_pcb_edit_set_copper_layers.h"

#include <cli/exit_codes.h>
#include <jobs/job_pcb_edit_set_copper_layers.h>
#include <wx/crt.h>
#include <wx/file.h>

#define ARG_COUNT "count"

CLI::PCB_EDIT_SET_COPPER_LAYERS_COMMAND::PCB_EDIT_SET_COPPER_LAYERS_COMMAND() :
        COMMAND( "set-copper-layers" )
{
    // Positional count first so usage reads `set-copper-layers 2 board.kicad_pcb`; addCommonArgs
    // then appends the input (and optional --output) positional/flag.
    m_argParser.add_argument( ARG_COUNT )
            .scan<'i', int>()
            .help( UTF8STDSTR( _( "Target number of copper layers (even, 2..32); reducing prunes inner-layer "
                                  "items" ) ) )
            .metavar( "COUNT" );

    addCommonArgs( true, true, IO_TYPE::FILE, IO_TYPE::FILE );

    m_argParser.add_description(
            UTF8STDSTR( _( "Set the number of copper layers and write the board back (overwrites the "
                           "input when --output is omitted)" ) ) );
}


int CLI::PCB_EDIT_SET_COPPER_LAYERS_COMMAND::doPerform( KIWAY& aKiway )
{
    std::unique_ptr<JOB_PCB_EDIT_SET_COPPER_LAYERS> job( new JOB_PCB_EDIT_SET_COPPER_LAYERS() );

    job->m_filename = m_argInput;
    job->SetConfiguredOutputPath( m_argOutput );
    job->m_copperLayerCount = m_argParser.get<int>( ARG_COUNT );

    if( !wxFile::Exists( job->m_filename ) )
    {
        wxFprintf( stderr, _( "Board file does not exist or is not accessible\n" ) );
        return CLI::EXIT_CODES::ERR_INVALID_INPUT_FILE;
    }

    if( job->m_copperLayerCount < 2 || job->m_copperLayerCount > 32 || ( job->m_copperLayerCount % 2 ) != 0 )
    {
        wxFprintf( stderr, _( "Copper layer count must be an even number between 2 and 32\n" ) );
        return CLI::EXIT_CODES::ERR_ARGS;
    }

    return aKiway.ProcessJob( KIWAY::FACE_PCB, job.get() );
}
