#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <unordered_set>
#include <queue>

using namespace std;

// Helper to register node names and assign integer IDs while preserving insertion order
int get_or_create_node_id(const string& name,
                           map<string, int>& name_to_id,
                           vector<string>& id_to_name) {
    auto it = name_to_id.find(name);
    if (it != name_to_id.end()) {
        return it->second;
    }
    int new_id = static_cast<int>(id_to_name.size());
    name_to_id[name] = new_id;
    id_to_name.push_back(name);
    return new_id;
}

void print_usage(const char* prog_name) {
    cout << "Usage: " << prog_name << " [FILE]\n"
         << "Write totally ordered list consistent with the partial ordering in FILE.\n"
         << "With no FILE, or when FILE is -, read standard input.\n\n"
         << "Options:\n"
         << "  -h, --help     Display this help message\n";
}

int main(int argc, char* argv[]) {
    string filename;

    // Parse options
    if (argc > 1) {
        string arg = argv[1];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
        if (arg != "-") {
            filename = arg;
        }
    }

    // Determine input stream
    ifstream file_stream;
    istream* in_stream = &cin;

    if (!filename.empty()) {
        file_stream.open(filename);
        if (!file_stream.is_open()) {
            cerr << "tsort: cannot open '" << filename << "' for reading\n";
            return 1;
        }
        in_stream = &file_stream;
    }

    // Read all tokens (pairs of items)
    vector<string> tokens;
    string token;
    while (*in_stream >> token) {
        tokens.push_back(token);
    }

    // Validate even number of items
    if (tokens.size() % 2 != 0) {
        cerr << "tsort: " << (filename.empty() ? "-" : filename) << ": input contains an odd number of tokens\n";
        return 1;
    }

    map<string, int> name_to_id;
    vector<string> id_to_name;

    // First pass: register all nodes in order of appearance
    for (const auto& tok : tokens) {
        get_or_create_node_id(tok, name_to_id, id_to_name);
    }

    int n = static_cast<int>(id_to_name.size());
    if (n == 0) {
        return 0; // Empty input
    }

    vector<int> in_degree(n, 0);
    vector<unordered_set<int>> adj(n);

    // Build directed graph (u -> v means u must precede v)
    for (size_t i = 0; i < tokens.size(); i += 2) {
        int u = get_or_create_node_id(tokens[i], name_to_id, id_to_name);
        int v = get_or_create_node_id(tokens[i + 1], name_to_id, id_to_name);

        if (u != v) { // Ignore self-loops
            // Add edge u -> v if not already present
            if (adj[u].find(v) == adj[u].end()) {
                adj[u].insert(v);
                in_degree[v]++;
            }
        }
    }

    // Queue for nodes with in-degree 0 (FIFO order preserves initial appearance preference)
    queue<int> q;
    vector<bool> visited(n, false);

    for (int i = 0; i < n; ++i) {
        if (in_degree[i] == 0) {
            q.push(i);
            visited[i] = true;
        }
    }

    int output_count = 0;

    // Kahn's Algorithm for Topological Sort
    while (output_count < n) {
        if (q.empty()) {
            // Cycle detected!
            cerr << "tsort: cycle in data\n";

            // Break cycle by picking the first unvisited node
            int cycle_node = -1;
            for (int i = 0; i < n; ++i) {
                if (!visited[i]) {
                    cycle_node = i;
                    break;
                }
            }

            if (cycle_node != -1) {
                q.push(cycle_node);
                visited[cycle_node] = true;
            } else {
                break;
            }
        }

        int u = q.front();
        q.pop();

        cout << id_to_name[u] << "\n";
        output_count++;

        for (int v : adj[u]) {
            in_degree[v]--;
            if (in_degree[v] <= 0 && !visited[v]) {
                q.push(v);
                visited[v] = true;
            }
        }
    }

    return 0;
}