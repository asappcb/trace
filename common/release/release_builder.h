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

#ifndef KICAD_RELEASE_BUILDER_H
#define KICAD_RELEASE_BUILDER_H

#include <kicommon.h>
#include <release/release_manifest.h>

#include <vector>

#include <wx/string.h>

/**
 * Inputs to BuildReleaseManifest().
 *
 * VCS state and the timestamp are supplied by the caller rather than read here, so building a
 * manifest is a deterministic function of the files on disk (and thus straightforward to unit
 * test).
 */
struct KICOMMON_API RELEASE_BUILD_OPTIONS
{
    wxString m_projectFile;   ///< Path to the .kicad_pro; design inputs are resolved beside it.
    wxString m_outputDir;     ///< Directory of produced outputs to fingerprint (recursively).
    wxString m_releaseName;   ///< Release tag, e.g. "v1.2.0".
    wxString m_createdUtc;    ///< ISO-8601 UTC timestamp (injected).
    wxString m_kicadVersion;
    wxString m_vcsHash;       ///< Git commit hash, or "no hash".
    wxString m_vcsBranch;
    bool     m_vcsDirty = false;
};

/**
 * Populate @p aOut for a release:
 *  - hash the design inputs (the .kicad_pro, its sibling .kicad_pcb, and every .kicad_sch in the
 *    project directory), recording paths relative to the project directory;
 *  - hash every file under @p aOpts.m_outputDir recursively (except m_manifestFileName), recording
 *    paths relative to the output directory;
 *  - copy the metadata and VCS fields from @p aOpts.
 *
 * @return false with *aError set on a hard failure (missing project file or output directory).
 */
KICOMMON_API bool BuildReleaseManifest( RELEASE_MANIFEST& aOut, const RELEASE_BUILD_OPTIONS& aOpts,
                                        wxString* aError = nullptr );

/**
 * List files present under @p aDir (recursively, including hidden files, excluding any
 * `.kicad_release` manifest) that are NOT recorded as outputs in @p aManifest, as paths relative
 * to @p aDir. These are files that appeared after the release was stamped; verify surfaces them so
 * an added artifact does not go unnoticed even though it cannot make a recorded output drift.
 */
KICOMMON_API std::vector<wxString> FindUnexpectedOutputs( const RELEASE_MANIFEST& aManifest,
                                                          const wxString& aDir );

#endif // KICAD_RELEASE_BUILDER_H
