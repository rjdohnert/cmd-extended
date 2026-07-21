#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <windows.h>

// Defines the standard ways input can be separated
enum class DelimitMode {
    WHITESPACE,
    NEWLINE,
    NULL_CHAR
};

// Properly escapes arguments for the Windows command line (CreateProcess rules)
std::string escape_argument(const std::string& arg) {
    if (arg.empty()) {
        return "\"\"";
    }
    // If there are no spaces, tabs, double quotes, or newlines, we can output it as-is
    if (arg.find_first_of(" \t\n\v\"") == std::string::npos) {
        return arg;
    }

    std::string escaped = "\"";
    for (size_t i = 0; i < arg.length(); ++i) {
        size_t backslashes = 0;
        while (i < arg.length() && arg[i] == '\\') {
            backslashes++;
            i++;
        }

        if (i == arg.length()) {
            escaped.append(backslashes * 2, '\\');
        } else if (arg[i] == '"') {
            escaped.append(backslashes * 2 + 1, '\\');
            escaped.push_back('"');
        } else {
            escaped.append(backslashes, '\\');
            escaped.push_back(arg[i]);
        }
    }
    escaped.push_back('"');
    return escaped;
}

// Replaces all occurrences of 'from' with 'to' in 'str'
std::string replace_all(std::string str, const std::string& from, const std::string& to) {
    if (from.empty()) return str;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}

// Builds a single command line string from a list of arguments
std::string build_command_line(const std::vector<std::string>& args) {
    std::string cmd_line;
    for (size_t i = 0; i < args.size(); ++i) {
        cmd_line += escape_argument(args[i]);
        if (i + 1 < args.size()) {
            cmd_line += " ";
        }
    }
    return cmd_line;
}

