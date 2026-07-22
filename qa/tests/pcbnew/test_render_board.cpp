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
 * @file test_render_board.cpp
 * Tests for the headless board->SVG render core (#117 render-query groundwork). Confirms that a
 * loaded board renders to a non-empty, well-formed SVG document in memory, and that the guards
 * hold for a null board / empty layer set.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>
#include <pcbnew_utils/board_test_utils.h>

#include <board.h>
#include <layer_ids.h>
#include <lseq.h>
#include <reporter.h>
#include <settings/settings_manager.h>

#include "../../../pcbnew/render_board.h"


struct RENDER_BOARD_FIXTURE
{
    RENDER_BOARD_FIXTURE()
    {
        KI_TEST::LoadBoard( m_settings, "complex_hierarchy", m_board );
        BOOST_REQUIRE( m_board );
    }

    SETTINGS_MANAGER       m_settings;
    std::unique_ptr<BOARD> m_board;
};


BOOST_FIXTURE_TEST_SUITE( RenderBoard, RENDER_BOARD_FIXTURE )


BOOST_AUTO_TEST_CASE( RendersNonEmptyWellFormedSvg )
{
    REPORTER& reporter = NULL_REPORTER::GetInstance();

    wxString svg = RenderBoardToSvg( m_board.get(), { F_Cu, Edge_Cuts }, reporter );

    BOOST_CHECK( !svg.IsEmpty() );
    BOOST_CHECK_MESSAGE( svg.Contains( wxT( "<svg" ) ), "render output should be an SVG document" );
    BOOST_CHECK( svg.Contains( wxT( "</svg>" ) ) );
}


BOOST_AUTO_TEST_CASE( GuardsNullBoardAndEmptyLayers )
{
    REPORTER& reporter = NULL_REPORTER::GetInstance();

    BOOST_CHECK( RenderBoardToSvg( nullptr, { F_Cu }, reporter ).IsEmpty() );
    BOOST_CHECK( RenderBoardToSvg( m_board.get(), {}, reporter ).IsEmpty() );
}


BOOST_AUTO_TEST_SUITE_END()
