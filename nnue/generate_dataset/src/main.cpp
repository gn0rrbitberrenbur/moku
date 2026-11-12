#include "../include/nnue.hpp"

#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

static int parse_int(const std::string& value)
{
    std::size_t parsed = 0;
    const int result = std::stoi(value, &parsed);
    if (parsed != value.size())
        throw std::invalid_argument("expected an integer");
    return result;
}

static std::size_t parse_positions(const std::string& value)
{
    if (value.empty() || value.front() == '-')
        throw std::invalid_argument("expected a nonnegative integer");

    std::size_t parsed = 0;
    const unsigned long long result = std::stoull(value, &parsed);
    if (parsed != value.size() || result > std::numeric_limits<std::size_t>::max())
        throw std::invalid_argument("expected a nonnegative integer");
    return static_cast<std::size_t>(result);
}

static unsigned parse_cores(const std::string& value)
{
    if (value.empty() || value.front() == '-')
        throw std::invalid_argument("expected a positive integer");

    std::size_t parsed = 0;
    const unsigned long long result = std::stoull(value, &parsed);
    if (parsed != value.size() || result > std::numeric_limits<unsigned>::max())
        throw std::invalid_argument("expected a positive integer");
    return static_cast<unsigned>(result);
}

static double parse_fraction(const std::string& value)
{
    std::size_t parsed = 0;
    const double result = std::stod(value, &parsed);
    if (parsed != value.size() || !std::isfinite(result))
        throw std::invalid_argument("expected a finite number");
    return result;
}

/**
 * Prints the command-line usage for the dataset generator.
 * @return void
 */
static void print_usage()
{
    std::cout << "nnue_gen - Gomoku NNUE Dataset Generator\n"
                 "  -n, --positions <N>  target positions > 0 (default 1000000)\n"
                 "  -s, --size <N>       board size 5..19 (default 15)\n"
                 "  -d, --depth <N>      maximum search depth > 0 (default 12)\n"
                 "  -t, --time <ms>      search time per move > 0 (default 20000)\n"
                 "  -c, --cores <N>      threads > 0 (default: all cores)\n"
                 "  -O, --opening <N>    max. random opening plies >= 0 (default 8, 0=off)\n"
                 "  -V, --val <F>        validation game fraction 0..1 (default 0.05)\n"
                 "  -o, --output <path>  output path without .bin\n"
                 "  --seed <N>          self-play seed (default 1234)\n"
                 "  --split-seed <N>    stable opening split seed (default 2026)\n"
                 "  --opening-pool <path> validated whitespace-separated move sequences\n"
                 "  --min-depth <N>     minimum completed depth, otherwise fail (default 1)\n"
                 "  --explore-plies <N> explore before this absolute ply (default 16)\n"
                 "  --explore-prob <F>  exploration probability (default 0.15)\n"
                 "  --explore-top-k <N> plausible candidate limit (default 4)\n"
                 "  --explore-max-loss <F> maximum raw score loss (default 200)\n"
                 "  --sampling-temperature <F> raw-score softmax scale (default 100)\n"
                 "  --policy-temperature <F> raw-score policy scale (default 200)\n"
                 "  --score-norm <F>    value tanh divisor (default 1000)\n"
                 "  --sample-stride <N> position sampling interval (default 2)\n"
                 "  --max-samples-per-game <N> sample cap per game (default 32)\n"
                 "  --relabel-every <N> deeper label every Nth position, 0=off (default 0)\n"
                 "  --relabel-depth <N> higher-budget label depth (default 14)\n"
                 "  --relabel-time <ms> higher-budget label time (default 80000)\n"
                 "  --tt-memory <bytes> per-worker TT storage budget (default 16777216)\n"
                 "  -h, --help           this help message\n";
}

/**
 * Entry point for the standalone dataset generator: parses CLI arguments,
 * creates the dataset and stores it.
 * @param argc ; int ; number of arguments
 * @param argv ; char** ; argument vector
 * @return int ; 0 on success, 1 on argument error
 */
