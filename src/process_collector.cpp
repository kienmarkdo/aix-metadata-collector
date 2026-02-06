/**
 * @file process_collector.cpp
 * @brief Implementation of process metadata collector for AIX
 *
 * This file implements the ProcessCollector class which uses AIX-specific
 * APIs to retrieve comprehensive process metadata.
 *
 * AIX-specific APIs used:
 *   - getprocs64(): Retrieves process table entries
 *   - /proc/[pid]/psinfo: Process status information
 *   - /proc/[pid]/cred: Process credentials
 *   - /proc/[pid]/fd/: Open file descriptors
 *   - readlink(): For symlink resolution
 */

#include "process_collector.h"

#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <sstream>
#include <fstream>
#include <dirent.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>

// AIX-specific headers for process information
#ifdef _AIX
#include <procinfo.h>
#include <sys/procfs.h>
#endif

namespace AixMetadata {

MetadataResult ProcessCollector::collect(const std::string& identifier) {
    MetadataResult result;
    result.type = "process";
    result.identifier = identifier;

    pid_t pid;
    if (!parsePid(identifier, pid)) {
        return createErrorResult(identifier, "Invalid PID format: " + identifier);
    }

    // Collect basic process info first - this validates the process exists
    if (!collectBasicInfo(pid, result)) {
        return createErrorResult(identifier,
            "Process not found or access denied for PID: " + identifier);
    }

    result.success = true;

    // Collect additional information (these may partially fail but we continue)
    collectExecutablePath(pid, result);
    collectWorkingDirectory(pid, result);
    collectCommandLine(pid, result);
    collectCredentials(pid, result);
    collectOpenFiles(pid, result);
    collectWparInfo(pid, result);

    // Optionally collect environment (may be restricted)
    // collectEnvironment(pid, result);

    return result;
}

bool ProcessCollector::parsePid(const std::string& identifier, pid_t& pid) {
    char* endPtr = nullptr;
    long value = std::strtol(identifier.c_str(), &endPtr, 10);

    // Check for conversion errors
    if (endPtr == identifier.c_str() || *endPtr != '\0') {
        return false;
    }

    // Check for valid PID range
    if (value <= 0 || value > INT32_MAX) {
        return false;
    }

    pid = static_cast<pid_t>(value);
    return true;
}

bool ProcessCollector::collectBasicInfo(pid_t pid, MetadataResult& result) {
#ifdef _AIX
    // Use getprocs64() to get process information on AIX
    struct procentry64 procInfo;
    pid_t inputPid = pid;

    // getprocs64 returns the number of processes copied
    // We request 1 process starting from the given PID
    int count = getprocs64(&procInfo, sizeof(procInfo), nullptr, 0, &inputPid, 1);

    if (count != 1 || procInfo.pi_pid != pid) {
        return false;
    }

    // Basic process identifiers
    result.addAttribute("pid", static_cast<int64_t>(procInfo.pi_pid));
    result.addAttribute("ppid", static_cast<int64_t>(procInfo.pi_ppid));
    result.addAttribute("pgid", static_cast<int64_t>(procInfo.pi_pgrp));
    result.addAttribute("sid", static_cast<int64_t>(procInfo.pi_sid));

    // Process name (command)
    result.addAttribute("comm", std::string(procInfo.pi_comm));

    // User ID (procentry64 has pi_uid but not pi_gid directly)
    result.addAttribute("uid", static_cast<int64_t>(procInfo.pi_uid));

    // Resolve username
    struct passwd* pwd = getpwuid(procInfo.pi_uid);
    if (pwd != nullptr) {
        result.addAttribute("user", std::string(pwd->pw_name));
        // Get primary group from passwd entry
        result.addAttribute("gid", static_cast<int64_t>(pwd->pw_gid));
        struct group* grp = getgrgid(pwd->pw_gid);
        if (grp != nullptr) {
            result.addAttribute("group", std::string(grp->gr_name));
        }
    }

    // Process state
    result.addAttribute("state", stateToString(procInfo.pi_state));

    // Priority and nice value
    result.addAttribute("priority", static_cast<int64_t>(procInfo.pi_pri));
    result.addAttribute("nice", static_cast<int64_t>(procInfo.pi_nice));

    // CPU information
    result.addAttribute("cpu", static_cast<int64_t>(procInfo.pi_cpu));

    // Memory information (in KB)
    // pi_size is the size of the process image in pages
    // pi_drss is the data resident set size in pages
    // pi_trss is the text resident set size in pages
    long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize > 0) {
        result.addAttribute("virtual_size_kb",
            static_cast<uint64_t>(procInfo.pi_size * pageSize / 1024));
        result.addAttribute("resident_size_kb",
            static_cast<uint64_t>((procInfo.pi_drss + procInfo.pi_trss) * pageSize / 1024));
    }

