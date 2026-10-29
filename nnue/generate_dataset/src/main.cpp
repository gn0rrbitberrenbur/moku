#include "../include/nnue.hpp"
#include <iostream>
#include <string>

/**
 * Prints the command-line usage for the dataset generator.
 * @return void
 */
static void print_usage() {
    std::cout <<
        "nnue_gen - Gomoku NNUE Dataset Generator\n"
        "  -n, --positions <N>  target number of positions (default 1000000)\n"
        "  -s, --size <N>       board size 5..19 (default 15)\n"
        "  -d, --depth <N>      maximum search depth (default 12)\n"
        "  -t, --time <ms>      search time per move in ms (default 20000)\n"
        "  -c, --cores <N>      threads (default: all cores)\n"
        "  -O, --opening <N>    max. random opening plies (default 8, 0=off)\n"
        "  -V, --val <F>        validation game fraction 0..1 (default 0.05)\n"
        "  -o, --output <path>  output path without .bin\n"
        "  -h, --help           this help message\n";
}

/**
 * Entry point for the standalone dataset generator: parses CLI arguments,
 * creates the dataset and stores it.
 * @param argc ; int ; number of arguments
 * @param argv ; char** ; argument vector
 * @return int ; 0 on success, 1 on argument error
 */
int main(int argc, char** argv) {
    NNUEDatasetConfig config;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const bool has_next = (i + 1 < argc);
        try {
            if ((a == "-n" || a == "--positions") && has_next)
                config.target_positions = std::stoull(argv[++i]);
            else if ((a == "-s" || a == "--size") && has_next)
                config.board_size = std::stoi(argv[++i]);
            else if ((a == "-d" || a == "--depth") && has_next)
                config.max_depth = std::stoi(argv[++i]);
            else if ((a == "-t" || a == "--time") && has_next)
                config.time_limit_ms = std::stoi(argv[++i]);
            else if ((a == "-c" || a == "--cores") && has_next)
                config.cores = static_cast<unsigned>(std::stoul(argv[++i]));
            else if ((a == "-O" || a == "--opening") && has_next)
                config.opening_max = std::stoi(argv[++i]);
            else if ((a == "-V" || a == "--val") && has_next)
                config.val_fraction = std::stod(argv[++i]);
            else if ((a == "-o" || a == "--output") && has_next)
                config.dataset_path = argv[++i];
            else if (a == "-h" || a == "--help") { print_usage(); return 0; }
            else { std::cerr << "unknown Argument: " << a << "\n"; return 1; }
        } catch (const std::exception&) {
            std::cerr << "invalid Value for " << a << "\n";
            return 1;
        }
    }

    if (config.board_size < 5 || config.board_size > 19) {
        std::cerr << "board_size must be 5..19\n";
        return 1;
    }

    std::cout << "Generating " << config.target_positions << " positions"
              << " | size " << config.board_size
              << " | depth " << config.max_depth
              << " | time " << config.time_limit_ms << "ms"
              << " | cores " << config.cores << "\n";

    NNUEDataset ds;
    ds.generate_dataset(config);
    ds.save_dataset(config);

    std::cout << "saved shards as: " << config.dataset_path << "-*.bin"
              << " (+ " << config.dataset_path << "_val-*.bin)\n";
    return 0;
}