#include "nnue.hpp"

#include "config.hpp"
#include "game/board.hpp"
#include "search/minimax.hpp"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_set>
#include <vector>

/**
 * Normalizes a raw STM search score into the range [-1, 1].
 * @param raw_score ; float ; raw side-to-move score
 * @param scale ; double ; positive score normalization divisor
 * @return float ; tanh-normalized value
 */
static inline float norm_value(float raw_score, double scale)
{
    return static_cast<float>(std::tanh(raw_score / scale));
}

/**
 * Quantizes a value in [-1, 1] into an int16 using NNUE_SCORE_SCALE.
 * @param v ; float ; value in [-1, 1]
 * @return int16_t ; scaled, clamped fixed-point value
 */
static inline int16_t value_to_q(float v)
{
    long q = std::lround(v * NNUE_SCORE_SCALE);
    if (q > 32767)
        q = 32767;
    if (q < -32768)
        q = -32768;
    return static_cast<int16_t>(q);
}

/**
 * Encodes the board from the perspective of the side to move (plane 0 = own stones,
 * plane 1 = opponent stones) directly into a preallocated buffer without allocation.
 * @param black ; const std::bitset<MAX_SQUARES>& ; black stones
 * @param white ; const std::bitset<MAX_SQUARES>& ; white stones
 * @param side_to_move ; uint8_t ; 1 when black is to move
 * @param sq ; int ; number of squares (board_size^2)
 * @param dst ; int8_t* ; destination buffer of length 2 * sq
 * @return void
 */
static inline void encode_stm(const std::bitset<MAX_SQUARES>& black,
                              const std::bitset<MAX_SQUARES>& white, uint8_t side_to_move, int sq,
                              int8_t* dst)
{
    std::memset(dst, 0, static_cast<std::size_t>(2) * sq);
    const bool btm = side_to_move == 1;
    const std::bitset<MAX_SQUARES>& own = btm ? black : white;
    const std::bitset<MAX_SQUARES>& opp = btm ? white : black;
    for (int p = 0; p < sq; ++p)
    {
        if (own.test(p))
            dst[p] = 1;
        if (opp.test(p))
            dst[sq + p] = 1;
    }
}

/**
 * Removes obsolete numbered shards after the current shard set has been written.
 * @param prefix ; const std::string& ; output prefix without shard suffix
 * @param shard_count ; std::size_t ; number of current shards
 * @return void
 */
static void remove_stale_shards(const std::string& prefix, std::size_t shard_count)
{
    const std::filesystem::path prefix_path(prefix);
    const std::filesystem::path directory =
        prefix_path.has_parent_path() ? prefix_path.parent_path() : std::filesystem::path(".");
    const std::string shard_prefix = prefix_path.filename().string() + "-";

    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
        if (!entry.is_regular_file())
            continue;

        const std::string name = entry.path().filename().string();
        if (name.size() < shard_prefix.size() + 5 + 4 ||
            name.compare(0, shard_prefix.size(), shard_prefix) != 0 ||
            name.compare(name.size() - 4, 4, ".bin") != 0)
        {
            continue;
        }

        const std::string_view digits(name.data() + shard_prefix.size(),
                                      name.size() - shard_prefix.size() - 4);
        if (digits.size() < 5 || !std::all_of(digits.begin(), digits.end(), [](char digit)
                                              { return digit >= '0' && digit <= '9'; }))
        {
            continue;
        }

        std::size_t shard_index = 0;
        const auto parsed =
            std::from_chars(digits.data(), digits.data() + digits.size(), shard_index);
        if ((parsed.ec == std::errc{} && parsed.ptr == digits.data() + digits.size() &&
             shard_index >= shard_count) ||
            parsed.ec == std::errc::result_out_of_range)
        {
            std::filesystem::remove(entry.path());
        }
    }
}

