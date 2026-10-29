#include "nnue.hpp"
#include "board.hpp"
#include "config.hpp"
#include "minimax/minimax.hpp"
#include <random>
#include <fstream>
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <iomanip>

// Divisor mapping raw engine scores into a bounded [-1,1] value via tanh.
static constexpr float EVAL_NORM = 1000.0f;

/**
 * Normalizes a raw (black-positive) search score into the range [-1, 1].
 * @param black_positive ; float ; raw score in black-positive convention
 * @return float ; tanh-normalized value
 */
static inline float norm_value(float black_positive) {
    return std::tanh(black_positive / EVAL_NORM);
}

/**
 * Quantizes a value in [-1, 1] into an int16 using NNUE_SCORE_SCALE.
 * @param v ; float ; value in [-1, 1]
 * @return int16_t ; scaled, clamped fixed-point value
 */
static inline int16_t value_to_q(float v) {
    long q = std::lround(v * NNUE_SCORE_SCALE);
    if (q > 32767) q = 32767;
    if (q < -32768) q = -32768;
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
                              const std::bitset<MAX_SQUARES>& white,
                              uint8_t side_to_move, int sq, int8_t* dst) {
    std::memset(dst, 0, static_cast<std::size_t>(2) * sq);
    const bool btm = side_to_move == 1;
    const std::bitset<MAX_SQUARES>& own = btm ? black : white;
    const std::bitset<MAX_SQUARES>& opp = btm ? white : black;
    for (int p = 0; p < sq; ++p) {
        if (own.test(p)) dst[p] = 1;
        if (opp.test(p)) dst[sq + p] = 1;
    }
}

/**
 * Temperature for move selection depending on ply: high in the opening
 * (exploration), decreasing toward the endgame (strong play).
 * @param ply ; int ; half-move index starting at 0
 * @param config ; const NNUEDatasetConfig& ; provides open_temp and min_temp
 * @return double ; temperature T > 0
 */
