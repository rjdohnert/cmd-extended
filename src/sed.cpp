#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <fstream>
#include <memory>
#include <cctype>

// Helper to trim leading/trailing whitespace
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

// Translates sed-style backreferences (\1, \2, etc. and &) to standard C++ regex syntax ($1, $2, $&)
std::string convert_replacement_syntax(const std::string& r) {
    std::string out;
    for (size_t i = 0; i < r.length(); ++i) {
        if (r[i] == '\\' && i + 1 < r.length() && std::isdigit(static_cast<unsigned char>(r[i+1]))) {
            out += '$';
            out += r[i+1];
            i++;
        } else if (r[i] == '&') {
            out += "$&";
        } else {
            out += r[i];
        }
    }
    return out;
}

struct SedCommand {
    enum Type { SUBSTITUTE, DELETE, PRINT, UNKNOWN };
    Type type = UNKNOWN;
    
    // Address matching criteria
    bool has_address = false;
    int start_line = -1;
    int end_line = -1;
    bool has_addr_regex = false;
    std::regex addr_regex;
    
    // Substitution properties
    std::regex sub_regex;
    std::string replacement;
    bool global = false;
};

// Parses a single command string (e.g., "3,5d" or "s/foo/bar/g")
bool parse_command(std::string cmd_str, SedCommand& cmd) {
    cmd_str = trim(cmd_str);
    if (cmd_str.empty()) return false;
    
    size_t idx = 0;
    
    // 1. Try to parse an address pattern or line range
    if (cmd_str[idx] == '/') {
        // Regex address, e.g., /pattern/
        size_t closing = cmd_str.find('/', idx + 1);
        if (closing == std::string::npos) return false;
        std::string pattern = cmd_str.substr(idx + 1, closing - idx - 1);
        cmd.has_address = true;
        cmd.has_addr_regex = true;
        cmd.addr_regex = std::regex(pattern);
        idx = closing + 1;
    } else if (std::isdigit(static_cast<unsigned char>(cmd_str[idx]))) {
        // Numeric range/line, e.g., 5 or 2,5
        size_t end_idx = idx;
        while (end_idx < cmd_str.length() && (std::isdigit(static_cast<unsigned char>(cmd_str[end_idx])) || cmd_str[end_idx] == ',')) {
            end_idx++;
        }
        std::string range_str = cmd_str.substr(idx, end_idx - idx);
        size_t comma = range_str.find(',');
        cmd.has_address = true;
        if (comma == std::string::npos) {
            cmd.start_line = std::stoi(range_str);
            cmd.end_line = cmd.start_line;
        } else {
            cmd.start_line = std::stoi(range_str.substr(0, comma));
            cmd.end_line = std::stoi(range_str.substr(comma + 1));
        }
        idx = end_idx;
    }
    
    // Skip optional spaces between address and action
    while (idx < cmd_str.length() && std::isspace(static_cast<unsigned char>(cmd_str[idx]))) {
        idx++;
    }
    
    if (idx >= cmd_str.length()) return false;
    
    // 2. Parse the action command
    char action = cmd_str[idx];
    if (action == 'd') {
        cmd.type = SedCommand::DELETE;
    } else if (action == 'p') {
        cmd.type = SedCommand::PRINT;
    } else if (action == 's') {
        cmd.type = SedCommand::SUBSTITUTE;
        if (idx + 1 >= cmd_str.length()) return false;
        char delim = cmd_str[idx + 1];
        
        // Safely extract the 'find' pattern, respecting escaped delimiters
        size_t p = idx + 2;
        std::string find_pattern;
        while (p < cmd_str.length()) {
            if (cmd_str[p] == '\\' && p + 1 < cmd_str.length() && cmd_str[p + 1] == delim) {
                find_pattern += delim;
                p += 2;
            } else if (cmd_str[p] == delim) {
                break;
            } else {
                find_pattern += cmd_str[p];
                p++;
            }
        }
        if (p >= cmd_str.length() || cmd_str[p] != delim) return false;
        
        p++; // Advance past the second delimiter
        
        // Safely extract the replacement pattern, respecting escaped delimiters
        std::string raw_replacement;
        while (p < cmd_str.length()) {
            if (cmd_str[p] == '\\' && p + 1 < cmd_str.length() && cmd_str[p + 1] == delim) {
                raw_replacement += delim;
                p += 2;
            } else if (cmd_str[p] == delim) {
                break;
            } else {
                raw_replacement += cmd_str[p];
                p++;
            }
        }
        if (p >= cmd_str.length() || cmd_str[p] != delim) return false;
        
        cmd.replacement = convert_replacement_syntax(raw_replacement);
        
        // Parse flags following the final delimiter
        std::string flags = cmd_str.substr(p + 1);
        auto regex_flags = std::regex_constants::ECMAScript;
        if (flags.find('i') != std::string::npos) {
            regex_flags |= std::regex_constants::icase;
        }
        if (flags.find('g') != std::string::npos) {
            cmd.global = true;
        }
        cmd.sub_regex = std::regex(find_pattern, regex_flags);
    } else {
        return false;
    }
    
    return true;
}

