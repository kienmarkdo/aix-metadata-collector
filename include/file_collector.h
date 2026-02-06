/**
 * @file file_collector.h
 * @brief File metadata collector for AIX
 *
 * This collector retrieves comprehensive metadata for a file given its path.
 * It uses standard POSIX APIs that work on AIX:
 *   - stat64() for file attributes
 *   - readlink() for symbolic links
 *   - getpwuid()/getgrgid() for owner/group name resolution
 *   - AIX-specific extended attributes if available
 */

#ifndef AIX_METADATA_FILE_COLLECTOR_H
#define AIX_METADATA_FILE_COLLECTOR_H

#include "collector_base.h"
#include <sys/types.h>
#include <sys/stat.h>

namespace AixMetadata {

/**
 * @brief Collects metadata for a file on AIX
 *
 * Given a file path, this collector retrieves:
 *   - File type (regular, directory, symlink, device, etc.)
 *   - Size in bytes
 *   - Permissions (mode) in octal and symbolic notation
 *   - Owner UID and username
 *   - Group GID and group name
 *   - Access time, modification time, change time
 *   - Inode number
 *   - Device ID
 *   - Number of hard links
 *   - Symlink target (if applicable)
 *   - Whether file is readable/writable/executable by current user
 */
class FileCollector : public CollectorBase {
public:
    FileCollector() = default;
    ~FileCollector() override = default;

    /**
     * @brief Collect all available metadata for a file
     *
     * @param identifier The file path (e.g., "/etc/passwd")
     * @return MetadataResult containing all file metadata
     */
    MetadataResult collect(const std::string& identifier) override;

    QueryType getType() const override { return QueryType::File; }
    std::string getName() const override { return "FileCollector"; }

private:
    /**
     * @brief Collect basic file stats using stat64()
     * @param path File path
     * @param result Output: MetadataResult to populate
     * @return true if successful
     */
    bool collectStats(const std::string& path, MetadataResult& result);

    /**
     * @brief Collect symlink information if applicable
     * @param path File path
     * @param statBuf Stat buffer with file info
     * @param result Output: MetadataResult to populate
     */
    void collectSymlinkInfo(const std::string& path,
                            const struct stat64& statBuf,
                            MetadataResult& result);

    /**
     * @brief Collect owner and group information
     * @param statBuf Stat buffer with file info
     * @param result Output: MetadataResult to populate
     */
    void collectOwnership(const struct stat64& statBuf, MetadataResult& result);

    /**
     * @brief Collect access permissions for current user
     * @param path File path
     * @param result Output: MetadataResult to populate
     */
    void collectAccessInfo(const std::string& path, MetadataResult& result);

    /**
     * @brief Convert file type from mode to string
     * @param mode File mode from stat
     * @return Human-readable file type string
     */
    std::string fileTypeToString(mode_t mode);

    /**
     * @brief Convert mode to symbolic permission string (e.g., "rwxr-xr-x")
     * @param mode File mode from stat
     * @return Symbolic permission string
     */
    std::string modeToSymbolic(mode_t mode);

    /**
     * @brief Convert time value to ISO 8601 string
     * @param timeVal Time value (seconds since epoch)
     * @return ISO 8601 formatted string
     */
    std::string timeToString(time_t timeVal);
};

} // namespace AixMetadata

#endif // AIX_METADATA_FILE_COLLECTOR_H
