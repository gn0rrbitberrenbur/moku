#pragma once

#include <filesystem>
#include <string>

namespace cli
{
/**
 * Writes a file through a sibling temporary file and atomically replaces the destination.
 * Missing parent directories are created.
 *
 * @param file destination path
 * @param content complete file content
 * @param description human-readable file description used in error messages
 */
void writeFileAtomically(const std::filesystem::path& file, const std::string& content,
                         const std::string& description);
}