/**
 * Selects a random (non-stored) opening move to diversify early positions: on an
 * empty board, pick a random square in the central region; otherwise pick a random
 * free square near existing stones (similar to the search move count).
 * @param board ; const Board& ; current board state
 * @param eval ; Evaluator& ; provides candidate moves near stones
 * @param rng ; std::mt19937& ; random source
 * @return int ; move index or -1 when no square is available
 */
static int random_opening_move(const Board& board, Evaluator& eval, std::mt19937& rng)
{
    const int SIZE = g_config.board_size;
    if (board.black.count() == 0 && board.white.count() == 0)
    {
        const int c = SIZE / 2;
        const int R = std::max(1, SIZE / 4);
        const int span = 2 * R + 1;
        const int x = std::clamp(c + static_cast<int>(rng() % span) - R, 0, SIZE - 1);
        const int y = std::clamp(c + static_cast<int>(rng() % span) - R, 0, SIZE - 1);
        return y * SIZE + x;
    }
    std::vector<int> cand = eval.get_valid_moves(board);
    if (cand.empty())
        return -1;
    return cand[rng() % cand.size()];
}

/**
 * Builds a quantized soft-policy target in STM view from the root candidates:
 * top moves by softmax, probabilities scaled to 0..65535.
 * @param cands ; const std::vector<RootCandidate>& ; root candidates (black-positive)
 * @param black_to_move ; bool ; True when black is to move
 * @param Tp ; double ; policy temperature
 * @param s ; NNUESample& ; target sample whose policy/policy_count is filled
 * @return void
 */
static void build_policy(const std::vector<RootCandidate>& cands, bool black_to_move, double Tp,
                         NNUESample& s)
{
    s.policy_count = 0;
    for (auto& entry : s.policy)
    {
        entry = {};
    }
    if (cands.empty())
    {
        throw std::runtime_error("cannot build a policy without completed root scores");
    }

    std::vector<std::pair<double, int>> scored;
    scored.reserve(cands.size());
    for (const auto& rc : cands)
    {
        scored.emplace_back(black_to_move ? rc.score : -rc.score, rc.move);
    }
    std::sort(scored.begin(), scored.end(),
              [](const std::pair<double, int>& a, const std::pair<double, int>& b)
              { return a.first == b.first ? a.second < b.second : a.first > b.first; });

    const int k = std::min<int>(static_cast<int>(scored.size()), NNUE_MAX_POLICY);
    const double maxw = scored[0].first;
    std::vector<double> w(k);
    double sum = 0.0;
    for (int i = 0; i < k; ++i)
    {
        w[i] = std::exp((scored[i].first - maxw) / Tp);
        sum += w[i];
    }
    long quantized_sum = 0;
    for (int i = 0; i < k; ++i)
    {
        const long q = std::lround(w[i] / sum * 65535.0);
        if (q > 0)
        {
            s.policy[s.policy_count].move = static_cast<uint16_t>(scored[i].second);
            s.policy[s.policy_count].prob = static_cast<uint16_t>(q);
            ++s.policy_count;
            quantized_sum += q;
        }
    }
    s.policy[0].prob = static_cast<uint16_t>(s.policy[0].prob + 65535 - quantized_sum);
}

struct GenerationState
{
    std::mutex mutex;
    std::unordered_set<std::string> seen;
    std::atomic<bool> cancelled{false};
    uint64_t games = 0;
    uint64_t duplicates = 0;
};

