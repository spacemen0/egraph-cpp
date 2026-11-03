#pragma once

#include "manifest.h"
#include <filesystem>

namespace egraph::frontend {

enum class ManifestFormat { Json, Text };

// Writes the supplied manifest without changing or lowering it. Replaces an existing file.
// JSON preserves typed expression trees and metadata; text is intended for inspection.
// Throws on serialization or file I/O failure. Parent directories must already exist.
void write_manifest(
    const ProgramManifest &manifest, const std::filesystem::path &path, ManifestFormat format = ManifestFormat::Json);

} // namespace egraph::frontend