// Executes the process on Windows using the Win32 API
bool execute_command(const std::string& cmd_line, bool verbose) {
    if (verbose) {
        std::cerr << cmd_line << std::endl;
    }

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // CreateProcess requires a modifiable char buffer
    std::vector<char> cmd_copy(cmd_line.begin(), cmd_line.end());
    cmd_copy.push_back('\0');

    if (!CreateProcessA(
        NULL,               // Application name
        cmd_copy.data(),    // Modifiable command line string
        NULL,               // Process attributes
        NULL,               // Thread attributes
        FALSE,              // Inherit handles
        0,                  // Creation flags
        NULL,               // Environment
        NULL,               // Current directory
        &si,                // Startup info
        &pi                 // Process information
    )) {
        std::cerr << "xargs: failed to execute process (Error: " << GetLastError() << ")\n";
        return false;
    }

    // Wait until child process exits
    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

// Reads and parses input stream based on selected delimitation rules
std::vector<std::string> read_input(DelimitMode mode) {
    std::vector<std::string> args;
    char ch;
    std::string current;

    if (mode == DelimitMode::NULL_CHAR) {
        while (std::cin.get(ch)) {
            if (ch == '\0') {
                if (!current.empty()) {
                    args.push_back(current);
                    current.clear();
                }
            } else {
                current.push_back(ch);
            }
        }
        if (!current.empty()) {
            args.push_back(current);
        }
    } else if (mode == DelimitMode::NEWLINE) {
        while (std::cin.get(ch)) {
            if (ch == '\n') {
                if (!current.empty() && current.back() == '\r') {
                    current.pop_back(); // Strip Carriage Return for Windows compatibility
                }
                if (!current.empty()) {
                    args.push_back(current);
                    current.clear();
                }
            } else {
                current.push_back(ch);
            }
        }
        if (!current.empty()) {
            if (current.back() == '\r') current.pop_back();
            if (!current.empty()) args.push_back(current);
        }
    } else { // DelimitMode::WHITESPACE
        // State machine parsing to handle escaping and quotes inside standard input
        enum State { NORMAL, IN_SINGLE_QUOTE, IN_DOUBLE_QUOTE, ESCAPE };
        State state = NORMAL;
        State prev_state = NORMAL;

        while (std::cin.get(ch)) {
            switch (state) {
                case NORMAL:
                    if (ch == '\\') {
                        prev_state = NORMAL;
                        state = ESCAPE;
                    } else if (ch == '\'') {
                        state = IN_SINGLE_QUOTE;
                    } else if (ch == '"') {
                        state = IN_DOUBLE_QUOTE;
                    } else if (isspace(static_cast<unsigned char>(ch))) {
                        if (!current.empty()) {
                            args.push_back(current);
                            current.clear();
                        }
                    } else {
                        current.push_back(ch);
                    }
                    break;
                case IN_SINGLE_QUOTE:
                    if (ch == '\'') {
                        state = NORMAL;
                    } else {
                        current.push_back(ch);
                    }
                    break;
                case IN_DOUBLE_QUOTE:
                    if (ch == '\\') {
                        prev_state = IN_DOUBLE_QUOTE;
                        state = ESCAPE;
                    } else if (ch == '"') {
                        state = NORMAL;
                    } else {
                        current.push_back(ch);
                    }
                    break;
                case ESCAPE:
                    current.push_back(ch);
                    state = prev_state;
                    break;
            }
        }
        if (!current.empty()) {
            args.push_back(current);
        }
    }
    return args;
}

void print_usage() {
    std::cerr << "Usage: xargs [-0] [-t] [-r] [-n max_args] [-I replace_str] [command [initial-arguments]]\n";
}

int main(int argc, char* argv[]) {
    bool null_terminated = false;
    bool verbose = false;
    bool no_run_if_empty = false;
    int max_args = -1;
    std::string replace_str = "";
    std::vector<std::string> command;

    // Command-line options parsing loop
    int i = 1;
    while (i < argc) {
        std::string arg = argv[i];
        if (arg == "--") {
            i++;
            break;
        } else if (arg == "-0") {
            null_terminated = true;
            i++;
        } else if (arg == "-t") {
            verbose = true;
            i++;
        } else if (arg == "-r" || arg == "--no-run-if-empty") {
            no_run_if_empty = true;
            i++;
        } else if (arg == "-n") {
            if (i + 1 < argc) {
                try {
                    max_args = std::stoi(argv[i + 1]);
                    if (max_args <= 0) {
                        std::cerr << "xargs: value for -n option must be >= 1\n";
                        return 1;
                    }
                } catch (...) {
                    std::cerr << "xargs: invalid number for -n option\n";
                    return 1;
                }
                i += 2;
            } else {
                std::cerr << "xargs: option -n requires an argument\n";
                return 1;
            }
        } else if (arg == "-I") {
            if (i + 1 < argc) {
                replace_str = argv[i + 1];
                i += 2;
            } else {
                std::cerr << "xargs: option -I requires an argument\n";
                return 1;
            }
        } else if (arg[0] == '-') {
            std::cerr << "xargs: unknown option: " << arg << "\n";
            print_usage();
            return 1;
        } else {
            break;
        }
    }

    // Remaining arguments denote the target command to run
    while (i < argc) {
        command.push_back(argv[i]);
        i++;
    }

    // Standard xargs defaults to "echo" if no utility is specified
    if (command.empty()) {
        command = { "cmd.exe", "/c", "echo" };
    }

    // Configure delimiting mode
    DelimitMode mode = DelimitMode::WHITESPACE;
    if (null_terminated) {
        mode = DelimitMode::NULL_CHAR;
    } else if (!replace_str.empty()) {
        // -I implies processing input line-by-line
        mode = DelimitMode::NEWLINE;
    }

    std::vector<std::string> input_args = read_input(mode);

    if (input_args.empty()) {
        if (!no_run_if_empty) {
            std::string cmd_line = build_command_line(command);
            execute_command(cmd_line, verbose);
        }
        return 0;
    }

    // Execution Logic
    if (!replace_str.empty()) {
        // Placeholder replacement execution (one run per input item)
        for (const auto& item : input_args) {
            std::vector<std::string> replaced_command;
            for (const auto& arg : command) {
                replaced_command.push_back(replace_all(arg, replace_str, item));
            }
            std::string cmd_line = build_command_line(replaced_command);
            execute_command(cmd_line, verbose);
        }
    } else {
        // Grouped execution mode
        size_t base_length = 0;
        for (const auto& c : command) {
            base_length += escape_argument(c).length() + 1;
        }

        std::vector<std::string> current_chunk = command;
        size_t current_length = base_length;
        int current_args_count = 0;

        for (const auto& item : input_args) {
            std::string escaped_item = escape_argument(item);
            bool limit_reached = false;

            if (max_args > 0 && current_args_count >= max_args) {
                limit_reached = true;
            }
            // Win32 API maximum command line is 32,767. We cut off early to be safe.
            if (current_length + escaped_item.length() + 1 >= 32000) {
                limit_reached = true;
            }

            if (limit_reached && current_args_count > 0) {
                std::string cmd_line = build_command_line(current_chunk);
                execute_command(cmd_line, verbose);

                current_chunk = command;
                current_length = base_length;
                current_args_count = 0;
            }

            current_chunk.push_back(item);
            current_length += escaped_item.length() + 1;
            current_args_count++;
        }

        if (current_args_count > 0) {
            std::string cmd_line = build_command_line(current_chunk);
            execute_command(cmd_line, verbose);
        }
    }

    return 0;
}