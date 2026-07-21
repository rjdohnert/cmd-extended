#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>
#include <cstdint>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <fcntl.h>
#include <io.h>

// ============================================================================
// Self-Contained SHA-256 Implementation (FIPS 180-4)
// ============================================================================
class SHA256 {
public:
    SHA256() { init(); }

    void init() {
        m_state[0] = 0x6a09e667;
        m_state[1] = 0xbb67ae85;
        m_state[2] = 0x3c6ef372;
        m_state[3] = 0xa54ff53a;
        m_state[4] = 0x510e527f;
        m_state[5] = 0x9b05688c;
        m_state[6] = 0x1f83d9ab;
        m_state[7] = 0x5be0cd19;
        m_count = 0;
        m_buffer_len = 0;
    }

    void update(const uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; ++i) {
            m_buffer[m_buffer_len++] = data[i];
            if (m_buffer_len == 64) {
                transform(m_buffer);
                m_count += 512;
                m_buffer_len = 0;
            }
        }
    }

    std::string finalize() {
        uint64_t total_bits = m_count + m_buffer_len * 8;
        m_buffer[m_buffer_len++] = 0x80;

        if (m_buffer_len > 56) {
            while (m_buffer_len < 64) {
                m_buffer[m_buffer_len++] = 0x00;
            }
            transform(m_buffer);
            m_buffer_len = 0;
        }

        while (m_buffer_len < 56) {
            m_buffer[m_buffer_len++] = 0x00;
        }

        // Big-endian 64-bit total length in bits
        for (int i = 7; i >= 0; --i) {
            m_buffer[56 + (7 - i)] = static_cast<uint8_t>((total_bits >> (i * 8)) & 0xFF);
        }
        transform(m_buffer);

        std::ostringstream ss;
        for (int i = 0; i < 8; ++i) {
            ss << std::hex << std::setw(8) << std::setfill('0') << m_state[i];
        }
        return ss.str();
    }

private:
    uint32_t m_state[8];
    uint64_t m_count;
    uint8_t m_buffer[64];
    size_t m_buffer_len;

    static inline uint32_t rotr(uint32_t x, uint32_t n) {
        return (x >> n) | (x << (32 - n));
    }

    static inline uint32_t choose(uint32_t e, uint32_t f, uint32_t g) {
        return (e & f) ^ (~e & g);
    }

    static inline uint32_t majority(uint32_t a, uint32_t b, uint32_t c) {
        return (a & b) ^ (a & c) ^ (b & c);
    }

    static inline uint32_t sig0(uint32_t x) {
        return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
    }

    static inline uint32_t sig1(uint32_t x) {
        return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
    }

    static inline uint32_t sub0(uint32_t x) {
        return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
    }

    static inline uint32_t sub1(uint32_t x) {
        return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
    }

    static const uint32_t K[64];

    void transform(const uint8_t chunk[64]) {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(chunk[i * 4]) << 24) |
                   (static_cast<uint32_t>(chunk[i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(chunk[i * 4 + 2]) << 8) |
                   (static_cast<uint32_t>(chunk[i * 4 + 3]));
        }
        for (int i = 16; i < 64; ++i) {
            w[i] = sub1(w[i - 2]) + w[i - 7] + sub0(w[i - 15]) + w[i - 16];
        }

        uint32_t a = m_state[0], b = m_state[1], c = m_state[2], d = m_state[3];
        uint32_t e = m_state[4], f = m_state[5], g = m_state[6], h = m_state[7];

        for (int i = 0; i < 64; ++i) {
            uint32_t t1 = h + sig1(e) + choose(e, f, g) + K[i] + w[i];
            uint32_t t2 = sig0(a) + majority(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        m_state[0] += a; m_state[1] += b; m_state[2] += c; m_state[3] += d;
        m_state[4] += e; m_state[5] += f; m_state[6] += g; m_state[7] += h;
    }
};

const uint32_t SHA256::K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

// ============================================================================
// Application Logic
// ============================================================================

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [OPTION]... [FILE]...\n"
              << "Print or check SHA256 (256-bit) checksums.\n\n"
              << "Options:\n"
              << "  -b, --binary         read in binary mode (default on Windows)\n"
              << "  -t, --text           read in text mode\n"
              << "  -c, --check          read SHA256 sums from the FILEs and check them\n"
              << "      --status         don't output anything, status code shows success\n"
              << "  -q, --quiet          don't print OK for each successfully verified file\n"
              << "  -w, --warn           warn about improperly formatted checksum lines\n"
              << "  -h, --help           display this help and exit\n\n"
              << "With no FILE, or when FILE is -, read standard input.\n";
}

std::string compute_sha256_stream(std::istream& is) {
    SHA256 sha;
    char buffer[65536];
    while (is.read(buffer, sizeof(buffer)) || is.gcount() > 0) {
        sha.update(reinterpret_cast<const uint8_t*>(buffer), static_cast<size_t>(is.gcount()));
    }
    return sha.finalize();
}