// Reject incomplete searches and preserve raw scores separately from bounded targets.
static void record_search(NNUESample& sample, const MinimaxAgent& agent,
                          const NNUEDatasetConfig& config)
{
    if (agent.get_completed_depth() < config.min_depth || agent.get_last_root_candidates().empty())
    {
        throw std::runtime_error("search did not complete the required depth; increase --time "
                                 "or lower --min-depth");
    }
    const float sign = sample.side_to_move ? 1.0f : -1.0f;
    sample.raw_search = sign * agent.get_last_root_score();
    if (!std::isfinite(sample.raw_search))
    {
        throw std::runtime_error("nonfinite search score in dataset generation");
    }
    sample.search_score = value_to_q(norm_value(sample.raw_search, config.score_norm));
    sample.depth = static_cast<uint16_t>(agent.get_completed_depth());
    sample.nodes = static_cast<uint64_t>(agent.get_nodes_searched());
    const auto& cands = agent.get_last_root_candidates();
    const auto played = std::find_if(cands.begin(), cands.end(), [&](const RootCandidate& c)
                                     { return c.move == sample.played_move; });
    if (played == cands.end() || !std::isfinite(played->score))
    {
        throw std::runtime_error("played move has no finite completed search score");
    }
    sample.played_score = sign * played->score;
    build_policy(cands, sample.side_to_move != 0, config.policy_temp, sample);
}

/**
 * Completes self-play games, optionally relabels positions, and retains diverse samples.
 * The worker owns search state and buffers; shared full-key deduplication is synchronized.
 * Exceptions propagate through the caller's cancellation and join boundary.
 */