// Evaluates all parsed commands sequentially on a line-by-line basis
void process_stream(std::istream& in, const std::vector<SedCommand>& commands, bool quiet, size_t& line_num) {
    std::string line;
    while (std::getline(in, line)) {
        line_num++;
        bool deleted = false;
        
        for (const auto& cmd : commands) {
            bool matches_address = true;
            if (cmd.has_address) {
                if (cmd.has_addr_regex) {
                    matches_address = std::regex_search(line, cmd.addr_regex);
                } else {
                    matches_address = (static_cast<int>(line_num) >= cmd.start_line && static_cast<int>(line_num) <= cmd.end_line);
                }
            }
            
            if (matches_address) {
                if (cmd.type == SedCommand::DELETE) {
                    deleted = true;
                    break; // Skip further commands and do not print this line
                } else if (cmd.type == SedCommand::PRINT) {
                    std::cout << line << "\n";
                } else if (cmd.type == SedCommand::SUBSTITUTE) {
                    if (cmd.global) {
                        line = std::regex_replace(line, cmd.sub_regex, cmd.replacement);
                    } else {
                        line = std::regex_replace(line, cmd.sub_regex, cmd.replacement, std::regex_constants::format_first_only);
                    }
                }
            }
        }
        
        if (!deleted && !quiet) {
            std::cout << line << "\n";
        }
    }
}

int main(int argc, char* argv[]) {
    bool quiet = false;
    std::vector<std::string> raw_scripts;
    std::vector<std::string> files;

    // Command-line flag parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-n") {
            quiet = true;
        } else if (arg == "-e") {
            if (i + 1 < argc) {
                raw_scripts.push_back(argv[++i]);
            } else {
                std::cerr << "Error: -e requires an argument.\n";
                return 1;
            }
        } else if (arg.front() == '-' && arg.length() > 1) {
            std::cerr << "Error: Unknown option " << arg << "\n";
            return 1;
        } else if (raw_scripts.empty() && files.empty()) {
            raw_scripts.push_back(arg); // Standard sed: first positional argument is the script if -e isn't used
        } else {
            files.push_back(arg);
        }
    }

    if (raw_scripts.empty()) {
        std::cerr << "Usage: sed [-n] [-e command] [command] [file...]\n";
        return 1;
    }

    // Compile commands
    std::vector<SedCommand> commands;
    for (const auto& raw : raw_scripts) {
        SedCommand cmd;
        if (!parse_command(raw, cmd)) {
            std::cerr << "Error: Failed to parse command: \"" << raw << "\"\n";
            return 1;
        }
        commands.push_back(cmd);
    }

    size_t line_num = 0;

    // Read from standard input or process files sequentially
    if (files.empty()) {
        process_stream(std::cin, commands, quiet, line_num);
    } else {
        for (const auto& file : files) {
            std::ifstream infile(file);
            if (!infile.is_open()) {
                std::cerr << "Error: Could not open file: " << file << "\n";
                continue;
            }
            process_stream(infile, commands, quiet, line_num);
        }
    }

    return 0;
}