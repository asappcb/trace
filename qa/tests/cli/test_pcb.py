#
# This program source code file is part of KiCad, a free EDA CAD application.
#
# Copyright (C) 2023 Mark Roszko <mark.roszko@gmail.com>
# Copyright (C) 2023 Roberto Fernandez Bautista <roberto.fer.bau@gmail.com>
# Copyright (C) 2023 KiCad Developers
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <https://www.gnu.org/licenses/>.
#

import utils
import json
from pathlib import Path
import pytest
import re
from typing import List, Tuple
from conftest import KiTestFixture
import sys


def get_generated_path(kitest: KiTestFixture,
                       input_file: Path,
                       test_name: str,
                       layer_name: str,
                       ) -> Tuple[Path, str]:
    layer_name_fixed = "-" + layer_name.replace( ".", "_" )
    generated_dir = str( kitest.get_output_path( "cli/{}/{}/".format( test_name, input_file.stem ) ) )
    generated_name = input_file.stem + layer_name_fixed + "-generated" + input_file.suffix
    generated_path = Path( generated_dir + "/" + generated_name )

    if generated_path.exists():
        generated_path.unlink()  # Delete file

    return [generated_path, layer_name_fixed]


def run_and_check_export_command(kitest: KiTestFixture,
                                 command: List[str],
                                 expected_output_file: Path):
    assert not expected_output_file.exists()

    stdout, stderr, exitcode = utils.run_and_capture( command )
    assert exitcode == 0
    # Don't assert stderr (legacy fills will have errors)
    assert stdout is not None
    assert expected_output_file.exists()

    kitest.add_attachment( expected_output_file )


@pytest.mark.parametrize("test_file,layers_to_test",
                         [
                            (
                                "cli/artwork_generation_regressions/ZoneFill-4.0.7.kicad_pcb",
                                ["F.Cu","B.Cu"]
                            ),
                            (   "cli/artwork_generation_regressions/ZoneFill-Legacy.brd",
                                ["F.Cu","B.Cu"]
                            )
                         ])
def test_pcb_export_svg( kitest: KiTestFixture,
                         test_file: str,
                         layers_to_test: List[str] ):

    input_file = kitest.get_data_file_path( test_file )

    for layer_name in layers_to_test:
        generated_svg_path, layer_name_fixed = get_generated_path( kitest,
                                                                   input_file.with_suffix( ".svg" ),
                                                                   "export_svg",
                                                                   layer_name )

        command = [utils.kicad_cli(), "pcb", "export", "svg", "--page-size-mode", "1",  # 1=Current page size
                   "--exclude-drawing-sheet", "--black-and-white", "--layers", layer_name,
                   "-o", str(generated_svg_path), str(input_file)]

        run_and_check_export_command( kitest, command, generated_svg_path )

        svg_source_path = str( input_file.with_suffix( "" ) )
        svg_source_path += layer_name_fixed + ".svg"

        # This test works only with Python >= 3.9 because it uses a pathlib function only existing
        # in 3.9 and newer. So skip it for previous versions
        if sys.hexversion >= 0x03090000 :
            # Comparison DPI = 850 => 1px == 30um. I.e. allowable error of 90 um after eroding
            assert utils.svgs_are_equivalent( str( generated_svg_path ), svg_source_path, 850,
                                              diff_handler=kitest.add_attachment )


@pytest.mark.parametrize("test_file,layers_to_test",
                         [
                            (
                                "cli/artwork_generation_regressions/ZoneFill-4.0.7.kicad_pcb",
                                ["F.Cu","B.Cu"]
                            ),
                            (   "cli/artwork_generation_regressions/ZoneFill-Legacy.brd",
                                ["F.Cu","B.Cu"]
                            )
                         ])