static void generate_worker(const NNUEDatasetConfig& config, std::size_t quota, unsigned worker,
                            const std::vector<std::vector<int>>& pool, GenerationState& state,
                            std::vector<NNUESample>& out_train, std::vector<NNUESample>& out_val,
                            std::atomic<std::size_t>& progress)
{
    if (quota == 0)
    {
        return;
    }
    MinimaxAgent agent(config.max_depth, config.tt_memory_bytes);
    Evaluator opening_eval;
    std::mt19937 rng(config.seed + worker);
    const int squares = config.squares();
    const float lambda = static_cast<float>(config.td_lambda);
    uint64_t game_index = worker;
    unsigned stalled_games = 0;

    while (out_train.size() + out_val.size() < quota && !state.cancelled.load())
    {
        if (game_index > std::numeric_limits<uint32_t>::max())
        {
            throw std::runtime_error("game ID range exhausted");
        }
        const uint64_t game_id = (static_cast<uint64_t>(config.seed) << 32) | game_index;
        game_index += config.cores;
        agent.clear_tt();
        Board board;
        bool black_to_move = true;
        int stones = 0;
        std::vector<NNUESample> game;
        float result_black = 0.0f;

        std::vector<int> opening_moves;
        if (!pool.empty())
        {
            opening_moves = pool[rng() % pool.size()];
        }
        else
        {
            int opening = 0;
            if (config.opening_max >= config.opening_min)
            {
                const int span = config.opening_max - config.opening_min + 1;
                opening = config.opening_min + static_cast<int>(rng() % span);
            }
            for (int o = 0; o < opening && stones < squares; ++o)
            {
                const int pos = random_opening_move(board, opening_eval, rng);
                if (pos < 0)
                {
                    throw std::runtime_error("no candidate for randomized opening");
                }
                board.make_move(pos, black_to_move);
                if (board.wins_at(pos, black_to_move))
                {
                    board.undo_move(pos);
                    break;
                }
                opening_moves.push_back(pos);
                ++stones;
                black_to_move = !black_to_move;
            }
        }
        if (!pool.empty())
        {
            for (int pos : opening_moves)
            {
                board.make_move(pos, black_to_move);
                ++stones;
                black_to_move = !black_to_move;
            }
        }
        const std::string opening_key = nnue_quality::canonical_key(board, black_to_move);
        const uint64_t opening_id = nnue_quality::key_hash(opening_key);
        const bool validation =
            nnue_quality::validation_opening(opening_key, config.split_seed, config.val_fraction);
        uint8_t end_reason = 2;
        while (stones < squares && !state.cancelled.load())
        {
            const int move = agent.get_best_move_timed(board, black_to_move, config.time_limit_ms);
            if (move < 0)
            {
                throw std::runtime_error("search returned no move on a nonterminal board");
            }

            const std::vector<RootCandidate>& cands = agent.get_last_root_candidates();
            if (agent.get_completed_depth() < config.min_depth || cands.empty())
            {
                throw std::runtime_error("search did not complete the required depth; "
                                         "increase --time or lower --min-depth");
            }
            const auto selection =
                nnue_quality::select_move(board, black_to_move, cands, move, stones, config, rng);

            NNUESample s;
            s.black = board.black;
            s.white = board.white;
            s.ply = static_cast<uint16_t>(stones);
            s.side_to_move = black_to_move ? 1 : 0;
            s.game_id = game_id;
            s.opening_id = opening_id;
            s.position_key =
                nnue_quality::key_hash(nnue_quality::canonical_key(board, black_to_move));
            s.played_move = static_cast<uint16_t>(selection.move);
            s.flags = selection.flags;
            s.tactical = selection.tactical;
            record_search(s, agent, config);
            game.push_back(std::move(s));

            board.make_move(selection.move, black_to_move);
            ++stones;
            if (board.wins_at(selection.move, black_to_move))
            {
                result_black = black_to_move ? 1.0f : -1.0f;
                end_reason = 1;
                break;
            }
            black_to_move = !black_to_move;
        }
        if (state.cancelled.load())
        {
            return;
        }

        for (std::size_t i = 0; i < game.size(); ++i)
        {
            if (state.cancelled.load())
            {
                return;
            }
            auto& s = game[i];
            const bool s_black = s.side_to_move == 1;
            s.final_value = value_to_q(s_black ? result_black : -result_black);
            s.end_reason = end_reason;
            if (config.relabel_every > 0 && i % config.relabel_every == 0)
            {
                Board position;
                position.black = s.black;
                position.white = s.white;
                agent.clear_tt();
                agent.set_max_depth(config.relabel_depth);
                agent.get_best_move_timed(position, s_black, config.relabel_time_ms);
                record_search(s, agent, config);
                s.flags |= RELABELLED;
                agent.set_max_depth(config.max_depth);
            }
        }

        float td_black_next = result_black;
        for (int i = static_cast<int>(game.size()) - 1; i >= 0; --i)
        {
            const bool s_black = game[i].side_to_move == 1;
            const float sv_stm = game[i].search_score / static_cast<float>(NNUE_SCORE_SCALE);
            const float sv_black = s_black ? sv_stm : -sv_stm;
            const float td_black = (1.0f - lambda) * sv_black + lambda * td_black_next;
            td_black_next = td_black;
            game[i].td_value = value_to_q(s_black ? td_black : -td_black);
        }

        std::vector<std::size_t> retained;
        const std::size_t offset = rng() % config.sample_stride;
        for (std::size_t i = offset; i < game.size(); i += config.sample_stride)
        {
            retained.push_back(i);
        }
        std::shuffle(retained.begin(), retained.end(), rng);
        if (retained.size() > static_cast<std::size_t>(config.max_samples_per_game))
        {
            retained.resize(config.max_samples_per_game);
        }
        std::sort(retained.begin(), retained.end());
        std::vector<NNUESample>& dst = validation ? out_val : out_train;
        std::size_t added = 0;
        {
            std::lock_guard<std::mutex> lock(state.mutex);
            ++state.games;
            for (std::size_t i : retained)
            {
                auto& s = game[i];
                Board position;
                position.black = s.black;
                position.white = s.white;
                if (!state.seen.insert(nnue_quality::canonical_key(position, s.side_to_move != 0))
                         .second)
                {
                    ++state.duplicates;
                    continue;
                }
                dst.push_back(std::move(s));
                ++added;
                progress.fetch_add(1, std::memory_order_relaxed);
                if (out_train.size() + out_val.size() >= quota)
                {
                    break;
                }
            }
        }
        stalled_games = added == 0 ? stalled_games + 1 : 0;
        if (stalled_games >= 1000)
        {
            throw std::runtime_error("1000 games produced no new samples; diversify openings "
                                     "or reduce the requested dataset size");
        }
    }
}

/**
 * Generates the dataset in parallel and prints live progress (percent, rate,
 * elapsed time and ETA) to stderr during generation.
 * @param config ; const NNUEDatasetConfig& ; dataset generation parameters
 * @return void
 */
