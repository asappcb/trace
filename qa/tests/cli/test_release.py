#
# This program source code file is part of KiCad, a free EDA CAD application.
#
# Copyright The KiCad Developers, see AUTHORS.txt for contributors.
#
# This program is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#

import json
import utils
from pathlib import Path


def _make_project(tmp: Path) -> Path:
    """A minimal project tree. `release create` only hashes these files, so dummy content is fine."""
    proj = tmp / "widget.kicad_pro"
    proj.write_text('{"meta": {"version": 1}}')
    (tmp / "widget.kicad_pcb").write_text("(kicad_pcb (version 20240101))")
    (tmp / "widget.kicad_sch").write_text("(kicad_sch (version 20240101))")
    return proj


def _make_outputs(tmp: Path) -> Path:
    outdir = tmp / "release"
    outdir.mkdir()
    (outdir / "top.gbr").write_text("G04 top layer*")
    (outdir / "drill").mkdir()
    (outdir / "drill" / "plated.drl").write_text("M48")
    return outdir


def test_release_create_and_verify(tmp_path):
    proj = _make_project(tmp_path)
    outdir = _make_outputs(tmp_path)

    # ---- create ----
    cmd = [utils.kicad_cli(), "release", "create", str(proj),
           "--output", str(outdir), "--version", "v1.0"]
    stdout, stderr, exitcode = utils.run_and_capture(cmd)
    assert exitcode == 0, stderr

    manifest = outdir / "v1.0.kicad_release"
    assert manifest.exists()

    data = json.loads(manifest.read_text())
    assert data["release_name"] == "v1.0"
    assert data["schema_version"] == 1
    assert "content_hash" in data

    input_paths = {e["path"] for e in data["inputs"]}
    output_paths = {e["path"] for e in data["outputs"]}
    assert {"widget.kicad_pro", "widget.kicad_pcb", "widget.kicad_sch"} <= input_paths
    assert "top.gbr" in output_paths
    assert "drill/plated.drl" in output_paths
    # The manifest must never fingerprint itself.
    assert "v1.0.kicad_release" not in output_paths

    # ---- verify: clean ----
    cmd = [utils.kicad_cli(), "release", "verify", str(manifest)]
    stdout, stderr, exitcode = utils.run_and_capture(cmd)
    assert exitcode == 0, stderr
    assert "verified" in stdout.lower()

    # ---- verify: drift detected after tampering an output ----
    (outdir / "top.gbr").write_text("G04 tampered*")
    cmd = [utils.kicad_cli(), "release", "verify", str(manifest)]
    stdout, stderr, exitcode = utils.run_and_capture(cmd)
    assert exitcode != 0
    assert "changed" in (stdout + stderr).lower()


def test_release_create_zip(tmp_path):
    proj = _make_project(tmp_path)
    outdir = _make_outputs(tmp_path)

    cmd = [utils.kicad_cli(), "release", "create", str(proj),
           "--output", str(outdir), "--zip"]
    stdout, stderr, exitcode = utils.run_and_capture(cmd)
    assert exitcode == 0, stderr

    # --zip bundles the output directory alongside it.
    assert (tmp_path / "release.zip").exists()
    # With no --version, the manifest is named after the project.
    assert (outdir / "widget.kicad_release").exists()


def test_release_create_requires_existing_output(tmp_path):
    proj = _make_project(tmp_path)

    # No --output at all.
    cmd = [utils.kicad_cli(), "release", "create", str(proj)]
    _, _, exitcode = utils.run_and_capture(cmd)
    assert exitcode != 0

    # --output pointing at a nonexistent directory.
    cmd = [utils.kicad_cli(), "release", "create", str(proj),
           "--output", str(tmp_path / "nope")]
    _, _, exitcode = utils.run_and_capture(cmd)
    assert exitcode != 0