def test_pcb_export_png( kitest: KiTestFixture,
                         test_file: str,
                         layers_to_test: List[str] ):

    input_file = kitest.get_data_file_path( test_file )

    for layer_name in layers_to_test:
        output_dir = kitest.get_output_path( "cli/export_png/{}/{}/".format(
                input_file.stem, layer_name.replace( ".", "_" ) ) )

        if output_dir.exists():
            import shutil
            shutil.rmtree( output_dir )

        output_dir.mkdir( parents=True, exist_ok=True )

        command = [utils.kicad_cli(), "pcb", "export", "png", "--dpi", "300",
                   "--black-and-white", "--layers", layer_name,
                   "-o", str(output_dir), str(input_file)]

        stdout, stderr, exitcode = utils.run_and_capture( command )
        # Don't assert stderr (legacy fills will have errors)
        assert exitcode == 0
        assert stdout is not None

        png_files = list( output_dir.glob( "*.png" ) )
        assert len( png_files ) == 1, \
            "Expected 1 PNG file in output dir, found {}".format( len( png_files ) )

        generated_png_path = png_files[0]
        kitest.add_attachment( generated_png_path )

        # Verify the filename contains the board stem
        assert generated_png_path.stem.startswith( input_file.stem ), \
            "Unexpected output filename: {}".format( generated_png_path.name )

        assert not utils.image_is_blank( str( generated_png_path ) )

        # FIXME: add golden/reference PNG comparison using utils.images_are_equal()
        # for regression testing (see test_pcb_export_svg for the pattern)


@pytest.mark.skipif(not utils.is_gerbview_available(), reason="Requires gerbview kiface (kicad-cli gerber)")
@pytest.mark.parametrize("test_file,layers_to_test,max_diff_percent",
                         [
                            (
                                "cli/artwork_generation_regressions/ZoneFill-4.0.7.kicad_pcb",
                                ["F.Cu","B.Cu"],
                                0.5
                            ),
                            (   "cli/artwork_generation_regressions/ZoneFill-Legacy.brd",
                                ["F.Cu","B.Cu"],
                                0.5
                            ),
                            (
                                # Regression for https://gitlab.com/kicad/code/kicad/-/issues/24143:
                                # gr_poly shapes whose outline contains a degenerate
                                # near-zero-width "spike" must keep that spike (drawn as a thick
                                # stroke) when exported to gerber. Fracture/Simplify drops
                                # the spike from the fill, so the stroke must be plotted from the
                                # unfractured outline.
                                "cli/artwork_generation_regressions/Issue24143.kicad_pcb",
                                ["F.Cu"],
                                0.0
                            )
                         ])
def test_pcb_export_gerber( kitest: KiTestFixture,
                            test_file: str,
                            layers_to_test: List[str],
                            max_diff_percent: float ):

    input_file = kitest.get_data_file_path( test_file )

    for layer_name in layers_to_test:
        output_dir = kitest.get_output_path( "cli/export_gerber/{}/{}/".format(
                input_file.stem, layer_name.replace( ".", "_" ) ) )

        if output_dir.exists():
            import shutil
            shutil.rmtree( output_dir )

        output_dir.mkdir( parents=True, exist_ok=True )

        command = [utils.kicad_cli(), "pcb", "export", "gerbers", "--no-x2", "--use-drill-file-origin",
                   "--layers", layer_name,
                   "-o", str(output_dir), str(input_file)]

        stdout, stderr, exitcode = utils.run_and_capture( command )
        assert exitcode == 0
        assert stdout is not None

        gerber_files = [f for f in output_dir.iterdir() if not f.name.endswith( ".gbrjob" )]
        assert len( gerber_files ) == 1, \
            "Expected 1 gerber file in output dir, found {}".format( len( gerber_files ) )

        generated_gerber_path = gerber_files[0]
        kitest.add_attachment( generated_gerber_path )

        gbr_source_path = str( input_file.with_suffix( "" ) )
        layer_name_fixed = "-" + layer_name.replace( ".", "_" )
        gbr_source_path += layer_name_fixed + ".gbr"

        assert utils.gerbers_are_equivalent( str( generated_gerber_path ), gbr_source_path,
                                             diff_handler=kitest.add_attachment,
                                             max_diff_percent=max_diff_percent )


