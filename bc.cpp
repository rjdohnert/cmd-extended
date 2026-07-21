#include <iostream>
#include <string>
#include <cmath>
#include <cctype>
#include <stdexcept>
#include <algorithm>

class ExpressionParser {
private:
    std::string src;
    size_t pos = 0;

    char peek() const {
        if (pos < src.length()) return src[pos];
        return '\0';
    }

    char get() {
        if (pos < src.length()) return src[pos++];
        return '\0';
    }

    void skip_whitespace() {
        while (pos < src.length() && std::isspace(static_cast<unsigned char>(src[pos]))) {
            pos++;
        }
    }

    double expression();
    double term();
    double factor();
    double primary();
    double number();
    std::string name();

public:
    explicit ExpressionParser(std::string str) : src(std::move(str)), pos(0) {}

    double parse() {
        pos = 0;
        double res = expression();
        skip_whitespace();
        if (pos < src.length()) {
            throw std::runtime_error("Unexpected character: '" + std::string(1, src[pos]) + "'");
        }
        return res;
    }
};

// Handles addition and subtraction
double ExpressionParser::expression() {
    double left = term();
    while (true) {
        skip_whitespace();
        char op = peek();
        if (op == '+' || op == '-') {
            get();
            double right = term();
            if (op == '+') left += right;
            else left -= right;
        } else {
            break;
        }
    }
    return left;
}

// Handles multiplication and division
double ExpressionParser::term() {
    double left = factor();
    while (true) {
        skip_whitespace();
        char op = peek();
        if (op == '*' || op == '/') {
            get();
            double right = factor();
            if (op == '*') {
                left *= right;
            } else {
                if (right == 0.0) throw std::runtime_error("Division by zero");
                left /= right;
            }
        } else {
            break;
        }
    }
    return left;
}

// Handles exponentiation (right-associative)
double ExpressionParser::factor() {
    double left = primary();
    skip_whitespace();
    if (peek() == '^') {
        get();
        double right = factor();
        left = std::pow(left, right);
    }
    return left;
}

// Handles numbers, parentheses, functions, and unary operators
double ExpressionParser::primary() {
    skip_whitespace();
    char c = peek();
    if (c == '\0') {
        throw std::runtime_error("Unexpected end of expression");
    }

    // Unary plus and minus
    if (c == '+') {
        get();
        return primary();
    }
    if (c == '-') {
        get();
        return -primary();
    }

    // Parentheses
    if (c == '(') {
        get();
        double val = expression();
        skip_whitespace();
        if (get() != ')') {
            throw std::runtime_error("Missing closing parenthesis");
        }
        return val;
    }

    // Numbers
    if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
        return number();
    }

    // Functions and Constants
    if (std::isalpha(static_cast<unsigned char>(c))) {
        std::string id = name();
        skip_whitespace();
        
        if (peek() == '(') {
            get(); // consume '('
            double arg = expression();
            skip_whitespace();
            if (get() != ')') {
                throw std::runtime_error("Missing closing parenthesis for function '" + id + "'");
            }
            
            // Normalize function names to lowercase
            std::transform(id.begin(), id.end(), id.begin(), [](unsigned char c){ return std::tolower(c); });

            if (id == "sin") return std::sin(arg);
            if (id == "cos") return std::cos(arg);
            if (id == "tan") return std::tan(arg);
            if (id == "asin") return std::asin(arg);
            if (id == "acos") return std::acos(arg);
            if (id == "atan") return std::atan(arg);
            if (id == "sqrt") {
                if (arg < 0) throw std::runtime_error("Square root of negative number");
                return std::sqrt(arg);
            }
            if (id == "log") {
                if (arg <= 0) throw std::runtime_error("Logarithm (base 10) of non-positive number");
                return std::log10(arg);
            }
            if (id == "ln") {
                if (arg <= 0) throw std::runtime_error("Natural logarithm of non-positive number");
                return std::log(arg);
            }
            if (id == "abs") return std::abs(arg);
            if (id == "exp") return std::exp(arg);

            throw std::runtime_error("Unknown function: '" + id + "'");
        } else {
            // Check for constants
            std::transform(id.begin(), id.end(), id.begin(), [](unsigned char c){ return std::tolower(c); });
            if (id == "pi") return 3.14159265358979323846;
            if (id == "e") return 2.71828182845904523536;
            throw std::runtime_error("Unknown constant: '" + id + "'");
        }
    }

    throw std::runtime_error("Unexpected character: '" + std::string(1, c) + "'");
}

double ExpressionParser::number() {
    size_t start = pos;
    bool has_dot = false;
    while (pos < src.length()) {
        char c = src[pos];
        if (std::isdigit(static_cast<unsigned char>(c))) {
            pos++;
        } else if (c == '.') {
            if (has_dot) break;
            has_dot = true;
            pos++;
        } else {
            break;
        }
    }
    if (start == pos) {
        throw std::runtime_error("Expected a number");
    }
    return std::stod(src.substr(start, pos - start));
}

std::string ExpressionParser::name() {
    size_t start = pos;
    while (pos < src.length() && std::isalpha(static_cast<unsigned char>(src[pos]))) {
        pos++;
    }
    return src.substr(start, pos - start);
}

void show_help() {
    std::cout << "\n";

    std::cout << "Operators:   +, -, *, /, ^ (exponentiation)\n";
    std::cout << "Grouping:    ( and )\n";
    std::cout << "Constants:   pi, e\n";
    std::cout << "Functions:   sin(x), cos(x), tan(x), asin(x), acos(x), atan(x)\n";
    std::cout << "             sqrt(x), log(x) [base 10], ln(x), abs(x), exp(x)\n";
    std::cout << "             (Trig functions use radians)\n";
    std::cout << "Commands:    'exit' or 'quit' to close the program\n";
    std::cout << "             'help' to display this message\n";
    std::cout << "\n\n";
}

int main() {
    show_help();
    std::string input;

    while (true) {
        std::cout << "bc> ";
        if (!std::getline(std::cin, input)) {
            break;
        }

        // Remove leading/trailing spaces for standard command checks
        std::string command = input;
        command.erase(command.begin(), std::find_if(command.begin(), command.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        command.erase(std::find_if(command.rbegin(), command.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), command.end());

        if (command.empty()) {
            continue;
        }

        if (command == "exit" || command == "quit") {
            break;
        }

        if (command == "help") {
            show_help();
            continue;
        }

        try {
            ExpressionParser parser(input);
            double result = parser.parse();
            std::cout << "= " << result << "\n\n";
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n\n";
        }
    }

    return 0;
}