void NNUEDataset::generate_dataset(const NNUEDatasetConfig& config)
{
    if (config.board_size < 5 || config.board_size > 19 || config.target_positions == 0 ||
        config.cores == 0 || config.max_depth < 1 || config.max_depth > 65535 ||
        config.min_depth < 1 || config.min_depth > config.max_depth || config.time_limit_ms < 1 ||
        config.opening_min < 0 || config.opening_max < 0 || config.opening_max > config.squares() ||
        config.sample_stride < 1 || config.max_samples_per_game < 1 || config.explore_plies < 0 ||
        config.explore_top_k < 1 || config.relabel_every < 0 || config.relabel_depth < 1 ||
        config.relabel_depth > 65535 || config.relabel_time_ms < 1 ||
        (config.relabel_every > 0 && (config.relabel_depth < config.max_depth ||
                                      config.relabel_time_ms < config.time_limit_ms)) ||
        !std::isfinite(config.val_fraction) || config.val_fraction < 0 || config.val_fraction > 1 ||
        !std::isfinite(config.explore_probability) || config.explore_probability < 0 ||
        config.explore_probability > 1 || !std::isfinite(config.explore_max_loss) ||
        config.explore_max_loss < 0 || !std::isfinite(config.sampling_temperature) ||
        config.sampling_temperature <= 0 || !std::isfinite(config.policy_temp) ||
        config.policy_temp <= 0 || !std::isfinite(config.score_norm) || config.score_norm <= 0 ||
        !std::isfinite(config.td_lambda) || config.td_lambda < 0 || config.td_lambda > 1 ||
        config.shard_size == 0 || config.shard_size > std::numeric_limits<uint32_t>::max() ||
        config.dataset_path.empty())
    {
        throw std::invalid_argument("invalid dataset configuration; check positive budgets, "
                                    "depths, temperatures, fractions, and sampling limits");
    }
    g_config.board_size = config.board_size;
    Evaluator evaluator;
    evaluator.position_score(0); // Initialize the shared cache before concurrent readers.
    const auto pool = nnue_quality::load_opening_pool(config.opening_pool_path, config.board_size);
    used_openings = pool;
    GenerationState state;
    std::exception_ptr failure;

    const unsigned n_threads =
        static_cast<unsigned>(std::min<std::size_t>(config.cores, config.target_positions));
    const std::size_t base = config.target_positions / n_threads;
    const std::size_t rest = config.target_positions - base * n_threads;

    std::vector<std::vector<NNUESample>> buffers(n_threads);
    std::vector<std::vector<NNUESample>> val_buffers(n_threads);
    std::vector<std::thread> threads;
    std::atomic<std::size_t> progress{0};
    std::atomic<bool> done{false};

    const auto start = std::chrono::steady_clock::now();
    std::thread monitor(
        [&]
        {
            while (!done.load(std::memory_order_relaxed))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                const std::size_t cur = progress.load(std::memory_order_relaxed);
                const double elapsed =
                    std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
                const double rate = elapsed > 0.0 ? static_cast<double>(cur) / elapsed : 0.0;
                const double pct = 100.0 * double(cur) / double(config.target_positions);
                const double eta =
                    rate > 0.0 ? static_cast<double>(config.target_positions - cur) / rate : 0.0;
                std::cerr << "\r[" << std::fixed << std::setprecision(1) << pct << "%] " << cur
                          << "/" << config.target_positions << " | " << int(rate) << " pos/s"
                          << " | elapsed " << int(elapsed) << "s"
                          << " | eta " << int(eta) << "s        " << std::flush;
            }
        });

    try
    {
        for (unsigned t = 0; t < n_threads; ++t)
        {
            const std::size_t quota = base + (t == n_threads - 1 ? rest : 0);
            threads.emplace_back(
                [&, t, quota]
                {
                    try
                    {
                        generate_worker(config, quota, t, pool, state, buffers[t], val_buffers[t],
                                        progress);
                    }
                    catch (...)
                    {
                        std::lock_guard<std::mutex> lock(state.mutex);
                        if (!failure)
                        {
                            failure = std::current_exception();
                        }
                        state.cancelled.store(true);
                    }
                });
        }
    }
    catch (...)
    {
        state.cancelled.store(true);
        for (auto& th : threads)
        {
            th.join();
        }
        done.store(true);
        monitor.join();
        throw;
    }

    for (auto& th : threads)
        th.join();
    done.store(true, std::memory_order_relaxed);
    monitor.join();
    if (failure)
    {
        std::rethrow_exception(failure);
    }
    generated_games = state.games;
    duplicate_positions = state.duplicates;

    samples.clear();
    val_samples.clear();
    samples.reserve(config.target_positions);
    for (auto& buf : buffers)
        samples.insert(samples.end(), std::make_move_iterator(buf.begin()),
                       std::make_move_iterator(buf.end()));
    for (auto& buf : val_buffers)
        val_samples.insert(val_samples.end(), std::make_move_iterator(buf.begin()),
                           std::make_move_iterator(buf.end()));

    std::cerr << "\nfinished: " << samples.size() << " train + " << val_samples.size()
              << " val positions\n";
}