@pytest.mark.parametrize("test_file,golden_name,output_dir,skip_line_count,cli_args",
                         [
                            (
                                "cli/basic_test/basic_test.kicad_pcb",
                                "basic_test_excellon_default.drl",
                                "basic_test/drills/excellon_default/",
                                5,
                                ["--format","excellon"]
                            ),
                            (
                                "cli/basic_test/basic_test.kicad_pcb",
                                "basic_test_excellon_inches.drl",
                                "basic_test/drills/excellon_inches/",
                                5,
                                ["--format","excellon","-u","in"]
                            ),
                            (
                                "cli/basic_test/basic_test.kicad_pcb",
                                "basic_test_excellon_mirror.drl",
                                "basic_test/drills/excellon_mirror/",
                                5,
                                ["--format","excellon","--excellon-mirror-y"]
                            ),
                            (
                                "cli/basic_test/basic_test.kicad_pcb",
                                "basic_test-PTH-drl.gbr",
                                "basic_test/drills/gerber_default/",
                                9,
                                ["--format","gerber"]
                            )
                         ])
def test_pcb_export_drill( kitest: KiTestFixture,
                         test_file: str,
                         golden_name: str,
                         output_dir: str,
                         skip_line_count: int,
                         cli_args: List[str]  ):

    input_file = kitest.get_data_file_path( test_file )

    output_path =  kitest.get_output_path( "cli/{}/".format( output_dir ) )

    command = [utils.kicad_cli(), "pcb", "export", "drill"]
    command.extend( cli_args )
    command.append( "-o" )
    command.append( str( output_path ) )
    command.append( str( input_file ) )

    stdout, stderr, exitcode = utils.run_and_capture( command )

    print(stdout)

    assert exitcode == 0
    assert stdout is not None

    stdout_regex = re.search("Created file '(.+)'", stdout)
    assert stdout_regex

    output_drill_path = Path( stdout_regex.group(1) )
    assert output_drill_path.exists()

    kitest.add_attachment( output_drill_path )

    compare_filepath = kitest.get_data_file_path( "cli/basic_test/{}".format( golden_name ) )
    assert utils.textdiff_files( compare_filepath, output_drill_path, skip_line_count )


def test_pcb_optimize_swaps(kitest: KiTestFixture):
    """
    Run the headless gate-swap optimizer and confirm it writes a valid board and reports.
    """
    input_file = kitest.get_data_file_path("cli/basic_test/basic_test.kicad_pcb")
    output_path = kitest.get_output_path("cli/optimize_swaps/")
    Path(output_path).mkdir(parents=True, exist_ok=True)
    output_file = Path(output_path) / "optimized.kicad_pcb"

    if output_file.exists():
        output_file.unlink()

    command = [
        utils.kicad_cli(),
        "pcb",
        "optimize-swaps",
        str(input_file),
        "--output",
        str(output_file),
    ]

    stdout, stderr, exitcode = utils.run_and_capture(command)

    assert exitcode == 0
    assert output_file.exists()
    # A valid KiCad board was written.
    assert output_file.read_text().lstrip().startswith("(kicad_pcb")
    # The handler reports how many swaps it applied.
    assert "gate swap" in stdout.lower()


def test_pcb_ratsnest( kitest: KiTestFixture ):
    """`pcb ratsnest` reports unconnected connections per net.

    issue7086 is partially unrouted: 4 unconnected connections across two nets
    (Net-(J3-Pad4)=3, Net-(J3-Pad1)=1).
    """
    input_file = str( kitest.get_data_file_path( "pcbnew/issue7086.kicad_pcb" ) )

    # JSON goes to stdout as the sole content, so it must parse cleanly.
    stdout, _, exitcode = utils.run_and_capture(
            [utils.kicad_cli(), "pcb", "ratsnest", "--format", "json", input_file] )
    assert exitcode == 0    # default: a report, not a gate
    report = json.loads( stdout )
    assert report["unconnected"] == 4
    assert { n["net"]: n["unconnected"] for n in report["nets"] } == {
        "Net-(J3-Pad4)": 3, "Net-(J3-Pad1)": 1 }

    # --exit-code-violations turns "any net unconnected" into a nonzero exit for scripting.
    _, _, exitcode = utils.run_and_capture(
            [utils.kicad_cli(), "pcb", "ratsnest", "--exit-code-violations", input_file] )
    assert exitcode == 5

    # A fully-connected board reports zero and does not gate.
    connected = str( kitest.get_data_file_path( "pcbnew/complex_hierarchy.kicad_pcb" ) )
    stdout, _, exitcode = utils.run_and_capture(
            [utils.kicad_cli(), "pcb", "ratsnest", "--format", "json",
             "--exit-code-violations", connected] )
    assert exitcode == 0
    assert json.loads( stdout )["unconnected"] == 0


