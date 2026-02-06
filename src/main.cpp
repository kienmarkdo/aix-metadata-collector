/**
 * @file main.cpp
 * @brief Main entry point for the AIX Metadata Collector CLI
 *
 * This is the main command-line interface for the AIX Metadata Collector.
 * It provides a unified interface to query metadata for:
 *   - Processes (by PID)
 *   - Files (by path)
 *   - Network ports (by port number)
 *
 * Usage:
 *   aix-metadata-collector --process <pid>
 *   aix-metadata-collector --file <path>
 *   aix-metadata-collector --port <port> [--protocol tcp|udp|both]
 *   aix-metadata-collector --help
 *   aix-metadata-collector --version
 *
 * Output is in JSON format by default.
 */

#include "types.h"
#include "process_collector.h"
#include "file_collector.h"
#include "port_collector.h"
#include "json_formatter.h"

#include <iostream>
#include <cstring>
#include <cstdlib>

namespace {

// Version information
const char* VERSION = "1.0.0";
const char* PROGRAM_NAME = "aix-metadata-collector";

/**
 * @brief Print usage information
 */
void printUsage() {
    std::cout << "AIX Metadata Collector v" << VERSION << "\n"
              << "\n"
              << "Usage:\n"
              << "  " << PROGRAM_NAME << " --process <pid>\n"
              << "  " << PROGRAM_NAME << " --file <path>\n"
              << "  " << PROGRAM_NAME << " --port <port> [--protocol tcp|udp|both]\n"
              << "  " << PROGRAM_NAME << " --help\n"
              << "  " << PROGRAM_NAME << " --version\n"
              << "\n"
              << "Options:\n"
              << "  -p, --process <pid>     Collect metadata for a process by PID\n"
              << "  -f, --file <path>       Collect metadata for a file by path\n"
              << "  -P, --port <port>       Collect metadata for network connections on a port\n"
              << "  --protocol <proto>      Protocol filter for port queries (tcp, udp, or both)\n"
              << "                          Default: both\n"
              << "  --compact               Output compact JSON (no pretty printing)\n"
              << "  -h, --help              Show this help message\n"
              << "  -v, --version           Show version information\n"
              << "\n"
              << "Examples:\n"
              << "  " << PROGRAM_NAME << " --process 1234\n"
              << "  " << PROGRAM_NAME << " --file /etc/passwd\n"
              << "  " << PROGRAM_NAME << " --port 22 --protocol tcp\n"
              << "  " << PROGRAM_NAME << " -p 1 --compact\n"
              << "\n"
              << "Output:\n"
              << "  Results are output in JSON format to stdout.\n"
              << "  Errors are output to stderr.\n"
              << "\n"
              << "Notes:\n"
              << "  - Some operations may require root privileges for full information.\n"
              << "  - Process and port queries may have limited data without elevated access.\n"
              << std::endl;
}

/**
 * @brief Print version information
 */
void printVersion() {
    std::cout << PROGRAM_NAME << " version " << VERSION << "\n"
              << "Built for AIX 7.2."
              << std::endl;
}

/**
 * @brief Parse command line arguments
 */
struct CommandLineArgs {
    enum class Mode {
        None,
        Process,
        File,
        Port,
        Help,
        Version
    };

    Mode mode = Mode::None;
    std::string identifier;
    AixMetadata::Protocol protocol = AixMetadata::Protocol::Both;
    bool prettyPrint = true;
    bool valid = true;
    std::string errorMessage;
};

CommandLineArgs parseArgs(int argc, char* argv[]) {
    CommandLineArgs args;

    if (argc < 2) {
        args.mode = CommandLineArgs::Mode::Help;
        return args;
    }

    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            args.mode = CommandLineArgs::Mode::Help;
            return args;
        }

        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--version") == 0) {
            args.mode = CommandLineArgs::Mode::Version;
            return args;
        }

        if (strcmp(arg, "-p") == 0 || strcmp(arg, "--process") == 0) {
            if (i + 1 >= argc) {
                args.valid = false;
                args.errorMessage = "Missing PID argument for --process";
                return args;
            }
            args.mode = CommandLineArgs::Mode::Process;
            args.identifier = argv[++i];
            continue;
        }

        if (strcmp(arg, "-f") == 0 || strcmp(arg, "--file") == 0) {
            if (i + 1 >= argc) {
                args.valid = false;
                args.errorMessage = "Missing path argument for --file";
                return args;
            }
            args.mode = CommandLineArgs::Mode::File;
            args.identifier = argv[++i];
            continue;
        }

        if (strcmp(arg, "-P") == 0 || strcmp(arg, "--port") == 0) {
            if (i + 1 >= argc) {
                args.valid = false;
                args.errorMessage = "Missing port argument for --port";
                return args;
            }
            args.mode = CommandLineArgs::Mode::Port;
            args.identifier = argv[++i];
            continue;
        }

        if (strcmp(arg, "--protocol") == 0) {
            if (i + 1 >= argc) {
                args.valid = false;
                args.errorMessage = "Missing protocol argument for --protocol";
                return args;
            }
            const char* proto = argv[++i];
            if (strcmp(proto, "tcp") == 0) {
                args.protocol = AixMetadata::Protocol::TCP;
            } else if (strcmp(proto, "udp") == 0) {
                args.protocol = AixMetadata::Protocol::UDP;
            } else if (strcmp(proto, "both") == 0) {
                args.protocol = AixMetadata::Protocol::Both;
            } else {
                args.valid = false;
                args.errorMessage = "Invalid protocol. Use: tcp, udp, or both";
                return args;
            }
            continue;
        }

        if (strcmp(arg, "--compact") == 0) {
            args.prettyPrint = false;
            continue;
        }

        // Unknown argument
        args.valid = false;
        args.errorMessage = std::string("Unknown argument: ") + arg;
        return args;
    }

    if (args.mode == CommandLineArgs::Mode::None) {
        args.valid = false;
        args.errorMessage = "No operation specified. Use --process, --file, or --port";
    }

    return args;
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    // Parse command line arguments
    CommandLineArgs args = parseArgs(argc, argv);

    // Handle invalid arguments
    if (!args.valid) {
        std::cerr << "Error: " << args.errorMessage << "\n";
        std::cerr << "Use --help for usage information." << std::endl;
        return 1;
    }

    // Handle help and version
    if (args.mode == CommandLineArgs::Mode::Help) {
        printUsage();
        return 0;
    }

    if (args.mode == CommandLineArgs::Mode::Version) {
        printVersion();
        return 0;
    }

    // Perform the requested operation
    AixMetadata::MetadataResult result;

    switch (args.mode) {
        case CommandLineArgs::Mode::Process: {
            AixMetadata::ProcessCollector collector;
            result = collector.collect(args.identifier);
            break;
        }

        case CommandLineArgs::Mode::File: {
            AixMetadata::FileCollector collector;
            result = collector.collect(args.identifier);
            break;
        }

        case CommandLineArgs::Mode::Port: {
            AixMetadata::PortCollector collector(args.protocol);
            result = collector.collect(args.identifier);
            break;
        }

        default:
            std::cerr << "Error: Internal error - unknown mode" << std::endl;
            return 1;
    }

    // Output result as JSON
    std::string json = AixMetadata::JsonFormatter::format(result, args.prettyPrint);
    std::cout << json << std::endl;

    // Return appropriate exit code
    return result.success ? 0 : 1;
}
