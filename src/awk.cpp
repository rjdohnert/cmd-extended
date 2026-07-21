#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <regex>
#include <memory>

// Helper to trim whitespace from the ends of a string
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

// Replaces escaped quotes \" with standard quotes " to handle CMD command parsing quirks
std::string sanitize_script(std::string str) {
    size_t pos = 0;
    while ((pos = str.find("\\\"", pos)) != std::string::npos) {
        str.replace(pos, 2, "\"");
        pos += 1;
    }
    return str;
}

// Custom split algorithm to mirror awk behavior
// If fs is " ", consecutive spaces/tabs are treated as a single delimiter.
// Otherwise, splits strictly by the separator string.
std::vector<std::string> split_fields(const std::string& line, const std::string& fs) {
    std::vector<std::string> fields;
    if (fs == " ") {
        std::string current;
        for (char c : line) {
            if (c == ' ' || c == '\t') {
                if (!current.empty()) {
                    fields.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
        if (!current.empty()) {
            fields.push_back(current);
        }
    } else {
        size_t start = 0;
        size_t end = line.find(fs);
        while (end != std::string::npos) {
            fields.push_back(line.substr(start, end - start));
            start = end + fs.length();
            end = line.find(fs, start);
        }
        fields.push_back(line.substr(start));
    }
    return fields;
}

// Parses print arguments while keeping quoted strings intact
// Example input: "$1, \" -> \", $2"
std::vector<std::string> parse_print_args(const std::string& args_str) {
    std::vector<std::string> args;
    std::string current;
    bool in_quotes = false;
    
    for (char c : args_str) {
        if (c == '"') {
            in_quotes = !in_quotes;
            current += c;
        } else if (c == ',' && !in_quotes) {
            args.push_back(trim(current));
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        args.push_back(trim(current));
    }
    return args;
}

// Processes an input stream line by line
void process_stream(std::istream& in, 
                    const std::string& fs, 
                    const std::regex* pattern_regex, 
                    const std::vector<std::string>& print_args, 
                    size_t& nr) {
    std::string line;
    while (std::getline(in, line)) {
        nr++; // Increment Record Number
        
        // Apply regex filtering if defined
        if (pattern_regex && !std::regex_search(line, *pattern_regex)) {
            continue;
        }

        std::vector<std::string> fields = split_fields(line, fs);
        size_t nf = fields.size(); // Number of Fields

        // Execute print statement
        for (size_t i = 0; i < print_args.size(); ++i) {
            const std::string& arg = print_args[i];
            std::string val;

            if (arg.front() == '"' && arg.back() == '"' && arg.length() >= 2) {
                // String literal (strip outer quotes)
                val = arg.substr(1, arg.length() - 2);
            } else if (arg == "$0") {
                val = line;
            } else if (arg == "$NF") {
                if (nf > 0) val = fields.back();
            } else if (arg.front() == '$') {
                try {
                    int idx = std::stoi(arg.substr(1));
                    if (idx > 0 && static_cast<size_t>(idx) <= nf) {
                        val = fields[idx - 1];
                    }
                } catch (...) {
                    val = arg; // Fallback to raw string if numeric conversion fails
                }
            } else if (arg == "NR") {
                val = std::to_string(nr);
            } else if (arg == "NF") {
                val = std::to_string(nf);
            } else {
                val = arg;
            }

            std::cout << val;
            if (i + 1 < print_args.size()) {
                std::cout << " "; // Default Output Field Separator (OFS)
            }
        }
        std::cout << "\n";
    }
}

int main(int argc, char* argv[]) {
    std::string fs = " "; // Default Field Separator is whitespace
    std::string script_arg;
    std::vector<std::string> files;

    // Command-line parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-F") {
            if (i + 1 < argc) {
                fs = argv[++i];
            } else {
                std::cerr << "Error: -F option requires an argument\n";
                return 1;
            }
        } else if (script_arg.empty()) {
            script_arg = arg;
        } else {
            files.push_back(arg);
        }
    }

    if (script_arg.empty()) {
        std::cerr << "Usage: awk [-F <delimiter>] \"[pattern] { print arg1, arg2, ... }\" [file...]\n";
        return 1;
    }

    // Prepare script
    std::string sanitized = sanitize_script(script_arg);

    // Regex to match: optional /pattern/ followed by required { print ... }
    std::regex main_regex(R"(^\s*(?:\/([^\/]+)\/)?\s*\{\s*print\s*(.*?)\s*\}\s*$)");
    std::smatch match;

    if (!std::regex_match(sanitized, match, main_regex)) {
        std::cerr << "Error: Invalid script format. Expected: \"/pattern/ { print $1 }\" or \"{ print $1 }\"\n";
        return 1;
    }

    std::string pattern_str = match[1].str();
    std::string action_str = match[2].str();

    // Set up regular expression filter if provided
    std::unique_ptr<std::regex> pattern_regex;
    if (!pattern_str.empty()) {
        try {
            pattern_regex = std::make_unique<std::regex>(pattern_str);
        } catch (const std::regex_error& e) {
            std::cerr << "Error: Invalid regex: " << e.what() << "\n";
            return 1;
        }
    }

    // Determine print targets
    std::vector<std::string> print_args;
    if (trim(action_str).empty()) {
        print_args.push_back("$0"); // Default print statement is equivalent to print $0
    } else {
        print_args = parse_print_args(action_str);
    }

    size_t nr = 0; // Tracks line number across files

    // Stream from files or standard input
    if (files.empty()) {
        process_stream(std::cin, fs, pattern_regex.get(), print_args, nr);
    } else {
        for (const auto& file : files) {
            std::ifstream infile(file);
            if (!infile.is_open()) {
                std::cerr << "Error: Could not open file: " << file << "\n";
                continue;
            }
            process_stream(infile, fs, pattern_regex.get(), print_args, nr);
        }
    }

    return 0;
}