static inline double temperature(int ply, const NNUEDatasetConfig& config) {
    return std::max(config.min_temp, config.open_temp * std::pow(0.5, ply / 20.0));
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
static int random_opening_move(const Board& board, Evaluator& eval, std::mt19937& rng) {
    const int SIZE = g_config.board_size;
    if (board.black.count() == 0 && board.white.count() == 0) {
        const int c = SIZE / 2;
        const int R = std::max(1, SIZE / 4);
        const int span = 2 * R + 1;
        const int x = std::clamp(c + static_cast<int>(rng() % span) - R, 0, SIZE - 1);
        const int y = std::clamp(c + static_cast<int>(rng() % span) - R, 0, SIZE - 1);
        return y * SIZE + x;
    }
    std::vector<int> cand = eval.get_valid_moves(board);
    if (cand.empty()) return -1;
    return cand[rng() % cand.size()];
}

/**
 * Selects a move by softmax over the normalized STM scores of the root candidates
 * with the given temperature.
 * @param cands ; const std::vector<RootCandidate>& ; root candidates (black-positive)
 * @param black_to_move ; bool ; True when black is to move
 * @param T ; double ; sampling temperature
 * @param rng ; std::mt19937& ; random source
 * @param fallback ; int ; return value if no candidates exist
 * @return int ; selected move index
 */
static int sample_move(const std::vector<RootCandidate>& cands, bool black_to_move,
                       double T, std::mt19937& rng, int fallback) {
    if (cands.empty()) return fallback;
    if (cands.size() == 1) return cands[0].move;

    std::vector<double> w(cands.size());
    double maxw = -1e300;
    for (std::size_t i = 0; i < cands.size(); ++i) {
        const double sn = norm_value(cands[i].score);
        w[i] = black_to_move ? sn : -sn;
        maxw = std::max(maxw, w[i]);
    }
    double sum = 0.0;
    const double Tc = std::max(T, 1e-6);
    for (std::size_t i = 0; i < cands.size(); ++i) {
        w[i] = std::exp((w[i] - maxw) / Tc);
        sum += w[i];
    }
    std::uniform_real_distribution<double> pick(0.0, sum);
    double r = pick(rng);
    double acc = 0.0;
    for (std::size_t i = 0; i < cands.size(); ++i) {
        acc += w[i];
        if (r <= acc) return cands[i].move;
    }
    return cands.back().move;
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
static void build_policy(const std::vector<RootCandidate>& cands, bool black_to_move,
                         double Tp, NNUESample& s) {
    s.policy_count = 0;
    if (cands.empty()) return;

    std::vector<std::pair<double, int>> scored;
    scored.reserve(cands.size());
    for (const auto& rc : cands) {
        const double sn = norm_value(rc.score);
        scored.emplace_back(black_to_move ? sn : -sn, rc.move);
    }
    std::sort(scored.begin(), scored.end(),
              [](const std::pair<double, int>& a, const std::pair<double, int>& b) {
                  return a.first > b.first;
              });

    const int k = std::min<int>(static_cast<int>(scored.size()), NNUE_MAX_POLICY);
    const double maxw = scored[0].first;
    const double Tc = std::max(Tp, 1e-6);
    std::vector<double> w(k);
    double sum = 0.0;
    for (int i = 0; i < k; ++i) {
        w[i] = std::exp((scored[i].first - maxw) / Tc);
        sum += w[i];
    }
    s.policy_count = static_cast<uint8_t>(k);
    for (int i = 0; i < k; ++i) {
        const double p = sum > 0.0 ? w[i] / sum : 0.0;
        long q = std::lround(p * 65535.0);
        if (q > 65535) q = 65535;
        if (q < 0) q = 0;
        s.policy[i].move = static_cast<uint16_t>(scored[i].second);
        s.policy[i].prob = static_cast<uint16_t>(q);
    }
}

/**
 * Plays self-play games with a thread-local agent and collects, for each position,
 * compact STM features, search value, final value, TD(lambda) value and a soft
 * policy target until the assigned quota is reached.
 * Move selection uses temperature-based sampling from the root candidates.
 * @param config ; const NNUEDatasetConfig& ; dataset generation parameters
 * @param quota ; std::size_t ; number of samples to generate for this worker
 * @param seed ; unsigned ; per-worker RNG seed
 * @param out_train ; std::vector<NNUESample>& ; thread-local training buffer
 * @param out_val ; std::vector<NNUESample>& ; thread-local validation buffer
 * @param progress ; std::atomic<std::size_t>& ; global progress counter
 * @return void
 */
static void generate_worker(const NNUEDatasetConfig& config, std::size_t quota,
                            unsigned seed, std::vector<NNUESample>& out_train,
                            std::vector<NNUESample>& out_val,
                            std::atomic<std::size_t>& progress) {
    MinimaxAgent agent(config.max_depth);
    Evaluator opening_eval;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    const int squares = config.squares();
    const float lambda = static_cast<float>(config.td_lambda);

    while (out_train.size() + out_val.size() < quota) {
        agent.clear_tt();
        Board board;
        bool black_to_move = true;
        int stones = 0;
        std::vector<NNUESample> game;
        float result_black = 0.0f;

        int opening = 0;
        if (config.opening_max > 0 && config.opening_max >= config.opening_min) {
            const int span = config.opening_max - config.opening_min + 1;
            opening = config.opening_min + static_cast<int>(rng() % span);
        }
        for (int o = 0; o < opening && stones < squares; ++o) {
            const int pos = random_opening_move(board, opening_eval, rng);
            if (pos < 0) break;
            board.make_move(pos, black_to_move);
            if (board.wins_at(pos, black_to_move)) {
                board.undo_move(pos);
                break;
            }
            ++stones;
            black_to_move = !black_to_move;
        }

        while (stones < squares) {
            const int move =
                agent.get_best_move_timed(board, black_to_move, config.time_limit_ms);
            if (move < 0) break;

            const std::vector<RootCandidate>& cands = agent.get_last_root_candidates();
            const float root = agent.get_last_root_score();

            NNUESample s;
            s.black = board.black;
            s.white = board.white;
            s.ply = static_cast<uint16_t>(stones);
            s.side_to_move = black_to_move ? 1 : 0;
            const float stm_root = black_to_move ? root : -root;
            s.search_score = value_to_q(norm_value(stm_root));
            build_policy(cands, black_to_move, config.policy_temp, s);
            game.push_back(std::move(s));

            const int chosen =
                sample_move(cands, black_to_move, temperature(stones, config), rng, move);

            board.make_move(chosen, black_to_move);
            ++stones;
            if (board.wins_at(chosen, black_to_move)) {
                result_black = black_to_move ? 1.0f : -1.0f;
                break;
            }
            black_to_move = !black_to_move;
        }

        for (auto& s : game) {
            const bool s_black = s.side_to_move == 1;
            s.final_value = value_to_q(s_black ? result_black : -result_black);
        }

        float td_black_next = result_black;
        for (int i = static_cast<int>(game.size()) - 1; i >= 0; --i) {
            const bool s_black = game[i].side_to_move == 1;
            const float sv_stm = game[i].search_score / static_cast<float>(NNUE_SCORE_SCALE);
            const float sv_black = s_black ? sv_stm : -sv_stm;
            const float td_black = (1.0f - lambda) * sv_black + lambda * td_black_next;
            td_black_next = td_black;
            game[i].td_value = value_to_q(s_black ? td_black : -td_black);
        }

        std::vector<NNUESample>& dst =
            (unit(rng) < config.val_fraction) ? out_val : out_train;
        for (auto& s : game) {
            dst.push_back(std::move(s));
            progress.fetch_add(1, std::memory_order_relaxed);
            if (out_train.size() + out_val.size() >= quota) break;
        }
    }
}

/**
 * Generates the dataset in parallel and prints live progress (percent, rate,
 * elapsed time and ETA) to stderr during generation.
 * @param config ; const NNUEDatasetConfig& ; dataset generation parameters
 * @return void
 */
void NNUEDataset::generate_dataset(const NNUEDatasetConfig& config) {
    g_config.board_size = config.board_size;

    const unsigned n_threads = std::max(1u, config.cores);
    const std::size_t base = config.target_positions / n_threads;
    const std::size_t rest = config.target_positions - base * n_threads;

    std::vector<std::vector<NNUESample>> buffers(n_threads);
    std::vector<std::vector<NNUESample>> val_buffers(n_threads);
    std::vector<std::thread> threads;
    std::atomic<std::size_t> progress{0};
    std::atomic<bool> done{false};

    const auto start = std::chrono::steady_clock::now();
    std::thread monitor([&] {
        while (!done.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            const std::size_t cur = progress.load(std::memory_order_relaxed);
            const double elapsed =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            const double rate = elapsed > 0.0 ? cur / elapsed : 0.0;
            const double pct = 100.0 * double(cur) / double(config.target_positions);
            const double eta = rate > 0.0 ? (config.target_positions - cur) / rate : 0.0;
            std::cerr << "\r[" << std::fixed << std::setprecision(1) << pct << "%] "
                      << cur << "/" << config.target_positions
                      << " | " << int(rate) << " pos/s"
                      << " | elapsed " << int(elapsed) << "s"
                      << " | eta " << int(eta) << "s        " << std::flush;
        }
    });

    for (unsigned t = 0; t < n_threads; ++t) {
        const std::size_t quota = base + (t == n_threads - 1 ? rest : 0);
        threads.emplace_back(generate_worker, std::cref(config), quota,
                             config.seed + t, std::ref(buffers[t]),
                             std::ref(val_buffers[t]), std::ref(progress));
    }

    for (auto& th : threads) th.join();
    done.store(true, std::memory_order_relaxed);
    monitor.join();

    samples.clear();
    val_samples.clear();
    samples.reserve(config.target_positions);
    for (auto& buf : buffers)
        samples.insert(samples.end(),
                       std::make_move_iterator(buf.begin()),
                       std::make_move_iterator(buf.end()));
    for (auto& buf : val_buffers)
        val_samples.insert(val_samples.end(),
                           std::make_move_iterator(buf.begin()),
                           std::make_move_iterator(buf.end()));

    std::cerr << "\nfinished: " << samples.size() << " train + "
              << val_samples.size() << " val positions\n";
}

/**
 * Writes a set of samples as versioned binary shards. Each shard file
 * (<prefix>-NNNNN.bin) starts with a DatasetHeader, followed by samples:
 * feature_len int8 planes (STM), side_to_move (u8), ply (u16), search_score,
 * final_value, td_value (i16 each), policy_count (u8) and NNUE_MAX_POLICY
 * policy entries (u16 move + u16 prob each; unused entries set to 0).
 * @param prefix ; const std::string& ; file prefix without shard suffix
 * @param samples ; const std::vector<NNUESample>& ; samples to write
 * @param config ; const NNUEDatasetConfig& ; provides board_size and shard_size
 * @return std::size_t ; number of shards written
 */
static std::size_t write_shards(const std::string& prefix,
                                const std::vector<NNUESample>& samples,
                                const NNUEDatasetConfig& config) {
    const int sq = config.squares();
    const int32_t flen = 2 * sq;
    const std::size_t total = samples.size();
    const std::size_t shard_size = std::max<std::size_t>(1, config.shard_size);
    const std::size_t n_shards = total == 0 ? 0 : (total + shard_size - 1) / shard_size;

    std::vector<int8_t> feat(static_cast<std::size_t>(flen));

    for (std::size_t shard = 0; shard < n_shards; ++shard) {
        const std::size_t begin = shard * shard_size;
        const std::size_t end = std::min(total, begin + shard_size);
        const uint32_t count = static_cast<uint32_t>(end - begin);

        char suffix[16];
        std::snprintf(suffix, sizeof(suffix), "-%05zu.bin", shard);
        const std::string path = prefix + suffix;
        std::ofstream out(path, std::ios::binary);

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

        for (std::size_t i = begin; i < end; ++i) {
            const NNUESample& s = samples[i];
            encode_stm(s.black, s.white, s.side_to_move, sq, feat.data());
            out.write(reinterpret_cast<const char*>(feat.data()), flen);
            out.write(reinterpret_cast<const char*>(&s.side_to_move), sizeof(s.side_to_move));
            out.write(reinterpret_cast<const char*>(&s.ply), sizeof(s.ply));
            out.write(reinterpret_cast<const char*>(&s.search_score), sizeof(s.search_score));
            out.write(reinterpret_cast<const char*>(&s.final_value), sizeof(s.final_value));
            out.write(reinterpret_cast<const char*>(&s.td_value), sizeof(s.td_value));
            out.write(reinterpret_cast<const char*>(&s.policy_count), sizeof(s.policy_count));
            for (int k = 0; k < NNUE_MAX_POLICY; ++k) {
                out.write(reinterpret_cast<const char*>(&s.policy[k].move), sizeof(uint16_t));
                out.write(reinterpret_cast<const char*>(&s.policy[k].prob), sizeof(uint16_t));
            }
        }
    }
    return n_shards;
}

/**
 * Writes training and validation samples into separate shard prefixes
 * (<path>-NNNNN.bin and <path>_val-NNNNN.bin).
 * @param config ; const NNUEDatasetConfig& ; provides output path and shard_size
 * @return void
 */
void NNUEDataset::save_dataset(const NNUEDatasetConfig& config) {
    const std::size_t train_shards = write_shards(config.dataset_path, samples, config);
    const std::size_t val_shards =
        write_shards(config.dataset_path + "_val", val_samples, config);

    std::cerr << "wrote " << train_shards << " train shard(s) / " << samples.size()
              << " samples, " << val_shards << " val shard(s) / "
              << val_samples.size() << " samples\n";
}