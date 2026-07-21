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
// Self-Contained MD5 Implementation (RFC 1321)
// ============================================================================
class MD5 {
public:
    MD5() { init(); }

    void init() {
        count[0] = count[1] = 0;
        state[0] = 0x67452301;
        state[1] = 0xefcdab89;
        state[2] = 0x98badcfe;
        state[3] = 0x10325476;
    }

    void update(const uint8_t* input, size_t inputLen) {
        size_t i, index, partLen;
        index = (size_t)((count[0] >> 3) & 0x3F);
        if ((count[0] += ((uint32_t)inputLen << 3)) < ((uint32_t)inputLen << 3)) {
            count[1]++;
        }
        count[1] += ((uint32_t)inputLen >> 29);

        partLen = 64 - index;

        if (inputLen >= partLen) {
            std::memcpy(&buffer[index], input, partLen);
            transform(buffer);

            for (i = partLen; i + 63 < inputLen; i += 64) {
                transform(&input[i]);
            }
            index = 0;
        } else {
            i = 0;
        }

        std::memcpy(&buffer[index], &input[i], inputLen - i);
    }

    std::string finalize() {
        uint8_t bits[8];
        encode(bits, count, 8);

        size_t index = (size_t)((count[0] >> 3) & 0x3f);
        size_t padLen = (index < 56) ? (56 - index) : (120 - index);

        static const uint8_t PADDING[64] = { 0x80 };
        update(PADDING, padLen);
        update(bits, 8);

        uint8_t digest[16];
        encode(digest, state, 16);

        std::ostringstream ss;
        for (int i = 0; i < 16; ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
        }
        return ss.str();
    }

private:
    uint32_t state[4];
    uint32_t count[2];
    uint8_t buffer[64];

    static inline uint32_t F(uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (~x & z); }
    static inline uint32_t G(uint32_t x, uint32_t y, uint32_t z) { return (x & z) | (y & ~z); }
    static inline uint32_t H(uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z; }
    static inline uint32_t I(uint32_t x, uint32_t y, uint32_t z) { return y ^ (x | ~z); }