    // Start time (convert from AIX time format to epoch seconds)
    result.addAttribute("start_time", timeToString(procInfo.pi_start));

    // Number of threads
    result.addAttribute("num_threads", static_cast<int64_t>(procInfo.pi_thcount));

    // Flags
    std::ostringstream flagsHex;
    flagsHex << "0x" << std::hex << procInfo.pi_flags;
    result.addAttribute("flags", flagsHex.str());

    // TTY (controlling terminal)
    if (procInfo.pi_ttyd != (dev_t)-1) {
        std::ostringstream ttyStr;
        ttyStr << "major:" << major(procInfo.pi_ttyd)
               << ",minor:" << minor(procInfo.pi_ttyd);
        result.addAttribute("tty", ttyStr.str());
    } else {
        result.addAttribute("tty", "none");
    }

    return true;

#else
    // Non-AIX stub for compilation testing on other platforms
    // This allows development on macOS with actual testing on AIX
    result.addAttribute("pid", static_cast<int64_t>(pid));
    result.addAttribute("_note", "Process info collection requires AIX");
    return true;
#endif
}

void ProcessCollector::collectCommandLine(pid_t pid, MetadataResult& result) {
#ifdef _AIX
    // On AIX, we can try to read from /proc/[pid]/psinfo or use getargs()
    // First, try using getargs64() which gets the command line arguments

    struct procentry64 procInfo;
    pid_t inputPid = pid;

    if (getprocs64(&procInfo, sizeof(procInfo), nullptr, 0, &inputPid, 1) == 1) {
        // Buffer for command line arguments
        char argsBuffer[4096];
        memset(argsBuffer, 0, sizeof(argsBuffer));

        // getargs64 retrieves the command line arguments
        if (getargs(&procInfo, sizeof(procInfo), argsBuffer, sizeof(argsBuffer)) == 0) {
            // Arguments are null-separated, convert to space-separated
            std::string cmdline;
            char* ptr = argsBuffer;
            bool first = true;
            while (ptr < argsBuffer + sizeof(argsBuffer) && *ptr != '\0') {
                if (!first) {
                    cmdline += " ";
                }
                cmdline += ptr;
                ptr += strlen(ptr) + 1;
                first = false;

                // Safety check to avoid infinite loop
                if (ptr >= argsBuffer + sizeof(argsBuffer)) break;
            }

            if (!cmdline.empty()) {
                result.addAttribute("cmdline", cmdline);
            }
        }
    }
#else
    // Non-AIX stub
    std::ostringstream path;
    path << "/proc/" << pid << "/cmdline";

    std::ifstream file(path.str(), std::ios::binary);
    if (file.is_open()) {
        std::string cmdline;
        std::getline(file, cmdline, '\0');
        result.addAttribute("cmdline", cmdline);
    }
#endif
}

void ProcessCollector::collectEnvironment(pid_t pid, MetadataResult& result) {
#ifdef _AIX
    // On AIX, environment variables can be retrieved using getevars()
    struct procentry64 procInfo;
    pid_t inputPid = pid;

    if (getprocs64(&procInfo, sizeof(procInfo), nullptr, 0, &inputPid, 1) == 1) {
        char envBuffer[8192];
        memset(envBuffer, 0, sizeof(envBuffer));

        // Note: getevars may require elevated privileges
        if (getevars(&procInfo, sizeof(procInfo), envBuffer, sizeof(envBuffer)) == 0) {
            // Environment variables are null-separated
            std::vector<std::string> envVars;
            char* ptr = envBuffer;
            while (ptr < envBuffer + sizeof(envBuffer) && *ptr != '\0') {
                envVars.push_back(std::string(ptr));
                ptr += strlen(ptr) + 1;
                if (ptr >= envBuffer + sizeof(envBuffer)) break;
            }

            if (!envVars.empty()) {
                result.addAttribute("environment", envVars);
            }
        }
    }
#endif
}