def test_pcb_export_json( kitest: KiTestFixture ):
    """`pcb export json` emits a structured board model: layers, nets, footprints (absolute
    position + rotation), pads (with net), tracks, vias, and zones (filled polygons), in mm."""
    input_file = str( kitest.get_data_file_path( "pcbnew/issue7086.kicad_pcb" ) )

    stdout, _, exitcode = utils.run_and_capture(
            [utils.kicad_cli(), "pcb", "export", "json", input_file] )
    assert exitcode == 0
    doc = json.loads( stdout )   # stdout must be pure JSON

    assert doc["units"] == "mm"
    assert set( doc ) >= { "layers", "nets", "footprints", "tracks", "vias", "zones" }

    layer_names = { l["name"] for l in doc["layers"] }
    assert { "F.Cu", "B.Cu" } <= layer_names

    # J1 is a 1x4 P2.54mm pin header on the front copper.
    j1 = next( fp for fp in doc["footprints"] if fp["ref"] == "J1" )
    assert j1["layer"] == "F.Cu"
    pads = sorted( j1["pads"], key=lambda p: p["number"] )
    assert len( pads ) == 4
    # Absolute board coordinates: adjacent pads are one 2.54 mm pitch apart.
    assert round( pads[1]["position"][0] - pads[0]["position"][0], 3 ) == 2.54
    assert all( p["net"] for p in pads )

    # At least one zone carries a filled polygon (a list of [x, y] rings).
    filled = [ z for z in doc["zones"] if z.get( "filled_polygons" ) ]
    assert filled
    ring = next( iter( filled[0]["filled_polygons"].values() ) )[0]
    assert len( ring ) >= 3 and len( ring[0] ) == 2
def _strip_zone_fill( text: str ) -> str:
    """Remove balanced (filled_polygon ...) blocks so the board loads but is unfilled."""
    out, i = [], 0
    while i < len( text ):
        j = text.find( "(filled_polygon", i )
        if j < 0:
            out.append( text[i:] ); break
        out.append( text[i:j] )
        depth, k = 0, j
        while k < len( text ):
            if text[k] == "(": depth += 1
            elif text[k] == ")":
                depth -= 1
                if depth == 0: k += 1; break
            k += 1
        i = k
    return "".join( out )


def test_pcb_zone_fill( kitest: KiTestFixture, tmp_path ):
    """`pcb zone fill` fills copper zones and writes a valid board back."""
    src = kitest.get_data_file_path( "pcbnew/issue7086.kicad_pcb" ).read_text()

    # Start from an unfilled copy so the test proves fill actually creates the pour, not a no-op.
    unfilled = tmp_path / "unfilled.kicad_pcb"
    unfilled.write_text( _strip_zone_fill( src ) )
    assert "filled_polygon" not in unfilled.read_text()

    out = tmp_path / "filled.kicad_pcb"
    _, _, exitcode = utils.run_and_capture(
            [utils.kicad_cli(), "pcb", "zone", "fill", str( unfilled ), "-o", str( out )] )
    assert exitcode == 0
    assert "filled_polygon" in out.read_text()   # 0 -> N: the pour was created

    # And the written board is valid (DRC can load it).
    _, _, drc_exit = utils.run_and_capture(
            [utils.kicad_cli(), "pcb", "drc", str( out )] )
    assert drc_exit in ( 0, 5 )   # 5 == DRC violations exist, still a successful load


