#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cctype>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

// Class to manage pushbacks (LIFO buffer) and file inclusions
class PushbackStream {
private:
    std::vector<std::istream*> stream_stack;
    std::string pushback_buf;

public:
    PushbackStream() {}

    void push_stream(std::istream* stream) {
        stream_stack.push_back(stream);
    }

    void pop_stream() {
        if (!stream_stack.empty()) {
            stream_stack.pop_back();
        }
    }

    // Pushback a string (reverses character order to preserve LIFO reading)
    void push_back(const std::string& s) {
        for (auto it = s.rbegin(); it != s.rend(); ++it) {
            pushback_buf.push_back(*it);
        }
    }

    void push_char(char c) {
        pushback_buf.push_back(c);
    }

    int get_char() {
        if (!pushback_buf.empty()) {
            char c = pushback_buf.back();
            pushback_buf.pop_back();
            return static_cast<unsigned char>(c);
        }
        while (!stream_stack.empty()) {
            std::istream* current = stream_stack.back();
            int c = current->get();
            if (c != EOF) {
                return c;
            }
            if (stream_stack.size() > 1) {
                stream_stack.pop_back();
            } else {
                return EOF;
            }
        }
        return EOF;
    }
};

enum class TokenType {
    EOF_TOKEN,
    NAME,
    SIMPLE,
    QUOTED,
    COMMENT
};

struct Token {
    TokenType type;
    std::string value;
};

class M4Processor {
private:
    PushbackStream input;
    std::string lquote = "`";
    std::string rquote = "'";
    std::string comment_start = "#";
    std::string comment_end = "\n";
    std::ostream& out_stream;

    struct MacroDef {
        bool is_builtin;
        std::string text;
        std::function<std::string(const std::vector<std::string>&)> builtin_func;
    };

    std::unordered_map<std::string, MacroDef> macros;

    // Helper to match a multi-character delimiter at the current stream position
    bool match_string(int first_char, const std::string& pattern) {
        if (pattern.empty()) return false;
        if (first_char != static_cast<unsigned char>(pattern[0])) return false;

        std::string read_chars;
        read_chars.push_back(static_cast<char>(first_char));
        bool matched = true;

        for (size_t i = 1; i < pattern.size(); ++i) {
            int next = input.get_char();
            if (next == EOF) {
                matched = false;
                break;
            }
            read_chars.push_back(static_cast<char>(next));
            if (next != static_cast<unsigned char>(pattern[i])) {
                matched = false;
                break;
            }
        }

        if (!matched) {
            input.push_back(read_chars);
            return false;
        }
        return true;
    }

    Token get_token() {
        int c = input.get_char();
        if (c == EOF) {
            return {TokenType::EOF_TOKEN, ""};
        }

        // Handle Comments
        if (!comment_start.empty() && match_string(c, comment_start)) {
            std::string comment = comment_start;
            while (true) {
                int next_c = input.get_char();
                if (next_c == EOF) break;
                comment.push_back(static_cast<char>(next_c));
                if (!comment_end.empty() && comment.size() >= comment_end.size() &&
                    comment.compare(comment.size() - comment_end.size(), comment_end.size(), comment_end) == 0) {
                    break;
                }
            }
            return {TokenType::COMMENT, comment};
        }

        // Handle Quoting
        if (!lquote.empty() && match_string(c, lquote)) {
            int nest_level = 1;
            std::string content;
            while (true) {
                int next_c = input.get_char();
                if (next_c == EOF) break;

                if (!lquote.empty() && match_string(next_c, lquote)) {
                    nest_level++;
                    content += lquote;
                } else if (!rquote.empty() && match_string(next_c, rquote)) {
                    nest_level--;
                    if (nest_level == 0) break;
                    content += rquote;
                } else {
                    content.push_back(static_cast<char>(next_c));
                }
            }
            return {TokenType::QUOTED, content};
        }

        // Handle Alphanumeric macro names
        if (std::isalpha(c) || c == '_') {
            std::string name;
            name.push_back(static_cast<char>(c));
            while (true) {
                int next_c = input.get_char();
                if (next_c == EOF) break;
                if (std::isalnum(next_c) || next_c == '_') {
                    name.push_back(static_cast<char>(next_c));
                } else {
                    input.push_char(static_cast<char>(next_c));
                    break;
                }
            }
            return {TokenType::NAME, name};
        }

        // Simple raw character
        return {TokenType::SIMPLE, std::string(1, static_cast<char>(c))};
    }

    void push_token_back(const Token& t) {
        if (t.type == TokenType::QUOTED) {
            input.push_back(lquote + t.value + rquote);
        } else {
            input.push_back(t.value);
        }
    }

    // Collect macro arguments and respect nested quoting
    std::vector<std::string> collect_arguments() {
        std::vector<std::string> args;
        Token t = get_token();
        if (t.type != TokenType::SIMPLE || t.value != "(") {
            if (t.type != TokenType::EOF_TOKEN) {
                push_token_back(t);
            }
            return args;
        }

        std::string current_arg;
        int paren_depth = 0;
        while (true) {
            Token arg_t = get_token();
            if (arg_t.type == TokenType::EOF_TOKEN) break;

            if (arg_t.type == TokenType::SIMPLE) {
                if (arg_t.value == "(") {
                    paren_depth++;
                    current_arg += "(";
                } else if (arg_t.value == ")") {
                    if (paren_depth == 0) {
                        args.push_back(current_arg);
                        break;
                    } else {
                        paren_depth--;
                        current_arg += ")";
                    }
                } else if (arg_t.value == "," && paren_depth == 0) {
                    args.push_back(current_arg);
                    current_arg.clear();
                } else {
                    current_arg += arg_t.value;
                }
            } else if (arg_t.type == TokenType::QUOTED) {
                current_arg += lquote + arg_t.value + rquote;
            } else if (arg_t.type == TokenType::NAME) {
                if (macros.count(arg_t.value)) {
                    expand_macro(arg_t.value);
                } else {
                    current_arg += arg_t.value;
                }
            } else {
                current_arg += arg_t.value;
            }
        }
        return args;
    }