std::string compute_sha256_file(const std::string& filepath, bool binary_mode, bool& error) {
    error = false;
    if (filepath == "-") {
        _setmode(_fileno(stdin), binary_mode ? _O_BINARY : _O_TEXT);
        return compute_sha256_stream(std::cin);
    }

    std::ios_base::openmode mode = std::ios::in;
    if (binary_mode) mode |= std::ios::binary;

    std::ifstream file(filepath, mode);
    if (!file.is_open()) {
        error = true;
        return "";
    }

    return compute_sha256_stream(file);
}

bool parse_checksum_line(const std::string& line, std::string& expected_hash, bool& is_binary, std::string& filename) {
    if (line.length() < 66) return false;

    expected_hash = line.substr(0, 64);
    for (char c : expected_hash) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
    }

    size_t idx = 64;
    if (line[idx] != ' ' && line[idx] != '\t') return false;

    idx++;
    if (idx >= line.length()) return false;

    if (line[idx] == '*') {
        is_binary = true;
        idx++;
    } else if (line[idx] == ' ') {
        is_binary = false;
        idx++;
    } else {
        is_binary = false;
    }

    if (idx >= line.length()) return false;
    filename = line.substr(idx);

    if (!filename.empty() && filename.back() == '\r') {
        filename.pop_back();
    }

    return true;
}

int check_mode(const std::vector<std::string>& files, bool default_binary, bool quiet, bool status, bool warn) {
    size_t read_failures = 0;
    size_t checksum_mismatches = 0;
    size_t format_errors = 0;
    size_t total_checked = 0;

    for (const auto& file : files) {
        std::istream* in_stream = &std::cin;
        std::ifstream infile;

        if (file != "-") {
            infile.open(file);
            if (!infile.is_open()) {
                if (!status) {
                    std::cerr << "sha256sum: " << file << ": No such file or directory\n";
                }
                read_failures++;
                continue;
            }
            in_stream = &infile;
        }

        std::string line;
        size_t line_num = 0;

        while (std::getline(*in_stream, line)) {
            line_num++;
            if (line.empty() || line[0] == '#') continue;

            std::string expected_hash, filename;
            bool line_is_binary = default_binary;

            if (!parse_checksum_line(line, expected_hash, line_is_binary, filename)) {
                format_errors++;
                if (warn && !status) {
                    std::cerr << "sha256sum: " << file << ": " << line_num << ": improperly formatted SHA256 checksum line\n";
                }
                continue;
            }

            bool err = false;
            std::string actual_hash = compute_sha256_file(filename, line_is_binary, err);

            total_checked++;

            if (err) {
                read_failures++;
                if (!status) {
                    std::cout << filename << ": FAILED open or read\n";
                }
            } else {
                std::string exp_lower = expected_hash;
                std::transform(exp_lower.begin(), exp_lower.end(), exp_lower.begin(), ::tolower);

                if (actual_hash == exp_lower) {
                    if (!status && !quiet) {
                        std::cout << filename << ": OK\n";
                    }
                } else {
                    checksum_mismatches++;
                    if (!status) {
                        std::cout << filename << ": FAILED\n";
                    }
                }
            }
        }
    }

    if (!status) {
        if (format_errors > 0 && !warn) {
            std::cerr << "sha256sum: WARNING: " << format_errors << " line(s) improperly formatted\n";
        }
        if (read_failures > 0) {
            std::cerr << "sha256sum: WARNING: " << read_failures << " listed file(s) could not be read\n";
        }
        if (checksum_mismatches > 0) {
            std::cerr << "sha256sum: WARNING: " << checksum_mismatches << " computed checksum(s) did NOT match\n";
        }
    }

    return (checksum_mismatches > 0 || read_failures > 0 || total_checked == 0) ? 1 : 0;
}

int main(int argc, char* argv[]) {
    bool binary_mode = true; // Default to binary mode on Windows
    bool do_check = false;
    bool quiet = false;
    bool status = false;
    bool warn = false;

    std::vector<std::string> files;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help" || arg == "/?") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-b" || arg == "--binary") {
            binary_mode = true;
        } else if (arg == "-t" || arg == "--text") {
            binary_mode = false;
        } else if (arg == "-c" || arg == "--check") {
            do_check = true;
        } else if (arg == "--status") {
            status = true;
        } else if (arg == "-q" || arg == "--quiet") {
            quiet = true;
        } else if (arg == "-w" || arg == "--warn") {
            warn = true;
        } else if (!arg.empty() && arg[0] == '-' && arg != "-") {
            std::cerr << "sha256sum: invalid option '" << arg << "'\n";
            return 1;
        } else {
            files.push_back(arg);
        }
    }

    if (files.empty()) {
        files.push_back("-");
    }

    if (do_check) {
        return check_mode(files, binary_mode, quiet, status, warn);
    }

    int exit_code = 0;

    for (const auto& file : files) {
        bool err = false;
        std::string hash = compute_sha256_file(file, binary_mode, err);

        if (err) {
            std::cerr << "sha256sum: " << file << ": No such file or directory\n";
            exit_code = 1;
        } else {
            std::cout << hash << " " << (binary_mode ? "*" : " ") << file << "\n";
        }
    }

    return exit_code;
}