/**
 * Writes a set of samples as versioned binary shards. Each shard file
 * (<prefix>-NNNNN.bin) starts with a DatasetHeader, followed by samples:
 * feature_len int8 planes (STM), side_to_move (u8), ply (u16), search_score,
 * final_value, td_value (i16 each), policy_count (u8) and NNUE_MAX_POLICY
 * policy entries, then game IDs, raw scores, and search/selection quality metadata.
 * @param prefix ; const std::string& ; file prefix without shard suffix
 * @param samples ; const std::vector<NNUESample>& ; samples to write
 * @param config ; const NNUEDatasetConfig& ; provides board_size and shard_size
 * @return std::size_t ; number of shards written
 */
static std::size_t write_shards(const std::string& prefix, const std::vector<NNUESample>& samples,
                                const NNUEDatasetConfig& config)
{
    const int sq = config.squares();
    const int32_t flen = 2 * sq;
    const std::size_t total = samples.size();
    const std::size_t shard_size = std::max<std::size_t>(1, config.shard_size);
    const std::size_t n_shards = total == 0 ? 0 : (total + shard_size - 1) / shard_size;

    std::vector<int8_t> feat(static_cast<std::size_t>(flen));

    for (std::size_t shard = 0; shard < n_shards; ++shard)
    {
        const std::size_t begin = shard * shard_size;
        const std::size_t end = std::min(total, begin + shard_size);
        const uint32_t count = static_cast<uint32_t>(end - begin);

        char suffix[16];
        std::snprintf(suffix, sizeof(suffix), "-%05zu.bin", shard);
        const std::string path = prefix + suffix;
        std::ofstream out(path, std::ios::binary);
        if (!out.is_open())
            throw std::runtime_error("failed to open dataset shard: " + path);

        const uint32_t magic = NNUE_DATASET_MAGIC;
        const uint32_t version = NNUE_DATASET_VERSION;
        const uint16_t board_size = static_cast<uint16_t>(config.board_size);
        const uint16_t rule = 0;
        const uint16_t feature_len = static_cast<uint16_t>(flen);
        const uint16_t score_scale = static_cast<uint16_t>(NNUE_SCORE_SCALE);
        const uint16_t max_policy = static_cast<uint16_t>(NNUE_MAX_POLICY);
        const uint16_t reserved0 = 0;
        const uint64_t reserved = 0;

        out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        out.write(reinterpret_cast<const char*>(&version), sizeof(version));
        out.write(reinterpret_cast<const char*>(&board_size), sizeof(board_size));
        out.write(reinterpret_cast<const char*>(&rule), sizeof(rule));
        out.write(reinterpret_cast<const char*>(&count), sizeof(count));
        out.write(reinterpret_cast<const char*>(&feature_len), sizeof(feature_len));
        out.write(reinterpret_cast<const char*>(&score_scale), sizeof(score_scale));
        out.write(reinterpret_cast<const char*>(&max_policy), sizeof(max_policy));
        out.write(reinterpret_cast<const char*>(&reserved0), sizeof(reserved0));
        out.write(reinterpret_cast<const char*>(&reserved), sizeof(reserved));
        out.write(reinterpret_cast<const char*>(&reserved), sizeof(reserved));
        out.write(reinterpret_cast<const char*>(&reserved), sizeof(reserved));

        for (std::size_t i = begin; i < end; ++i)
        {
            const NNUESample& s = samples[i];
            encode_stm(s.black, s.white, s.side_to_move, sq, feat.data());
            out.write(reinterpret_cast<const char*>(feat.data()), flen);
            out.write(reinterpret_cast<const char*>(&s.side_to_move), sizeof(s.side_to_move));
            out.write(reinterpret_cast<const char*>(&s.ply), sizeof(s.ply));
            out.write(reinterpret_cast<const char*>(&s.search_score), sizeof(s.search_score));
            out.write(reinterpret_cast<const char*>(&s.final_value), sizeof(s.final_value));
            out.write(reinterpret_cast<const char*>(&s.td_value), sizeof(s.td_value));
            out.write(reinterpret_cast<const char*>(&s.policy_count), sizeof(s.policy_count));
            for (int k = 0; k < NNUE_MAX_POLICY; ++k)
            {
                out.write(reinterpret_cast<const char*>(&s.policy[k].move), sizeof(uint16_t));
                out.write(reinterpret_cast<const char*>(&s.policy[k].prob), sizeof(uint16_t));
            }
            out.write(reinterpret_cast<const char*>(&s.game_id), sizeof(s.game_id));
            out.write(reinterpret_cast<const char*>(&s.opening_id), sizeof(s.opening_id));
            out.write(reinterpret_cast<const char*>(&s.position_key), sizeof(s.position_key));
            out.write(reinterpret_cast<const char*>(&s.raw_search), sizeof(s.raw_search));
            out.write(reinterpret_cast<const char*>(&s.played_score), sizeof(s.played_score));
            out.write(reinterpret_cast<const char*>(&s.nodes), sizeof(s.nodes));
            out.write(reinterpret_cast<const char*>(&s.depth), sizeof(s.depth));
            out.write(reinterpret_cast<const char*>(&s.played_move), sizeof(s.played_move));
            out.write(reinterpret_cast<const char*>(&s.flags), sizeof(s.flags));
            out.write(reinterpret_cast<const char*>(&s.end_reason), sizeof(s.end_reason));
            out.write(reinterpret_cast<const char*>(&s.tactical), sizeof(s.tactical));
        }

        out.flush();
        if (!out)
            throw std::runtime_error("failed to write dataset shard: " + path);
        out.close();
        if (!out)
            throw std::runtime_error("failed to close dataset shard: " + path);
    }
    remove_stale_shards(prefix, n_shards);
    return n_shards;
}