int main(int argc, char** argv)
{
    NNUEDatasetConfig config;

    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        const bool has_next = (i + 1 < argc);
        const auto next_value = [&]() -> std::string
        {
            if (!has_next)
                throw std::invalid_argument("missing option value");
            return argv[++i];
        };
        try
        {
            if (a == "-n" || a == "--positions")
                config.target_positions = parse_positions(next_value());
            else if (a == "-s" || a == "--size")
                config.board_size = parse_int(next_value());
            else if (a == "-d" || a == "--depth")
                config.max_depth = parse_int(next_value());
            else if (a == "-t" || a == "--time")
                config.time_limit_ms = parse_int(next_value());
            else if (a == "-c" || a == "--cores")
                config.cores = parse_cores(next_value());
            else if (a == "-O" || a == "--opening")
                config.opening_max = parse_int(next_value());
            else if (a == "-V" || a == "--val")
                config.val_fraction = parse_fraction(next_value());
            else if (a == "-o" || a == "--output")
                config.dataset_path = next_value();
            else if (a == "--seed")
                config.seed = parse_cores(next_value());
            else if (a == "--split-seed")
                config.split_seed = parse_cores(next_value());
            else if (a == "--opening-pool")
                config.opening_pool_path = next_value();
            else if (a == "--min-depth")
                config.min_depth = parse_int(next_value());
            else if (a == "--explore-plies")
                config.explore_plies = parse_int(next_value());
            else if (a == "--explore-prob")
                config.explore_probability = parse_fraction(next_value());
            else if (a == "--explore-top-k")
                config.explore_top_k = parse_int(next_value());
            else if (a == "--explore-max-loss")
                config.explore_max_loss = parse_fraction(next_value());
            else if (a == "--sampling-temperature")
                config.sampling_temperature = parse_fraction(next_value());
            else if (a == "--policy-temperature")
                config.policy_temp = parse_fraction(next_value());
            else if (a == "--score-norm")
                config.score_norm = parse_fraction(next_value());
            else if (a == "--sample-stride")
                config.sample_stride = parse_int(next_value());
            else if (a == "--max-samples-per-game")
                config.max_samples_per_game = parse_int(next_value());
            else if (a == "--relabel-every")
                config.relabel_every = parse_int(next_value());
            else if (a == "--relabel-depth")
                config.relabel_depth = parse_int(next_value());
            else if (a == "--relabel-time")
                config.relabel_time_ms = parse_int(next_value());
            else if (a == "--tt-memory")
                config.tt_memory_bytes = parse_positions(next_value());
            else if (a == "-h" || a == "--help")
            {
                print_usage();
                return 0;
            }
            else
            {
                std::cerr << "unknown Argument: " << a << "\n";
                return 1;
            }
        }
        catch (const std::exception& error)
        {
            std::cerr << "invalid value for " << a << ": " << error.what() << "\n";
            return 1;
        }
    }

    if (config.target_positions == 0)
    {
        std::cerr << "positions must be greater than zero\n";
        return 1;
    }
    if (config.board_size < 5 || config.board_size > 19)
    {
        std::cerr << "board_size must be 5..19\n";
        return 1;
    }
    if (config.max_depth <= 0)
    {
        std::cerr << "depth must be greater than zero\n";
        return 1;
    }
    if (config.time_limit_ms <= 0)
    {
        std::cerr << "time must be greater than zero\n";
        return 1;
    }
    if (config.cores == 0)
    {
        std::cerr << "cores must be greater than zero\n";
        return 1;
    }
    if (config.opening_max < 0)
    {
        std::cerr << "opening must be nonnegative\n";
        return 1;
    }
    if (config.val_fraction < 0.0 || config.val_fraction > 1.0)
    {
        std::cerr << "validation fraction must be in [0, 1]\n";
        return 1;
    }
    if (config.dataset_path.empty())
    {
        std::cerr << "output path must not be empty\n";
        return 1;
    }

    std::cout << "Generating " << config.target_positions << " positions"
              << " | size " << config.board_size << " | depth " << config.max_depth << " | time "
              << config.time_limit_ms << "ms"
              << " | cores " << config.cores << "\n";

    try
    {
        NNUEDataset ds;
        ds.generate_dataset(config);
        ds.save_dataset(config);
    }
    catch (const std::exception& error)
    {
        std::cerr << "dataset generation failed: " << error.what() << "\n";
        return 1;
    }

    std::cout << "saved shards as: " << config.dataset_path << "-*.bin"
              << " (+ " << config.dataset_path << "_val-*.bin)\n";
    return 0;
}