    // Standard $1, $2, $# argument substitution
    std::string substitute_args(const std::string& def, const std::vector<std::string>& args) {
        std::string res;
        for (size_t i = 0; i < def.size(); ++i) {
            if (def[i] == '$') {
                if (i + 1 < def.size()) {
                    char next = def[i + 1];
                    if (std::isdigit(next)) {
                        int arg_idx = next - '0';
                        if (arg_idx < static_cast<int>(args.size())) {
                            res += args[arg_idx];
                        }
                        i++;
                    } else if (next == '#') {
                        int count = static_cast<int>(args.size()) - 1;
                        res += std::to_string(count < 0 ? 0 : count);
                        i++;
                    } else if (next == '*') {
                        for (size_t j = 1; j < args.size(); ++j) {
                            if (j > 1) res += ",";
                            res += args[j];
                        }
                        i++;
                    } else if (next == '@') {
                        for (size_t j = 1; j < args.size(); ++j) {
                            if (j > 1) res += ",";
                            res += lquote + args[j] + rquote;
                        }
                        i++;
                    } else {
                        res.push_back('$');
                    }
                } else {
                    res.push_back('$');
                }
            } else {
                res.push_back(def[i]);
            }
        }
        return res;
    }

    void register_builtins() {
        macros["define"] = { true, "", [this](const std::vector<std::string>& args) {
            if (args.size() > 2) {
                macros[args[1]] = { false, args[2], nullptr };
            }
            return "";
        }};

        macros["undefine"] = { true, "", [this](const std::vector<std::string>& args) {
            if (args.size() > 1) {
                macros.erase(args[1]);
            }
            return "";
        }};

        macros["ifdef"] = { true, "", [this](const std::vector<std::string>& args) -> std::string {
            if (args.size() > 2) {
                if (macros.count(args[1]) > 0) {
                    return args[2];
                } else if (args.size() > 3) {
                    return args[3];
                }
            }
            return std::string("");
        }};

        macros["ifelse"] = { true, "", [this](const std::vector<std::string>& args) {
            if (args.size() < 4) return std::string("");
            size_t i = 1;
            while (i + 2 < args.size()) {
                if (args[i] == args[i + 1]) {
                    return args[i + 2];
                }
                i += 3;
            }
            if (i < args.size()) return args[i];
            return std::string("");
        }};

        macros["dnl"] = { true, "", [this](const std::vector<std::string>&) {
            while (true) {
                int c = input.get_char();
                if (c == EOF || c == '\n') break;
            }
            return "";
        }};

        macros["len"] = { true, "", [](const std::vector<std::string>& args) {
            return args.size() > 1 ? std::to_string(args[1].size()) : "0";
        }};

        macros["changequote"] = { true, "", [this](const std::vector<std::string>& args) {
            if (args.size() > 2) {
                lquote = args[1];
                rquote = args[2];
            } else {
                lquote = "`";
                rquote = "'";
            }
            return "";
        }};

        macros["include"] = { true, "", [this](const std::vector<std::string>& args) -> std::string {
            if (args.size() > 1) {
                std::ifstream file(args[1], std::ios::binary);
                if (file.is_open()) {
                    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                } else {
                    std::cerr << "m4: cannot open '" << args[1] << "': No such file or directory\n";
                }
            }
            return std::string("");
        }};
    }

public:
    M4Processor(std::ostream& out) : out_stream(out) {
        register_builtins();
    }

    void expand_macro(const std::string& name) {
        auto it = macros.find(name);
        if (it == macros.end()) return;

        std::vector<std::string> args = { name };
        std::vector<std::string> collected = collect_arguments();
        args.insert(args.end(), collected.begin(), collected.end());

        std::string expansion;
        if (it->second.is_builtin) {
            expansion = it->second.builtin_func(args);
        } else {
            expansion = substitute_args(it->second.text, args);
        }

        input.push_back(expansion);
    }

    void process(std::istream& in) {
        input.push_stream(&in);
        while (true) {
            Token t = get_token();
            if (t.type == TokenType::EOF_TOKEN) break;

            if (t.type == TokenType::NAME) {
                if (macros.count(t.value)) {
                    expand_macro(t.value);
                } else {
                    out_stream << t.value;
                }
            } else {
                out_stream << t.value;
            }
        }
        input.pop_stream();
    }
};

int main(int argc, char* argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    std::vector<std::string> files;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg[0] == '-' && arg.size() > 1) {
            if (arg == "-h" || arg == "--help") {
                std::cerr << "usage: m4 [file ...]\n";
                return 0;
            } else {
                std::cerr << "m4: unknown option: " << arg << "\n";
                return 1;
            }
        } else {
            files.push_back(arg);
        }
    }

    M4Processor processor(std::cout);

    if (files.empty()) {
        processor.process(std::cin);
    } else {
        for (const auto& file : files) {
            std::ifstream infile(file, std::ios::binary);
            if (!infile.is_open()) {
                std::cerr << "m4: cannot open '" << file << "': No such file or directory\n";
                return 1;
            }
            processor.process(infile);
        }
    }

    return 0;
}