/**
 * @file file_collector.cpp
 * @brief Implementation of file metadata collector for AIX
 *
 * This file implements the FileCollector class which uses POSIX APIs
 * to retrieve comprehensive file metadata. The implementation is
 * portable across UNIX systems including AIX.
 *
 * APIs used:
 *   - stat64(): File attributes (size, permissions, times, etc.)
 *   - lstat64(): Symlink attributes
 *   - readlink(): Symlink target resolution
 *   - access(): Check current user's access permissions
 *   - getpwuid()/getgrgid(): Owner/group name resolution
 */

#include "file_collector.h"

#include <cstring>
#include <cerrno>
#include <sstream>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

// Include header for major()/minor() macros - location varies by platform
// On AIX and most UNIX systems, these are in sys/sysmacros.h
// On some systems they may be in sys/mkdev.h or sys/types.h
#ifdef _AIX
#include <sys/sysmacros.h>
#include <sys/statfs.h>
#else
// For other platforms, try sysmacros.h first
#if __has_include(<sys/sysmacros.h>)
#include <sys/sysmacros.h>
#elif __has_include(<sys/mkdev.h>)
#include <sys/mkdev.h>
#endif
#endif

namespace AixMetadata {

MetadataResult FileCollector::collect(const std::string& identifier) {
    MetadataResult result;
    result.type = "file";
    result.identifier = identifier;

    if (identifier.empty()) {
        return createErrorResult(identifier, "Empty file path");
    }

    // Collect file statistics
    if (!collectStats(identifier, result)) {
        return result;  // Error already set in collectStats
    }

    result.success = true;

    // Collect access information for current user
    collectAccessInfo(identifier, result);

    return result;
}

bool FileCollector::collectStats(const std::string& path, MetadataResult& result) {
    struct stat64 statBuf;
    struct stat64 lstatBuf;

    // First, use lstat64 to get info about the path itself (not following symlinks)
    if (lstat64(path.c_str(), &lstatBuf) != 0) {
        int err = errno;
        std::ostringstream errMsg;
        errMsg << "Cannot stat file '" << path << "': " << strerror(err);
        result.success = false;
        result.errorMessage = errMsg.str();
        return false;
    }

    // Check if it's a symlink
    bool isSymlink = S_ISLNK(lstatBuf.st_mode);

    // If it's a symlink, also stat the target
    if (isSymlink) {
        if (stat64(path.c_str(), &statBuf) != 0) {
            // Symlink target doesn't exist or is inaccessible
            // We still have lstat info, so we can report partial data
            result.addAttribute("symlink_broken", "true");
            // Use lstat data for the rest
            memcpy(&statBuf, &lstatBuf, sizeof(statBuf));
        }
        collectSymlinkInfo(path, lstatBuf, result);
    } else {
        memcpy(&statBuf, &lstatBuf, sizeof(statBuf));
    }

    // File type
    result.addAttribute("type", fileTypeToString(lstatBuf.st_mode));

    // Size (in bytes)
    result.addAttribute("size", static_cast<uint64_t>(statBuf.st_size));

    // Device ID
    result.addAttribute("device", static_cast<uint64_t>(statBuf.st_dev));

    // Inode number
    result.addAttribute("inode", static_cast<uint64_t>(statBuf.st_ino));

    // Number of hard links
    result.addAttribute("nlink", static_cast<uint64_t>(statBuf.st_nlink));

    // Permissions - octal format
    std::ostringstream modeOctal;
    modeOctal << "0" << std::oct << (statBuf.st_mode & 07777);
    result.addAttribute("mode_octal", modeOctal.str());

    // Permissions - symbolic format
    result.addAttribute("mode_symbolic", modeToSymbolic(statBuf.st_mode));

    // Special bits
    if (statBuf.st_mode & S_ISUID) {
        result.addAttribute("setuid", "true");
    }
    if (statBuf.st_mode & S_ISGID) {
        result.addAttribute("setgid", "true");
    }
    if (statBuf.st_mode & S_ISVTX) {
        result.addAttribute("sticky", "true");
    }

    // Owner and group information
    collectOwnership(statBuf, result);

    // Timestamps
    result.addAttribute("access_time", timeToString(statBuf.st_atime));
    result.addAttribute("modify_time", timeToString(statBuf.st_mtime));
    result.addAttribute("change_time", timeToString(statBuf.st_ctime));

    // Epoch timestamps (for programmatic use)
    result.addAttribute("atime_epoch", static_cast<int64_t>(statBuf.st_atime));
    result.addAttribute("mtime_epoch", static_cast<int64_t>(statBuf.st_mtime));
    result.addAttribute("ctime_epoch", static_cast<int64_t>(statBuf.st_ctime));

    // Block size and blocks used
    result.addAttribute("block_size", static_cast<int64_t>(statBuf.st_blksize));
    result.addAttribute("blocks", static_cast<int64_t>(statBuf.st_blocks));

    // For device files, report major/minor numbers
    if (S_ISBLK(statBuf.st_mode) || S_ISCHR(statBuf.st_mode)) {
        result.addAttribute("rdev_major", static_cast<int64_t>(major(statBuf.st_rdev)));
        result.addAttribute("rdev_minor", static_cast<int64_t>(minor(statBuf.st_rdev)));
    }

    return true;
}

void FileCollector::collectSymlinkInfo(const std::string& path,
                                        const struct stat64& /* statBuf */,
                                        MetadataResult& result) {
    result.addAttribute("is_symlink", "true");

    // Read the symlink target
    char linkTarget[PATH_MAX];
    ssize_t len = readlink(path.c_str(), linkTarget, sizeof(linkTarget) - 1);

    if (len > 0) {
        linkTarget[len] = '\0';
        result.addAttribute("symlink_target", std::string(linkTarget));

        // Check if target path is absolute or relative
        if (linkTarget[0] == '/') {
            result.addAttribute("symlink_type", "absolute");
        } else {
            result.addAttribute("symlink_type", "relative");
        }
    } else {
        result.addAttribute("symlink_target", "unreadable");
    }
}

void FileCollector::collectOwnership(const struct stat64& statBuf, MetadataResult& result) {
    // User ID
    result.addAttribute("uid", static_cast<int64_t>(statBuf.st_uid));

    // Resolve username
    struct passwd* pwd = getpwuid(statBuf.st_uid);
    if (pwd != nullptr) {
        result.addAttribute("owner", std::string(pwd->pw_name));
    } else {
        result.addAttribute("owner", "unknown");
    }

    // Group ID
    result.addAttribute("gid", static_cast<int64_t>(statBuf.st_gid));

    // Resolve group name
    struct group* grp = getgrgid(statBuf.st_gid);
    if (grp != nullptr) {
        result.addAttribute("group", std::string(grp->gr_name));
    } else {
        result.addAttribute("group", "unknown");
    }
}

void FileCollector::collectAccessInfo(const std::string& path, MetadataResult& result) {
    // Check what access the current user has
    bool readable = (access(path.c_str(), R_OK) == 0);
    bool writable = (access(path.c_str(), W_OK) == 0);
    bool executable = (access(path.c_str(), X_OK) == 0);

    result.addAttribute("current_user_readable", readable ? "true" : "false");
    result.addAttribute("current_user_writable", writable ? "true" : "false");
    result.addAttribute("current_user_executable", executable ? "true" : "false");
}

std::string FileCollector::fileTypeToString(mode_t mode) {
    if (S_ISREG(mode))  return "regular";
    if (S_ISDIR(mode))  return "directory";
    if (S_ISLNK(mode))  return "symlink";
    if (S_ISBLK(mode))  return "block_device";
    if (S_ISCHR(mode))  return "character_device";
    if (S_ISFIFO(mode)) return "fifo";
    if (S_ISSOCK(mode)) return "socket";
    return "unknown";
}

std::string FileCollector::modeToSymbolic(mode_t mode) {
    char symbolic[10];

    // Owner permissions
    symbolic[0] = (mode & S_IRUSR) ? 'r' : '-';
    symbolic[1] = (mode & S_IWUSR) ? 'w' : '-';
    if (mode & S_ISUID) {
        symbolic[2] = (mode & S_IXUSR) ? 's' : 'S';
    } else {
        symbolic[2] = (mode & S_IXUSR) ? 'x' : '-';
    }

    // Group permissions
    symbolic[3] = (mode & S_IRGRP) ? 'r' : '-';
    symbolic[4] = (mode & S_IWGRP) ? 'w' : '-';
    if (mode & S_ISGID) {
        symbolic[5] = (mode & S_IXGRP) ? 's' : 'S';
    } else {
        symbolic[5] = (mode & S_IXGRP) ? 'x' : '-';
    }

    // Other permissions
    symbolic[6] = (mode & S_IROTH) ? 'r' : '-';
    symbolic[7] = (mode & S_IWOTH) ? 'w' : '-';
    if (mode & S_ISVTX) {
        symbolic[8] = (mode & S_IXOTH) ? 't' : 'T';
    } else {
        symbolic[8] = (mode & S_IXOTH) ? 'x' : '-';
    }

    symbolic[9] = '\0';
    return std::string(symbolic);
}

std::string FileCollector::timeToString(time_t timeVal) {
    struct tm* tm_info = localtime(&timeVal);

    if (tm_info == nullptr) {
        return "unknown";
    }

    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", tm_info);
    return std::string(buffer);
}

} // namespace AixMetadata
