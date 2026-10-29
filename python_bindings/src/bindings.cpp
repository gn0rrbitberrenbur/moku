#include "benchmark/benchmark.hpp"
#include "config.hpp"
#include "eval/evaluation.hpp"
#include "game/board.hpp"
#include "game/tools/algebraic_to_index.hpp"
#include "search/minimax.hpp"
#include "tt/transposition_table.hpp"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(_core, m)
{
    py::class_<Board>(m, "Board")
        .def(py::init<>())
        .def("make_move", &Board::make_move, py::arg("pos"), py::arg("is_black"))
        .def("undo_move", &Board::undo_move, py::arg("pos"))
        .def("test_pos", &Board::test_pos, py::arg("pos"))
        .def("check_win", &Board::check_win)
        .def("wins_at", &Board::wins_at, py::arg("pos"), py::arg("is_black"))
        .def("output_board", &Board::output_board)
        .def("size", &Board::size)
        .def("squares", &Board::squares)
        .def("pos", &Board::pos, py::arg("row"), py::arg("col"))
        .def("row", &Board::row, py::arg("pos"))
        .def("col", &Board::col, py::arg("pos"))
        .def("legal_moves",
             [](const Board& b)
             {
                 std::vector<int> moves;
                 int n = b.squares();
                 moves.reserve(n);
                 for (int p = 0; p < n; ++p)
                     if (!b.test_pos(p))
                         moves.push_back(p);
                 return moves;
             })
        .def("to_numpy",
             [](const Board& b)
             {
                 int s = b.size();
                 py::array_t<int8_t> arr({2, s, s});
                 auto buf = arr.mutable_unchecked<3>();
                 for (int r = 0; r < s; ++r)
                     for (int c = 0; c < s; ++c)
                     {
                         int p = b.pos(r, c);
                         buf(0, r, c) = b.black[p] ? 1 : 0;
                         buf(1, r, c) = b.white[p] ? 1 : 0;
                     }
                 return arr;
             });

    py::class_<RootCandidate>(m, "RootCandidate")
        .def_readonly("move", &RootCandidate::move)
        .def_readonly("score", &RootCandidate::score)
        .def_readonly("nodes", &RootCandidate::nodes);

    py::class_<MinimaxAgent>(m, "MinimaxAgent")
        .def(py::init<int>(), py::arg("depth") = 6)
        .def("get_best_move", &MinimaxAgent::get_best_move, py::arg("board"), py::arg("is_black"))
        .def("get_best_move_timed", &MinimaxAgent::get_best_move_timed, py::arg("board"),
             py::arg("is_black"), py::arg("time_limit_ms"))
        .def("get_best_move_timed_smp", &MinimaxAgent::get_best_move_timed_smp, py::arg("board"),
             py::arg("is_black"), py::arg("time_limit_ms"))
        .def("set_max_depth", &MinimaxAgent::set_max_depth, py::arg("depth"))
        .def("get_max_depth", &MinimaxAgent::get_max_depth)
        .def("get_last_root_score", &MinimaxAgent::get_last_root_score)
        .def("get_last_root_candidates", &MinimaxAgent::get_last_root_candidates,
             py::return_value_policy::reference_internal)
        .def("get_nodes_searched", &MinimaxAgent::get_nodes_searched)
        .def("get_tt_hits", &MinimaxAgent::get_tt_hits)
        .def("clear_tt", &MinimaxAgent::clear_tt);

    py::class_<Evaluator>(m, "Evaluator")
        .def(py::init<>())
        .def("evaluate_board", &Evaluator::evaluate_board, py::arg("board"))
        .def("evaluate_board_stm", &Evaluator::evaluate_board_stm, py::arg("board"),
             py::arg("black_to_move"))
        .def("get_valid_moves", &Evaluator::get_valid_moves, py::arg("board"))
        .def("move_priority", &Evaluator::move_priority, py::arg("board"), py::arg("move"),
             py::arg("is_black"))
        .def("position_score", &Evaluator::position_score, py::arg("pos"));

    py::class_<TranspositionTable>(m, "TranspositionTable")
        .def(py::init<>())
        .def("compute_hash", &TranspositionTable::compute_hash, py::arg("board"),
             py::arg("is_black_turn"))
        .def("update_hash", &TranspositionTable::update_hash, py::arg("hash"), py::arg("pos"),
             py::arg("is_black"))
        .def("clear", &TranspositionTable::clear)
        .def("size", &TranspositionTable::size);

    py::class_<GameConfig>(m, "GameConfig")
        .def_readwrite("board_size", &GameConfig::board_size)
        .def_readwrite("max_depth", &GameConfig::max_depth)
        .def_readwrite("time_limit_ms", &GameConfig::time_limit_ms)
        .def_readwrite("debug_output", &GameConfig::debug_output)
        .def_readwrite("version", &GameConfig::version)
        .def_readwrite("cores", &GameConfig::cores)
        .def("squares", &GameConfig::squares);

    m.def(
        "get_config", []() -> GameConfig& { return g_config; }, py::return_value_policy::reference);

    m.def(
        "set_board_size",
        [](int n)
        {
            if (n < 5 || n > MAX_SIZE)
                throw std::out_of_range("board size must be 5..19");
            g_config.board_size = n;
        },
        py::arg("n"));
    m.def("get_board_size", []() { return g_config.board_size; });
    m.def("algebraic_to_index", &algebraic_to_index, py::arg("input"), py::arg("size") = 0);

    py::class_<BenchResult>(m, "BenchResult")
        .def_readonly("name", &BenchResult::name)
        .def_readonly("reached_depth", &BenchResult::reached_depth)
        .def_readonly("nodes", &BenchResult::nodes)
        .def_readonly("tt_hits", &BenchResult::tt_hits)
        .def_readonly("tt_size", &BenchResult::tt_size)
        .def_readonly("time_s", &BenchResult::time_s)
        .def_readonly("best_move", &BenchResult::best_move)
        .def_readonly("score", &BenchResult::score);

    m.def("run_position", &run_position, py::arg("name"), py::arg("setup"), py::arg("side_to_move"),
          py::arg("depth"), py::arg("time_ms"), py::arg("board_size") = 15);
    m.def("run_benchmark", &run_benchmark, py::arg("depth"), py::arg("time_ms"));
}