void ProcessCollector::collectOpenFiles(pid_t pid, MetadataResult& result) {
#ifdef _AIX
    // On AIX, we can use /proc/[pid]/fd directory to list open file descriptors
    std::ostringstream fdDirPath;
    fdDirPath << "/proc/" << pid << "/fd";

    DIR* dir = opendir(fdDirPath.str().c_str());
    if (dir == nullptr) {
        // May not have permission to read /proc/[pid]/fd
        return;
    }

    std::vector<std::string> openFds;
    struct dirent* entry;

    while ((entry = readdir(dir)) != nullptr) {
        // Skip . and ..
        if (entry->d_name[0] == '.') {
            continue;
        }

        // Each entry in /proc/[pid]/fd is a symlink to the actual file
        std::ostringstream fdPath;
        fdPath << fdDirPath.str() << "/" << entry->d_name;

        char linkTarget[PATH_MAX];
        ssize_t len = readlink(fdPath.str().c_str(), linkTarget, sizeof(linkTarget) - 1);

        if (len > 0) {
            linkTarget[len] = '\0';
            std::ostringstream fdInfo;
            fdInfo << entry->d_name << ":" << linkTarget;
            openFds.push_back(fdInfo.str());
        } else {
            // If we can't read the link, just record the fd number
            openFds.push_back(std::string(entry->d_name));
        }
    }

    closedir(dir);

    if (!openFds.empty()) {
        result.addAttribute("open_files", openFds);
    }
#else
    // Non-AIX stub
    result.addAttribute("open_files_note", "Open files collection requires AIX");
#endif
}

void ProcessCollector::collectExecutablePath(pid_t pid, MetadataResult& result) {
#ifdef _AIX
    // On AIX, the executable path can be read from /proc/[pid]/object/a.out
    // which is a symlink to the executable
    std::ostringstream exePath;
    exePath << "/proc/" << pid << "/object/a.out";

    char linkTarget[PATH_MAX];
    ssize_t len = readlink(exePath.str().c_str(), linkTarget, sizeof(linkTarget) - 1);

    if (len > 0) {
        linkTarget[len] = '\0';
        result.addAttribute("exe_path", std::string(linkTarget));
    } else {
        // Alternative: try to get it from procentry64
        struct procentry64 procInfo;
        pid_t inputPid = pid;

        if (getprocs64(&procInfo, sizeof(procInfo), nullptr, 0, &inputPid, 1) == 1) {
            // Try to construct path from pi_comm if available
            // Note: pi_comm only contains the basename
            result.addAttribute("exe_name", std::string(procInfo.pi_comm));
        }
    }
#else
    // Non-AIX stub
    std::ostringstream exePath;
    exePath << "/proc/" << pid << "/exe";

    char linkTarget[PATH_MAX];
    ssize_t len = readlink(exePath.str().c_str(), linkTarget, sizeof(linkTarget) - 1);

    if (len > 0) {
        linkTarget[len] = '\0';
        result.addAttribute("exe_path", std::string(linkTarget));
    }
#endif
}

void ProcessCollector::collectWorkingDirectory(pid_t pid, MetadataResult& result) {
#ifdef _AIX
    // On AIX, current working directory is at /proc/[pid]/cwd
    std::ostringstream cwdPath;
    cwdPath << "/proc/" << pid << "/cwd";

    char linkTarget[PATH_MAX];
    ssize_t len = readlink(cwdPath.str().c_str(), linkTarget, sizeof(linkTarget) - 1);

    if (len > 0) {
        linkTarget[len] = '\0';
        result.addAttribute("cwd", std::string(linkTarget));
    }
#else
    // Non-AIX stub - same path on Linux
    std::ostringstream cwdPath;
    cwdPath << "/proc/" << pid << "/cwd";

    char linkTarget[PATH_MAX];
    ssize_t len = readlink(cwdPath.str().c_str(), linkTarget, sizeof(linkTarget) - 1);

    if (len > 0) {
        linkTarget[len] = '\0';
        result.addAttribute("cwd", std::string(linkTarget));
    }
#endif
}

void ProcessCollector::collectCredentials(pid_t pid, MetadataResult& result) {
#ifdef _AIX
    // On AIX, credential information can be read from /proc/[pid]/cred
    // The cred file contains a prcred structure
    std::ostringstream credPath;
    credPath << "/proc/" << pid << "/cred";

    int fd = open(credPath.str().c_str(), O_RDONLY);
    if (fd >= 0) {
        struct prcred cred;
        if (read(fd, &cred, sizeof(cred)) == sizeof(cred)) {
            result.addAttribute("euid", static_cast<int64_t>(cred.pr_euid));
            result.addAttribute("egid", static_cast<int64_t>(cred.pr_egid));
            result.addAttribute("ruid", static_cast<int64_t>(cred.pr_ruid));
            result.addAttribute("rgid", static_cast<int64_t>(cred.pr_rgid));
            result.addAttribute("suid", static_cast<int64_t>(cred.pr_suid));
            result.addAttribute("sgid", static_cast<int64_t>(cred.pr_sgid));

            // Resolve effective username
            struct passwd* pwd = getpwuid(cred.pr_euid);
            if (pwd != nullptr) {
                result.addAttribute("effective_user", std::string(pwd->pw_name));
            }
        }
        close(fd);
    }
#endif
}