    static inline uint32_t rotate_left(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

    static inline void FF(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac) {
        a = rotate_left(a + F(b, c, d) + x + ac, s) + b;
    }
    static inline void GG(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac) {
        a = rotate_left(a + G(b, c, d) + x + ac, s) + b;
    }
    static inline void HH(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac) {
        a = rotate_left(a + H(b, c, d) + x + ac, s) + b;
    }
    static inline void II(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac) {
        a = rotate_left(a + I(b, c, d) + x + ac, s) + b;
    }

    void decode(uint32_t* output, const uint8_t* input, size_t len) {
        for (size_t i = 0, j = 0; j < len; i++, j += 4) {
            output[i] = ((uint32_t)input[j]) | (((uint32_t)input[j+1]) << 8) |
                        (((uint32_t)input[j+2]) << 16) | (((uint32_t)input[j+3]) << 24);
        }
    }

    void encode(uint8_t* output, const uint32_t* input, size_t len) {
        for (size_t i = 0, j = 0; j < len; i++, j += 4) {
            output[j] = (uint8_t)(input[i] & 0xff);
            output[j+1] = (uint8_t)((input[i] >> 8) & 0xff);
            output[j+2] = (uint8_t)((input[i] >> 16) & 0xff);
            output[j+3] = (uint8_t)((input[i] >> 24) & 0xff);
        }
    }

    void transform(const uint8_t block[64]) {
        uint32_t a = state[0], b = state[1], c = state[2], d = state[3], x[16];
        decode(x, block, 64);

        /* Round 1 */
        FF(a, b, c, d, x[ 0], 7, 0xd76aa478);
        FF(d, a, b, c, x[ 1], 12, 0xe8c7b756);
        FF(c, d, a, b, x[ 2], 17, 0x242070db);
        FF(b, c, d, a, x[ 3], 22, 0xc1bdceee);
        FF(a, b, c, d, x[ 4], 7, 0xf57c0faf);
        FF(d, a, b, c, x[ 5], 12, 0x4787c62a);
        FF(c, d, a, b, x[ 6], 17, 0xa8304613);
        FF(b, c, d, a, x[ 7], 22, 0xfd469501);
        FF(a, b, c, d, x[ 8], 7, 0x698098d8);
        FF(d, a, b, c, x[ 9], 12, 0x8b44f7af);
        FF(c, d, a, b, x[10], 17, 0xffff5bb1);
        FF(b, c, d, a, x[11], 22, 0x895cd7be);
        FF(a, b, c, d, x[12], 7, 0x6b901122);
        FF(d, a, b, c, x[13], 12, 0xfd987193);
        FF(c, d, a, b, x[14], 17, 0xa679438e);
        FF(b, c, d, a, x[15], 22, 0x49b40821);

        /* Round 2 */
        GG(a, b, c, d, x[ 1], 5, 0xf61e2562);
        GG(d, a, b, c, x[ 6], 9, 0xc040b340);
        GG(c, d, a, b, x[11], 14, 0x265e5a51);
        GG(b, c, d, a, x[ 0], 20, 0xe9b6c7aa);
        GG(a, b, c, d, x[ 5], 5, 0xd62f105d);
        GG(d, a, b, c, x[10], 9, 0x02441453);
        GG(c, d, a, b, x[15], 14, 0xd8a1e681);
        GG(b, c, d, a, x[ 4], 20, 0xe7d3fbc8);
        GG(a, b, c, d, x[ 9], 5, 0x21e1cde6);
        GG(d, a, b, c, x[14], 9, 0xc33707d6);
        GG(c, d, a, b, x[ 3], 14, 0xf4d50d87);
        GG(b, c, d, a, x[ 8], 20, 0x455a14ed);
        GG(a, b, c, d, x[13], 5, 0xa9e3e905);
        GG(d, a, b, c, x[ 2], 9, 0xfcefa3f8);
        GG(c, d, a, b, x[ 7], 14, 0x676f02d9);
        GG(b, c, d, a, x[12], 20, 0x8d2a4c8a);

        /* Round 3 */
        HH(a, b, c, d, x[ 5], 4, 0xfffa3942);
        HH(d, a, b, c, x[ 8], 11, 0x8771f681);
        HH(c, d, a, b, x[11], 16, 0x6d9d6122);
        HH(b, c, d, a, x[14], 23, 0xfde5380c);
        HH(a, b, c, d, x[ 1], 4, 0xa4beea44);
        HH(d, a, b, c, x[ 4], 11, 0x4bdecfa9);
        HH(c, d, a, b, x[ 7], 16, 0xf6bb4b60);
        HH(b, c, d, a, x[10], 23, 0xbebfbc70);
        HH(a, b, c, d, x[13], 4, 0x289b7ec6);
        HH(d, a, b, c, x[ 0], 11, 0xeaa127fa);
        HH(c, d, a, b, x[ 3], 16, 0xd4ef3085);
        HH(b, c, d, a, x[ 6], 23, 0x04881d05);
        HH(a, b, c, d, x[ 9], 4, 0xd9d4d039);
        HH(d, a, b, c, x[12], 11, 0xe6db99e5);
        HH(c, d, a, b, x[15], 16, 0x1fa27cf8);
        HH(b, c, d, a, x[14], 23, 0xc4ac5665);

        /* Round 4 */
        II(a, b, c, d, x[ 0], 6, 0xf4292244);
        II(d, a, b, c, x[ 7], 10, 0x432aff97);
        II(c, d, a, b, x[14], 15, 0xab9423a7);
        II(b, c, d, a, x[ 5], 21, 0xfc93a039);
        II(a, b, c, d, x[12], 6, 0x655b59c3);
        II(d, a, b, c, x[ 3], 10, 0x8f0ccc92);
        II(c, d, a, b, x[10], 15, 0xffeff47d);
        II(b, c, d, a, x[ 1], 21, 0x85845dd1);
        II(a, b, c, d, x[ 6], 6, 0x6fa87e4f);
        II(d, a, b, c, x[11], 10, 0xfe2ce6e0);
        II(c, d, a, b, x[ 4], 15, 0xa3014314);
        II(b, c, d, a, x[ 9], 21, 0x4e0811a1);
        II(a, b, c, d, x[14], 6, 0xf7537e82);
        II(d, a, b, c, x[ 5], 10, 0xbd3af235);
        II(c, d, a, b, x[12], 15, 0x2ad7d2bb);
        II(b, c, d, a, x[ 3], 21, 0xeb86d391);

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
    }
};

// ============================================================================
// Helper Functions & Application Logic
// ============================================================================

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [OPTION]... [FILE]...\n"
              << "Print or check MD5 (128-bit) checksums.\n\n"
              << "Options:\n"
              << "  -b, --binary         read in binary mode (default on Windows)\n"
              << "  -t, --text           read in text mode\n"
              << "  -c, --check          read MD5 sums from the FILEs and check them\n"
              << "      --status         don't output anything, status code shows success\n"
              << "  -q, --quiet          don't print OK for each successfully verified file\n"
              << "  -w, --warn           warn about improperly formatted checksum lines\n"
              << "  -h, --help           display this help and exit\n\n"
              << "With no FILE, or when FILE is -, read standard input.\n";
}

