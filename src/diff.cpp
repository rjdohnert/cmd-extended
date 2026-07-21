#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

// Edit operations
enum Op { Match, Delete, Insert };

struct Step {
    Op op;
    int line_old; // 0-based index in File A (or -1)
    int line_new; // 0-based index in File B (or -1)
};

struct StepInfo {
    Step step;
    int line_a; // lines of A processed before this step
    int line_b; // lines of B processed before this step
};

struct Hunk {
    int a_start, a_end;
    int b_start, b_end;
    std::vector<int> deleted_lines;
    std::vector<int> inserted_lines;
};

// Help / usage information
void print_usage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " [-i] [-w] [-b] [-u] <file1> <file2>\n";
    std::cerr << "Options:\n";
    std::cerr << "  -i    Case-insensitive comparison.\n";
    std::cerr << "  -w    Ignore all white space.\n";
    std::cerr << "  -b    Ignore space change (ignore trailing whitespace, treat multiple spaces as one).\n";
    std::cerr << "  -u    Output in Unified Diff format.\n";
}

// Strip CRLF and read file into a vector of lines
std::vector<std::string> read_file(const std::string& filepath, bool& success) {
    std::vector<std::string> lines;
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        success = false;
        return lines;
    }
    success = true;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    return lines;
}

// Line normalization based on formatting flags
std::string normalize_line(const std::string& s, bool ignore_case, bool ignore_all_space, bool ignore_space_change) {
    std::string res;
    bool in_space = false;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (ignore_all_space) {
                continue;
            }
            if (ignore_space_change) {
                if (!in_space) {
                    res.push_back(' ');
                    in_space = true;
                }
                continue;
            }
        } else {
            in_space = false;
        }
        if (ignore_case) {
            res.push_back(std::tolower(static_cast<unsigned char>(c)));
        } else {
            res.push_back(c);
        }
    }
    if (ignore_space_change && !res.empty() && res.back() == ' ') {
        res.pop_back();
    }
    if (ignore_space_change && !res.empty() && res.front() == ' ') {
        res.erase(res.begin());
    }
    return res;
}

// Print a normal format hunk
void print_normal_hunk(const Hunk& hunk, const std::vector<std::string>& A, const std::vector<std::string>& B) {
    if (hunk.a_end > 0 && hunk.b_end > 0) {
        if (hunk.a_start == hunk.a_end) std::cout << hunk.a_start;
        else std::cout << hunk.a_start << "," << hunk.a_end;
        std::cout << "c";
        if (hunk.b_start == hunk.b_end) std::cout << hunk.b_start;
        else std::cout << hunk.b_start << "," << hunk.b_end;
        std::cout << "\n";
    } else if (hunk.a_end > 0) {
        if (hunk.a_start == hunk.a_end) std::cout << hunk.a_start;
        else std::cout << hunk.a_start << "," << hunk.a_end;
        std::cout << "d" << hunk.b_start << "\n";
    } else {
        std::cout << hunk.a_start << "a";
        if (hunk.b_start == hunk.b_end) std::cout << hunk.b_start;
        else std::cout << hunk.b_start << "," << hunk.b_end;
        std::cout << "\n";
    }

    for (int idx : hunk.deleted_lines) {
        std::cout << "< " << A[idx] << "\n";
    }
    if (hunk.a_end > 0 && hunk.b_end > 0) {
        std::cout << "---\n";
    }
    for (int idx : hunk.inserted_lines) {
        std::cout << "> " << B[idx] << "\n";
    }
}

// Print a unified format hunk
void print_unified_hunk(const std::vector<StepInfo>& step_infos, size_t hunk_start, size_t hunk_end,
                        const std::vector<std::string>& A, const std::vector<std::string>& B) {
    int s_a = 0, s_b = 0;
    int l_a = 0, l_b = 0;
    bool found_a = false, found_b = false;

    for (size_t j = hunk_start; j <= hunk_end; ++j) {
        const auto& info = step_infos[j];
        if (info.step.op == Match || info.step.op == Delete) {
            s_a++;
            if (!found_a) {
                l_a = info.line_a + 1;
                found_a = true;
            }
        }
        if (info.step.op == Match || info.step.op == Insert) {
            s_b++;
            if (!found_b) {
                l_b = info.line_b + 1;
                found_b = true;
            }
        }
    }

    if (!found_a) l_a = step_infos[hunk_start].line_a;
    if (!found_b) l_b = step_infos[hunk_start].line_b;

    std::cout << "@@ -" << l_a;
    if (s_a != 1) std::cout << "," << s_a;
    std::cout << " +" << l_b;
    if (s_b != 1) std::cout << "," << s_b;
    std::cout << " @@\n";

    for (size_t j = hunk_start; j <= hunk_end; ++j) {
        const auto& info = step_infos[j];
        if (info.step.op == Match) {
            std::cout << " " << A[info.step.line_old] << "\n";
        } else if (info.step.op == Delete) {
            std::cout << "-" << A[info.step.line_old] << "\n";
        } else if (info.step.op == Insert) {
            std::cout << "+" << B[info.step.line_new] << "\n";
        }
    }
}

