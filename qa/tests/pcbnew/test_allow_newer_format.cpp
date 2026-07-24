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

/**
 * @file test_allow_newer_format.cpp
 * Tests PCB_IO_KICAD_SEXPR_PARSER::SetAllowNewerFormat() — the loader side of kicad-cli's
 * --allow-newer-format (#147). A board whose format-version integer is newer than this build
 * supports, but which uses only known tokens, must hard-refuse by default and load (with a
 * warning) when the flag is set.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>
#include <pcbnew_utils/board_file_utils.h>

#include <pcb_io/kicad_sexpr/pcb_io_kicad_sexpr.h>
#include <pcb_io/kicad_sexpr/pcb_io_kicad_sexpr_parser.h>
#include <board.h>
#include <board_item.h>
#include <ki_exception.h>
#include <richio.h>

#include <fstream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>


BOOST_AUTO_TEST_SUITE( AllowNewerFormat )


/// Read a real, current board and bump only its (version N) integer past what this build supports,
/// so the file reads as "too recent" while still using exclusively known tokens.
static std::string readBumpedBoard()
{
    std::string   path = KI_TEST::GetPcbnewTestDataDir() + "issue7086.kicad_pcb";
    std::ifstream in( path );
    BOOST_REQUIRE_MESSAGE( in.is_open(), "cannot open test board: " << path );

    std::stringstream ss;
    ss << in.rdbuf();
    std::string board = ss.str();

    std::string future = std::to_string( SEXPR_BOARD_FILE_VERSION + 1 );
    std::regex  versionRe( R"(\(version\s+\d+\))" );
    std::string bumped = std::regex_replace( board, versionRe, "(version " + future + ")",
                                             std::regex_constants::format_first_only );

    BOOST_REQUIRE_MESSAGE( bumped != board, "failed to bump the (version ...) field" );
    return bumped;
}


BOOST_AUTO_TEST_CASE( TooRecentRefusedByDefault )
{
    std::string               board = readBumpedBoard();
    STRING_LINE_READER        reader( board, wxT( "bumped board" ) );
    PCB_IO_KICAD_SEXPR_PARSER parser( &reader, nullptr, nullptr );

    // FUTURE_FORMAT_ERROR derives from PARSE_ERROR derives from IO_ERROR.
    BOOST_CHECK_THROW( parser.Parse(), IO_ERROR );
    BOOST_CHECK( parser.IsTooRecent() );
}


BOOST_AUTO_TEST_CASE( TooRecentLoadsWithAllowNewerFormat )
{
    std::string               board = readBumpedBoard();
    STRING_LINE_READER        reader( board, wxT( "bumped board" ) );
    PCB_IO_KICAD_SEXPR_PARSER parser( &reader, nullptr, nullptr );

    parser.SetAllowNewerFormat( true );

    std::unique_ptr<BOARD_ITEM> item;
    BOOST_CHECK_NO_THROW( item.reset( parser.Parse() ) );
    BOOST_REQUIRE( item );
    BOOST_CHECK( dynamic_cast<BOARD*>( item.get() ) != nullptr );

    // The newer format must not be accepted silently: a one-time warning is recorded so the CLI
    // (and any GUI caller) can surface it.
    const std::vector<wxString>& warnings = parser.GetParseWarnings();
    BOOST_CHECK( !warnings.empty() );
}


BOOST_AUTO_TEST_SUITE_END()