std::string compute_md5_stream(std::istream& is) {
    MD5 md5;
    char buffer[65536];
    while (is.read(buffer, sizeof(buffer)) || is.gcount() > 0) {
        md5.update(reinterpret_cast<const uint8_t*>(buffer), static_cast<size_t>(is.gcount()));
    }
    return md5.finalize();
}

std::string compute_md5_file(const std::string& filepath, bool binary_mode, bool& error) {
    error = false;
    if (filepath == "-") {
        _setmode(_fileno(stdin), binary_mode ? _O_BINARY : _O_TEXT);
        return compute_md5_stream(std::cin);
    }

    std::ios_base::openmode mode = std::ios::in;
    if (binary_mode) mode |= std::ios::binary;

    std::ifstream file(filepath, mode);
    if (!file.is_open()) {
        error = true;
        return "";
    }

    return compute_md5_stream(file);
}

bool parse_checksum_line(const std::string& line, std::string& expected_hash, bool& is_binary, std::string& filename) {
    if (line.length() < 34) return false;

    expected_hash = line.substr(0, 32);
    for (char c : expected_hash) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
    }

    size_t idx = 32;
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
                    std::cerr << "md5sum: " << file << ": No such file or directory\n";
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
                    std::cerr << "md5sum: " << file << ": " << line_num << ": improperly formatted MD5 checksum line\n";
                }
                continue;
            }

            bool err = false;
            std::string actual_hash = compute_md5_file(filename, line_is_binary, err);

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
            std::cerr << "md5sum: WARNING: " << format_errors << " line(s) improperly formatted\n";
        }
        if (read_failures > 0) {
            std::cerr << "md5sum: WARNING: " << read_failures << " listed file(s) could not be read\n";
        }
        if (checksum_mismatches > 0) {
            std::cerr << "md5sum: WARNING: " << checksum_mismatches << " computed checksum(s) did NOT match\n";
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
            std::cerr << "md5sum: invalid option '" << arg << "'\n";
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
        std::string hash = compute_md5_file(file, binary_mode, err);

        if (err) {
            std::cerr << "md5sum: " << file << ": No such file or directory\n";
            exit_code = 1;
        } else {
            std::cout << hash << " " << (binary_mode ? "*" : " ") << file << "\n";
        }
    }

    return exit_code;
}