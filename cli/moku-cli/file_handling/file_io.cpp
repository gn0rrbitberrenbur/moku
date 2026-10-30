#include "file_handling/file_io.hpp"

#include <chrono>
#include <fstream>
#include <stdexcept>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace cli
{
/**
 * Writes a file atomically by first writing to a temporary file and then renaming it to the target
 * file.
 * @param file The path to the target file.
 * @param content The content to write to the file.
 * @param description A description of the file being written, used in error messages.
 * @throws std::runtime_error if the file cannot be written or renamed.
 * @throws std::filesystem::filesystem_error if the file cannot be renamed on non-Windows platforms.
 */
void writeFileAtomically(const std::filesystem::path& file, const std::string& content,
                         const std::string& description)
{
    if (!file.parent_path().empty())
    {
        std::filesystem::create_directories(file.parent_path());
    }
    auto temporary = file;
    temporary += ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            throw std::runtime_error("Cannot write " + description + ": " + temporary.u8string());
        }
        output << content;
        output.flush();
        if (!output)
        {
            throw std::runtime_error("Cannot save " + description + ": " + temporary.u8string());
        }
        output.close();
        if (output.fail())
        {
            throw std::runtime_error("Cannot close " + description + ": " + temporary.u8string());
        }
    }
#ifdef _WIN32
    DWORD error = ERROR_SUCCESS;
    for (int attempt = 0; attempt < 20; ++attempt)
    {
        if (MoveFileExW(temporary.c_str(), file.c_str(),
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            return;
        }
        error = GetLastError();
        if ((error != ERROR_SHARING_VIOLATION && error != ERROR_LOCK_VIOLATION &&
             error != ERROR_ACCESS_DENIED) ||
            attempt == 19)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    throw std::runtime_error("Cannot replace " + description + " (Windows error " +
                             std::to_string(error) + ").");
#else
    std::filesystem::rename(temporary, file);
#endif
}
} // namespace cli
