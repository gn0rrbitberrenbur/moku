#include "config.hpp"
#include "nnue.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace nnue_quality
{
std::string canonical_key(const Board& board, bool black_to_move)
{
    const int size = g_config.board_size;
    std::string best;
    for (int sym = 0; sym < 8; ++sym)
    {
        std::string key(static_cast<std::size_t>(size * size) + 2, '\0');
        key[0] = static_cast<char>(size);
        key[1] = black_to_move ? 1 : 0;
        for (int row = 0; row < size; ++row)
        {
            for (int col = 0; col < size; ++col)
            {
                int y = row;
                int x = sym >= 4 ? size - 1 - col : col;
                for (int turn = 0; turn < sym % 4; ++turn)
                {
                    const int old_y = y;
                    y = x;
                    x = size - 1 - old_y;
                }
                const int pos = row * size + col;
                key[2 + y * size + x] = board.black.test(pos) ? 1 : board.white.test(pos) ? 2 : 0;
            }
        }
        if (best.empty() || key < best)
        {
            best = std::move(key);
        }
    }
    return best;
}

uint64_t key_hash(const std::string& key)
{
    uint64_t hash = 14695981039346656037ull;
    for (unsigned char byte : key)
    {
        hash ^= byte;
        hash *= 1099511628211ull;
    }
    return hash;
}

bool validation_opening(const std::string& canonical_opening, unsigned split_seed,
                        double validation_fraction)
{
    std::string key = canonical_opening;
    for (int shift = 0; shift < 32; shift += 8)
    {
        key.push_back(static_cast<char>((split_seed >> shift) & 255));
    }
    const double split = static_cast<double>(key_hash(key) >> 11) * (1.0 / 9007199254740992.0);
    return split < validation_fraction;
}

Selection select_move(Board& board, bool black_to_move,
                      const std::vector<RootCandidate>& candidates, int best_move, int ply,
                      const NNUEDatasetConfig& config, std::mt19937& rng)
{
    if (candidates.empty())
    {
        throw std::runtime_error("cannot select a training move without completed root scores");
    }
    std::vector<int> own_wins;
    std::vector<int> opponent_wins;
    for (int pos = 0; pos < config.squares(); ++pos)
    {
        if (board.black.test(pos) || board.white.test(pos))
        {
            continue;
        }
        board.make_move(pos, black_to_move);
        if (board.wins_at(pos, black_to_move))
        {
            own_wins.push_back(pos);
        }
        board.undo_move(pos);
        board.make_move(pos, !black_to_move);
        if (board.wins_at(pos, !black_to_move))
        {
            opponent_wins.push_back(pos);
        }
        board.undo_move(pos);
    }

    if (!own_wins.empty())
    {
        const auto best = std::find(own_wins.begin(), own_wins.end(), best_move);
        return {best != own_wins.end() ? best_move : own_wins.front(), SEARCH_VALID | IMMEDIATE_WIN,
                1};
    }
    if (opponent_wins.size() == 1)
    {
        return {opponent_wins.front(), SEARCH_VALID | FORCED_DEFENSE, 0};
    }

    const int8_t tactical = opponent_wins.size() > 1 ? -1 : 0;
    if (ply >= config.explore_plies || tactical != 0 ||
        !std::bernoulli_distribution(config.explore_probability)(rng))
    {
        return {best_move, SEARCH_VALID, tactical};
    }

    std::vector<RootCandidate> plausible = candidates;
    const float sign = black_to_move ? 1.0f : -1.0f;
    std::sort(plausible.begin(), plausible.end(),
              [sign](const RootCandidate& a, const RootCandidate& b)
              { return a.score == b.score ? a.move < b.move : sign * a.score > sign * b.score; });
    const double best = sign * plausible.front().score;
    plausible.erase(std::remove_if(plausible.begin(), plausible.end(), [&](const RootCandidate& c)
                                   { return best - sign * c.score > config.explore_max_loss; }),
                    plausible.end());
    if (plausible.size() > static_cast<std::size_t>(config.explore_top_k))
    {
        plausible.resize(config.explore_top_k);
    }
    std::vector<double> weights;
    for (const auto& candidate : plausible)
    {
        weights.push_back(std::exp((sign * candidate.score - best) / config.sampling_temperature));
    }
    const int move =
        plausible[std::discrete_distribution<std::size_t>(weights.begin(), weights.end())(rng)]
            .move;
    return {move, static_cast<uint16_t>(SEARCH_VALID | (move != best_move ? EXPLORED : 0)),
            tactical};
}

std::vector<std::vector<int>> load_opening_pool(const std::string& path, int board_size)
{
    if (board_size != g_config.board_size || board_size < 5 || board_size > 19)
    {
        throw std::invalid_argument("opening pool dimensions must match configured board size");
    }
    if (path.empty())
    {
        return {};
    }
    std::ifstream input(path);
    if (!input)
    {
        throw std::runtime_error("cannot open opening pool: " + path);
    }
    std::vector<std::vector<int>> pool;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line))
    {
        ++line_number;
        line = line.substr(0, line.find('#'));
        std::istringstream parser(line);
        std::string token;
        Board board;
        std::vector<int> moves;
        while (parser >> token)
        {
            std::size_t parsed = 0;
            int move = -1;
            try
            {
                move = std::stoi(token, &parsed);
            }
            catch (const std::exception&)
            {
                throw std::runtime_error("invalid opening token on line " +
                                         std::to_string(line_number));
            }
            if (parsed != token.size() || move < 0 || move >= board_size * board_size ||
                board.black.test(move) || board.white.test(move))
            {
                throw std::runtime_error("illegal opening move on line " +
                                         std::to_string(line_number));
            }
            const bool black = moves.size() % 2 == 0;
            board.make_move(move, black);
            if (board.wins_at(move, black))
            {
                throw std::runtime_error("terminal opening on line " + std::to_string(line_number));
            }
            moves.push_back(move);
        }
        if (!moves.empty())
        {
            if (moves.size() == static_cast<std::size_t>(board_size * board_size))
            {
                throw std::runtime_error("full-board opening on line " +
                                         std::to_string(line_number));
            }
            pool.push_back(std::move(moves));
        }
    }
    if (input.bad() || pool.empty())
    {
        throw std::runtime_error("opening pool is unreadable or contains no openings: " + path);
    }
    return pool;
}
} // namespace nnue_quality