std::string ProcessCollector::stateToString(unsigned char state) {
#ifdef _AIX
    // AIX process states from procinfo.h
    switch (state) {
        case SNONE:   return "none";
        case SIDL:    return "idle";
        case SZOMB:   return "zombie";
        case SSTOP:   return "stopped";
        case SACTIVE: return "active";
        case SSWAP:   return "swapped";
        default:      return "unknown";
    }
#else
    return "unknown";
#endif
}

std::string ProcessCollector::timeToString(uint64_t timeVal) {
    // Convert time to ISO 8601 format
    time_t t = static_cast<time_t>(timeVal);
    struct tm* tm_info = localtime(&t);

    if (tm_info == nullptr) {
        return "unknown";
    }

    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", tm_info);
    return std::string(buffer);
}

void ProcessCollector::collectWparInfo(pid_t pid, MetadataResult& result) {
#ifdef _AIX
    /**
     * WPAR (Workload Partition) Detection for AIX
     *
     * Uses the pi_cid field from procentry64 structure to determine
     * if a process is running inside a WPAR container.
     *
     * From /usr/include/procinfo.h:
     *   cid_t pi_cid;  // configured wpar id
     *
     * Where cid_t is defined in <sys/corralid.h> as unsigned short.
     *
     * Values:
     *   pi_cid == 0  -> Process is in Global environment
     *   pi_cid > 0   -> Process is in a WPAR with that Corral ID
     *
     * Note: Running this from inside a WPAR will always show pi_cid=0
     *       because WPARs only see their own processes as "global" to them.
     */

    struct procentry64 procInfo;
    pid_t inputPid = pid;

    int count = getprocs64(&procInfo, sizeof(procInfo), nullptr, 0, &inputPid, 1);

    if (count == 1 && procInfo.pi_pid == pid) {
        // Get the Corral ID (WPAR ID) directly from the process structure
        cid_t wparCid = procInfo.pi_cid;

        result.addAttribute("wpar_cid", static_cast<int64_t>(wparCid));

        if (wparCid == 0) {
            // Process is in Global environment
            result.addAttribute("is_container", "false");
        } else {
            // Process is in a WPAR container
            result.addAttribute("is_container", "true");

            // Optionally, try to resolve WPAR name from /etc/corrals/index
            // Format: WparID:Type:Name:Kernel_CID
            // We look for a line where Kernel_CID matches our wparCid
            std::ifstream indexFile("/etc/corrals/index");
            if (indexFile.is_open()) {
                std::string line;
                while (std::getline(indexFile, line)) {
                    // Parse: ID:Type:Name:KernelCID
                    std::istringstream iss(line);
                    std::string wparId, wparType, wparName, kernelCidStr;

                    if (std::getline(iss, wparId, ':') &&
                        std::getline(iss, wparType, ':') &&
                        std::getline(iss, wparName, ':') &&
                        std::getline(iss, kernelCidStr, ':')) {

                        int kernelCid = std::atoi(kernelCidStr.c_str());
                        if (kernelCid == static_cast<int>(wparCid)) {
                            // Found matching WPAR
                            result.addAttribute("wpar_name", wparName);
                            result.addAttribute("wpar_id", wparId);

                            // Decode WPAR type
                            std::string typeStr;
                            if (wparType == "S") typeStr = "system";
                            else if (wparType == "A") typeStr = "application";
                            else if (wparType == "L") typeStr = "versioned";
                            else typeStr = wparType;
                            result.addAttribute("wpar_type", typeStr);

                            break;
                        }
                    }
                }
                indexFile.close();
            }
        }
    }
    // If getprocs64 failed, we don't add WPAR info (process may not exist)

#else
    // Non-AIX stub for development/testing
    result.addAttribute("wpar_cid", static_cast<int64_t>(0));
    result.addAttribute("is_container", "false");
    result.addAttribute("wpar_note", "WPAR detection requires AIX");
#endif
}

} // namespace AixMetadata