/**
 * Writes training and validation samples into separate shard prefixes
 * (<path>-NNNNN.bin and <path>_val-NNNNN.bin).
 * @param config ; const NNUEDatasetConfig& ; provides output path and shard_size
 * @return void
 */
void NNUEDataset::save_dataset(const NNUEDatasetConfig& config)
{
    const uint16_t endian_check = 1;
    if (*reinterpret_cast<const unsigned char*>(&endian_check) != 1 || sizeof(float) != 4 ||
        !std::numeric_limits<float>::is_iec559)
    {
        throw std::runtime_error("dataset writer requires little-endian IEEE-754 float32");
    }
    const std::size_t train_shards = write_shards(config.dataset_path, samples, config);
    const std::size_t val_shards = write_shards(config.dataset_path + "_val", val_samples, config);

    std::ofstream manifest(config.dataset_path + ".manifest.json");
    if (!manifest)
    {
        throw std::runtime_error("cannot open dataset manifest");
    }
    // Paths use JSON escaping, including Windows separators and control characters.
    const auto json_string = [](const std::string& text)
    {
        std::ostringstream out;
        out << '"';
        for (unsigned char ch : text)
        {
            if (ch == '"' || ch == '\\')
            {
                out << '\\' << static_cast<char>(ch);
            }
            else if (ch < 32)
            {
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << int(ch);
            }
            else
            {
                out << static_cast<char>(ch);
            }
        }
        out << '"';
        return out.str();
    };
    manifest << std::setprecision(17) << "{\n  \"format_version\": " << NNUE_DATASET_VERSION
             << ",\n  \"engine_version\": " << json_string(g_config.version)
             << ",\n  \"evaluator\": \"handcrafted\","
                "\n  \"model\": null,\n  \"board_size\": "
             << config.board_size << ",\n  \"seed\": " << config.seed
             << ",\n  \"split_seed\": " << config.split_seed << ",\n  \"workers\": " << config.cores
             << ",\n  \"target_positions\": " << config.target_positions
             << ",\n  \"max_depth\": " << config.max_depth
             << ",\n  \"min_depth\": " << config.min_depth
             << ",\n  \"time_ms\": " << config.time_limit_ms
             << ",\n  \"tt_memory_bytes\": " << config.tt_memory_bytes
             << ",\n  \"opening_min\": " << config.opening_min
             << ",\n  \"opening_max\": " << config.opening_max
             << ",\n  \"opening_pool\": " << json_string(config.opening_pool_path)
             << ",\n  \"validation_fraction\": " << config.val_fraction
             << ",\n  \"score_norm\": " << config.score_norm
             << ",\n  \"sampling_temperature\": " << config.sampling_temperature
             << ",\n  \"policy_temperature\": " << config.policy_temp
             << ",\n  \"explore_plies\": " << config.explore_plies
             << ",\n  \"explore_top_k\": " << config.explore_top_k
             << ",\n  \"explore_probability\": " << config.explore_probability
             << ",\n  \"explore_max_loss\": " << config.explore_max_loss
             << ",\n  \"sample_stride\": " << config.sample_stride
             << ",\n  \"max_samples_per_game\": " << config.max_samples_per_game
             << ",\n  \"td_lambda\": " << config.td_lambda
             << ",\n  \"relabel_every\": " << config.relabel_every
             << ",\n  \"relabel_depth\": " << config.relabel_depth
             << ",\n  \"relabel_time_ms\": " << config.relabel_time_ms
             << ",\n  \"shard_size\": " << config.shard_size
             << ",\n  \"generated_games\": " << generated_games
             << ",\n  \"duplicates_removed\": " << duplicate_positions
             << ",\n  \"train_samples\": " << samples.size()
             << ",\n  \"validation_samples\": " << val_samples.size()
             << ",\n  \"opening_sequences\": [";
    for (std::size_t i = 0; i < used_openings.size(); ++i)
    {
        manifest << (i == 0 ? "[" : ",[");
        for (std::size_t j = 0; j < used_openings[i].size(); ++j)
        {
            manifest << (j == 0 ? "" : ",") << used_openings[i][j];
        }
        manifest << "]";
    }
    const auto now = std::time(nullptr);
    const auto* utc = std::gmtime(&now);
    if (!utc)
    {
        throw std::runtime_error("cannot obtain UTC timestamp for manifest");
    }
    manifest << "],\n  \"written_utc\": \"" << std::put_time(utc, "%Y-%m-%dT%H:%M:%SZ")
             << "\"\n}\n";
    manifest.close();
    if (!manifest)
    {
        throw std::runtime_error("failed to write dataset manifest");
    }
    std::cerr << "wrote " << train_shards << " train shard(s) / " << samples.size() << " samples, "
              << val_shards << " val shard(s) / " << val_samples.size() << " samples\n";
}