int main(int argc, char* argv[]) {
    // Optimize Standard I/O operations
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    bool ignore_case = false;
    bool ignore_all_space = false;
    bool ignore_space_change = false;
    bool unified_format = false;
    std::vector<std::string> targets;

    // Command-line argument parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg[0] == '-' && arg.size() > 1) {
            for (size_t j = 1; j < arg.size(); ++j) {
                char opt = arg[j];
                if (opt == 'i') ignore_case = true;
                else if (opt == 'w') ignore_all_space = true;
                else if (opt == 'b') ignore_space_change = true;
                else if (opt == 'u') unified_format = true;
                else {
                    std::cerr << "diff: unknown option -- " << opt << "\n";
                    print_usage(argv[0]);
                    return 2;
                }
            }
        } else {
            targets.push_back(arg);
        }
    }

    if (targets.size() != 2) {
        print_usage(argv[0]);
        return 2;
    }

    std::string file1_path = targets[0];
    std::string file2_path = targets[1];

    bool success1 = false, success2 = false;
    std::vector<std::string> A = read_file(file1_path, success1);
    std::vector<std::string> B = read_file(file2_path, success2);

    if (!success1) {
        std::cerr << "diff: " << file1_path << ": No such file or directory\n";
        return 2;
    }
    if (!success2) {
        std::cerr << "diff: " << file2_path << ": No such file or directory\n";
        return 2;
    }

    int N = A.size();
    int M = B.size();

    // Pre-normalize vectors to accelerate comparisons during Myers search
    std::vector<std::string> norm_A(N);
    std::vector<std::string> norm_B(M);
    for (int i = 0; i < N; ++i) norm_A[i] = normalize_line(A[i], ignore_case, ignore_all_space, ignore_space_change);
    for (int i = 0; i < M; ++i) norm_B[i] = normalize_line(B[i], ignore_case, ignore_all_space, ignore_space_change);

    std::vector<Step> steps;
    bool found = false;

    // Dynamic threshold limit for the Myers algorithm to prevent high execution times
    const int cap = 10000;
    int max_d = (N + M > cap) ? cap : (N + M);

    std::vector<std::vector<int>> history;
    int offset = N;
    std::vector<int> V(N + M + 1, -1);
    V[1 + offset] = 0;

    for (int d = 0; d <= max_d; ++d) {
        history.push_back(V);
        for (int k = -d; k <= d; k += 2) {
            if (k < -N || k > M) continue;

            int x;
            bool move_down = (k == -d || (k != d && V[k - 1 + offset] < V[k + 1 + offset]));
            if (move_down) {
                x = V[k + 1 + offset];
            } else {
                x = V[k - 1 + offset] + 1;
            }
            int y = x - k;

            while (x < N && y < M && norm_A[x] == norm_B[y]) {
                x++;
                y++;
            }

            V[k + offset] = x;

            if (x >= N && y >= M) {
                found = true;
                break;
            }
        }
        if (found) break;
    }

    if (found) {
        // Reconstruct the shortest edit script path (traceback)
        std::vector<Step> r_steps;
        int x = N, y = M;
        for (int d = (int)history.size() - 1; d > 0; --d) {
            int k = x - y;
            const auto& V_prev = history[d];

            int k_prev = (k == -d || (k != d && V_prev[k - 1 + offset] < V_prev[k + 1 + offset])) ? k + 1 : k - 1;
            int x_prev = V_prev[k_prev + offset];
            int y_prev = x_prev - k_prev;

            int x_edit = (k_prev > k) ? x_prev : x_prev + 1;
            int y_edit = x_edit - k;

            while (x > x_edit && y > y_edit) {
                r_steps.push_back({Match, x - 1, y - 1});
                x--;
                y--;
            }

            if (k_prev > k) {
                r_steps.push_back({Insert, -1, y - 1});
                y--;
            } else {
                r_steps.push_back({Delete, x - 1, -1});
                x--;
            }
        }
        while (x > 0 && y > 0) {
            r_steps.push_back({Match, x - 1, y - 1});
            x--;
            y--;
        }
        std::reverse(r_steps.begin(), r_steps.end());
        steps = r_steps;
    } else {
        // Fallback option for completely different or massive files: sequential match
        int x = 0, y = 0;
        while (x < N && y < M) {
            if (norm_A[x] == norm_B[y]) {
                steps.push_back({Match, x, y});
                x++;
                y++;
            } else {
                break;
            }
        }
        while (x < N) {
            steps.push_back({Delete, x, -1});
            x++;
        }
        while (y < M) {
            steps.push_back({Insert, -1, y});
            y++;
        }
    }

    // Check if files are identical
    bool has_differences = false;
    for (const auto& step : steps) {
        if (step.op != Match) {
            has_differences = true;
            break;
        }
    }

    if (!has_differences) {
        return 0; // Standard exit status: files are identical
    }

    // Output formatting
    if (unified_format) {
        std::cout << "--- " << file1_path << "\n";
        std::cout << "+++ " << file2_path << "\n";

        // Generate StepInfo helper to track original line mappings
        std::vector<StepInfo> step_infos;
        int curr_a = 0, curr_b = 0;
        for (const auto& step : steps) {
            step_infos.push_back({step, curr_a, curr_b});
            if (step.op == Match) {
                curr_a++; curr_b++;
            } else if (step.op == Delete) {
                curr_a++;
            } else if (step.op == Insert) {
                curr_b++;
            }
        }

        // Gather all change indexes
        std::vector<size_t> change_indices;
        for (size_t i = 0; i < step_infos.size(); ++i) {
            if (step_infos[i].step.op != Match) {
                change_indices.push_back(i);
            }
        }

        // Group changes within standard unified context scope
        const int context = 3;
        std::vector<std::vector<size_t>> clusters;
        if (!change_indices.empty()) {
            std::vector<size_t> current_cluster = {change_indices[0]};
            for (size_t idx = 1; idx < change_indices.size(); ++idx) {
                size_t prev_idx = change_indices[idx - 1];
                size_t curr_idx = change_indices[idx];
                if (curr_idx - prev_idx - 1 <= 2 * context) {
                    current_cluster.push_back(curr_idx);
                } else {
                    clusters.push_back(current_cluster);
                    current_cluster = {curr_idx};
                }
            }
            clusters.push_back(current_cluster);
        }

        for (const auto& cluster : clusters) {
            size_t start_idx = cluster.front();
            size_t end_idx = cluster.back();
            size_t hunk_start = (start_idx >= context) ? start_idx - context : 0;
            size_t hunk_end = (end_idx + context < step_infos.size()) ? end_idx + context : step_infos.size() - 1;

            print_unified_hunk(step_infos, hunk_start, hunk_end, A, B);
        }
    } else {
        // Group steps into standard Normal Diff format hunks
        std::vector<Hunk> hunks;
        int line_a = 0;
        int line_b = 0;
        size_t i = 0;

        while (i < steps.size()) {
            if (steps[i].op == Match) {
                line_a++;
                line_b++;
                i++;
            } else {
                Hunk hunk;
                hunk.a_start = 0; hunk.a_end = 0;
                hunk.b_start = 0; hunk.b_end = 0;

                int block_start_line_a = line_a;
                int block_start_line_b = line_b;

                while (i < steps.size() && steps[i].op != Match) {
                    if (steps[i].op == Delete) {
                        hunk.deleted_lines.push_back(steps[i].line_old);
                        line_a++;
                    } else if (steps[i].op == Insert) {
                        hunk.inserted_lines.push_back(steps[i].line_new);
                        line_b++;
                    }
                    i++;
                }

                if (!hunk.deleted_lines.empty() && !hunk.inserted_lines.empty()) {
                    hunk.a_start = block_start_line_a + 1;
                    hunk.a_end = line_a;
                    hunk.b_start = block_start_line_b + 1;
                    hunk.b_end = line_b;
                } else if (!hunk.deleted_lines.empty()) {
                    hunk.a_start = block_start_line_a + 1;
                    hunk.a_end = line_a;
                    hunk.b_start = block_start_line_b;
                    hunk.b_end = 0;
                } else if (!hunk.inserted_lines.empty()) {
                    hunk.a_start = block_start_line_a;
                    hunk.a_end = 0;
                    hunk.b_start = block_start_line_b + 1;
                    hunk.b_end = line_b;
                }
                hunks.push_back(hunk);
            }
        }

        for (const auto& hunk : hunks) {
            print_normal_hunk(hunk, A, B);
        }
    }

    return 1; // Standard exit status: differences exist
}