def test_pcb_edit_set_track_width( kitest: KiTestFixture ):
    """`pcb edit set-track-width --net N --width W` sets the width of every track on the named net
    and writes the board back, leaving other nets untouched."""
    input_file = kitest.get_data_file_path( "pcbnew/issue12609.kicad_pcb" )
    output_dir = kitest.get_output_path( "cli/edit_set_track_width/" )
    output_dir.mkdir( parents=True, exist_ok=True )
    out_file = output_dir / "edited.kicad_pcb"

    command = [utils.kicad_cli(), "pcb", "edit", "set-track-width",
               "--net", "Net-(Q2-C)", "--width", "0.5",
               "-o", str( out_file ), str( input_file )]

    stdout, stderr, exitcode = utils.run_and_capture( command )
    assert exitcode == 0
    assert "Set width" in stdout
    assert out_file.exists()

    # Every track on the target net must now be 0.5 mm; at least one other net keeps a different
    # width (proving the --net filter, not a blanket change).
    txt = out_file.read_text()
    target_widths = set()
    other_has_non_half = False

    for blk in re.finditer( r'\((?:segment|arc)\b.*?\n\s*\)', txt, re.S ):
        b = blk.group( 0 )
        w = re.search( r'\(width ([0-9.]+)\)', b )
        n = re.search( r'\(net "([^"]*)"\)', b )

        if not w or not n:
            continue

        if n.group( 1 ) == "Net-(Q2-C)":
            target_widths.add( float( w.group( 1 ) ) )
        elif abs( float( w.group( 1 ) ) - 0.5 ) > 1e-6:
            other_has_non_half = True

    assert target_widths == { 0.5 }, "target net widths: {}".format( target_widths )
    assert other_has_non_half, "expected some other-net track to keep a non-0.5 width"


def test_pcb_edit_add_via( kitest: KiTestFixture ):
    """`pcb edit add-via --net N --at X,Y` drops one through via on the net at the given point and
    writes the board back."""
    input_file = kitest.get_data_file_path( "pcbnew/issue12609.kicad_pcb" )
    output_dir = kitest.get_output_path( "cli/edit_add_via/" )
    output_dir.mkdir( parents=True, exist_ok=True )
    out_file = output_dir / "withvia.kicad_pcb"

    # Count via items only ( \(via followed by space/newline/paren ), not "(viasonmask" in setup.
    def count_vias( s ):
        return len( re.findall( r'\(via[\s(]', s ) )

    vias_before = count_vias( input_file.read_text() )

    command = [utils.kicad_cli(), "pcb", "edit", "add-via",
               "--net", "GND", "--at", "150,95", "--size", "0.8", "--drill", "0.4",
               "-o", str( out_file ), str( input_file )]

    stdout, stderr, exitcode = utils.run_and_capture( command )
    assert exitcode == 0
    assert "Added a" in stdout and "via" in stdout
    assert out_file.exists()

    txt = out_file.read_text()
    # Exactly one via was added.
    assert count_vias( txt ) == vias_before + 1

    # A via block at (150, 95) carrying the requested size/drill, on the GND net.
    via_ok = False
    for blk in re.finditer( r'\(via\b.*?\n\s*\)', txt, re.S ):
        b = blk.group( 0 )
        if re.search( r'\(at 150 95\)', b ) and '(net "GND")' in b \
                and '(size 0.8)' in b and '(drill 0.4)' in b:
            via_ok = True

    assert via_ok, "expected a GND via at 150,95 with size 0.8 / drill 0.4"


def test_pcb_edit_add_via_bad_net( kitest: KiTestFixture ):
    """add-via fails cleanly on an unknown net rather than writing a bogus board."""
    input_file = kitest.get_data_file_path( "pcbnew/issue12609.kicad_pcb" )
    output_dir = kitest.get_output_path( "cli/edit_add_via_bad/" )
    output_dir.mkdir( parents=True, exist_ok=True )
    out_file = output_dir / "nope.kicad_pcb"

    command = [utils.kicad_cli(), "pcb", "edit", "add-via",
               "--net", "DOES_NOT_EXIST", "--at", "150,95",
               "-o", str( out_file ), str( input_file )]

    stdout, stderr, exitcode = utils.run_and_capture( command )
    assert exitcode != 0
    assert not out_file.exists()
