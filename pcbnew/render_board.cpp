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

#include <render_board.h>

#include <board.h>
#include <jobs/job_export_pcb_svg.h>
#include <pcb_plot_params.h>
#include <pcb_plotter.h>
#include <reporter.h>

#include <wx/ffile.h>
#include <wx/filefn.h>
#include <wx/filename.h>


wxString RenderBoardToSvg( BOARD* aBoard, const LSEQ& aLayers, REPORTER& aReporter )
{
    if( !aBoard || aLayers.empty() )
        return wxEmptyString;

    // Configure SVG plot options from a single-file export job, as `kicad-cli pcb export svg` does.
    // A render query is meant to hand back a clean image of the *board* (for a viewer/thumbnail),
    // so default to cropping to the board and dropping the drawing sheet / title block -- unlike a
    // fabrication SVG, which keeps the page frame. (These become explicit options in a later PR.)
    JOB_EXPORT_PCB_SVG job;
    job.m_genMode = JOB_EXPORT_PCB_SVG::GEN_MODE::SINGLE;
    job.m_fitPageToBoard = true;
    job.m_plotDrawingSheet = false;

    PCB_PLOT_PARAMS plotOpts;
    PCB_PLOTTER::PlotJobToPlotOpts( plotOpts, &job, aReporter );

    // Plot to a temporary file, then read it back. A true in-memory target (open_memstream into
    // the plotter's FILE*) is a follow-up; the temp file keeps this PR a pure, testable core with
    // no PLOTTER changes.
    const wxString tempPath = wxFileName::CreateTempFileName( wxT( "kicad-render-" ) ) + wxT( ".svg" );

    PCB_PLOTTER           plotter( aBoard, &aReporter, plotOpts );
    std::vector<wxString> outputFiles;

    const bool ok = plotter.Plot( tempPath, aLayers, /*aCommonLayers*/ {},
                                  /*aUseGerberFileExtensions*/ false, /*aOutputPathIsSingle*/ true, std::nullopt,
                                  std::nullopt, std::nullopt, &outputFiles );

    // Plot may write to a slightly different name (layer suffix, etc.); the reported path is
    // authoritative.
    const wxString written = outputFiles.empty() ? tempPath : outputFiles.front();

    wxString svg;

    if( ok )
    {
        wxFFile file( written, wxT( "rb" ) );

        if( file.IsOpened() )
            file.ReadAll( &svg );
    }

    if( wxFileExists( written ) )
        wxRemoveFile( written );

    if( written != tempPath && wxFileExists( tempPath ) )
        wxRemoveFile( tempPath );

    return svg;
}
