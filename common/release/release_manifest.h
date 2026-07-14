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

#ifndef KICAD_RELEASE_MANIFEST_H
#define KICAD_RELEASE_MANIFEST_H

#include <kicommon.h>
#include <wx/string.h>

#include <cstdint>
#include <vector>

/**
 * One hashed file recorded in a release manifest.
 *
 * The path is stored relative to the release/project root, using POSIX ('/') separators so a
 * manifest produced on one platform verifies on another.
 */
struct KICOMMON_API RELEASE_FILE_ENTRY
{
    wxString m_path;      ///< Relative path with '/' separators.
    wxString m_sha256;    ///< 64-char lowercase hex SHA-256 of the file bytes.
    uint64_t m_bytes = 0; ///< File size in bytes.
};


/**
 * An immutable, revision-stamped record of a design-output run ("release").
 *
 * A release captures which git revision and exactly which input bytes produced exactly which
 * output bytes, and when. It is the audit / immutability layer over a jobset run: the jobset
 * produces the outputs, and the manifest fingerprints them so a later verify can prove nothing
 * drifted.
 *
 * The type is deliberately a plain data model with explicit JSON (de)serialization and no
 * dependency on the jobset/runner machinery, so it is trivially unit-testable in isolation.
 */
class KICOMMON_API RELEASE_MANIFEST
{
public:
    RELEASE_MANIFEST() = default;

    // ---- metadata ---------------------------------------------------------------------------
    int      m_schemaVersion = 1;
    wxString m_releaseName;    ///< User-supplied release tag, e.g. "v1.2.0".
    wxString m_createdUtc;     ///< ISO-8601 UTC timestamp; injected by the caller (deterministic
                               ///< for tests).
    wxString m_projectName;
    wxString m_jobsetName;
    wxString m_kicadVersion;

    // ---- provenance -------------------------------------------------------------------------
    wxString m_vcsHash;        ///< Full git commit hash, or "no hash" when unavailable.
    wxString m_vcsBranch;
    bool     m_vcsDirty = false; ///< True if the working tree had uncommitted changes at release.

    // ---- contents ---------------------------------------------------------------------------
    std::vector<RELEASE_FILE_ENTRY> m_inputs;
    std::vector<RELEASE_FILE_ENTRY> m_outputs;

    void AddInput( const wxString& aRelPath, const wxString& aSha256, uint64_t aBytes );
    void AddOutput( const wxString& aRelPath, const wxString& aSha256, uint64_t aBytes );

    /**
     * Deterministic content fingerprint over every input and output entry.
     *
     * Computed from the sorted (kind, path, sha, bytes) tuples, so it is independent of the
     * order in which entries were added. This is the release's content identity; two runs that
     * produced byte-identical inputs and outputs share it.
     *
     * @return 64-char lowercase hex SHA-256.
     */
    wxString ComputeContentHash() const;

    // ---- serialization ----------------------------------------------------------------------
    /// Serialize to pretty-printed JSON (includes the freshly computed content hash).
    wxString Format() const;

    bool WriteToFile( const wxString& aPath, wxString* aError = nullptr ) const;

    static bool Parse( const wxString& aJson, RELEASE_MANIFEST& aOut, wxString* aError = nullptr );
    static bool LoadFromFile( const wxString& aPath, RELEASE_MANIFEST& aOut,
                              wxString* aError = nullptr );

    /// Content hash declared in the file this manifest was parsed from (empty if constructed
    /// in-memory). Used by verification to detect edits to the entry list.
    const wxString& DeclaredContentHash() const { return m_declaredContentHash; }

    // ---- verification / audit ---------------------------------------------------------------
    struct VERIFY_ENTRY
    {
        enum STATUS
        {
            OK,         ///< File present and SHA matches.
            CHANGED,    ///< File present but SHA differs.
            MISSING,    ///< File not found under the base directory.
            UNREADABLE  ///< File present but could not be opened/read to hash.
        };

        wxString m_path;
        STATUS   m_status = OK;
    };

    struct VERIFY_RESULT
    {
        std::vector<VERIFY_ENTRY> m_entries;
        bool m_contentHashOk = true; ///< Declared content hash matches the entries (no manifest
                                     ///< tampering). True when no hash was declared.

        bool Ok() const;             ///< All outputs OK and the content hash is consistent.
    };

    /**
     * Recompute the SHA-256 of every recorded output against the files found under @p aBaseDir
     * and report any drift, plus whether the manifest's own declared content hash still matches
     * its entries.
     */
    VERIFY_RESULT VerifyOutputs( const wxString& aBaseDir ) const;

private:
    wxString m_declaredContentHash;
};


/**
 * Compute the SHA-256 of a file's raw bytes.
 *
 * @param aPath  file to hash.
 * @param aOk    optional; set to false if the file could not be opened/read.
 * @param aBytes optional; set to the number of bytes hashed.
 * @return 64-char lowercase hex digest, or an empty string on failure.
 */
KICOMMON_API wxString ReleaseHashFile( const wxString& aPath, bool* aOk = nullptr,
                                       uint64_t* aBytes = nullptr );

#endif // KICAD_RELEASE_MANIFEST_H
