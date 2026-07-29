#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include "../include/board.hpp"
#include "../include/minimax/minimax.hpp"
#include "../include/minimax/transposition_table.hpp"
#include "../include/config.hpp"
#include "../include/utils.hpp"

namespace py = pybind11;

PYBIND11_MODULE(pymoku, m) {
    py::class_<Board>(m, "Board")
        .def(py::init<>())
        .def("make_move", &Board::make_move, py::arg("pos"), py::arg("is_black"))
        .def("undo_move", &Board::undo_move, py::arg("pos"))
        .def("test_pos", &Board::test_pos, py::arg("pos"))
        .def("check_win", &Board::check_win)
        .def("output_board", &Board::output_board)
        .def("size", &Board::size)
        .def("squares", &Board::squares)
        .def("pos", &Board::pos, py::arg("row"), py::arg("col"))
        .def("legal_moves", [](const Board& b) {
            std::vector<int> moves;
            int n = b.squares();
            moves.reserve(n);
            for (int p = 0; p < n; ++p)
                if (!b.test_pos(p)) moves.push_back(p);
            return moves;
        }
    )
        .def("to_numpy", [](const Board& b) {
            int s = b.size();
            py::array_t<int8_t> arr({2, s, s});
            auto buf = arr.mutable_unchecked<3>();
            for (int r = 0; r < s; ++r)
                for (int c = 0; c < s; ++c) {
                    int p = b.pos(r, c);
                    buf(0, r, c) = b.black[p] ? 1 : 0;
                    buf(1, r, c) = b.white[p] ? 1 : 0;
                }
            return arr;
        }
    );

    py::class_<MinimaxAgent>(m, "MinimaxAgent")
        .def(py::init<int>(), py::arg("depth") = 6)
        .def("get_best_move", &MinimaxAgent::get_best_move,
             py::arg("board"), py::arg("is_black"))
        .def("get_best_move_timed", &MinimaxAgent::get_best_move_timed,
             py::arg("board"), py::arg("is_black"), py::arg("time_limit_ms"))
        .def("set_max_depth", &MinimaxAgent::set_max_depth, py::arg("depth"))
        .def("get_max_depth", &MinimaxAgent::get_max_depth)
        .def("get_nodes_searched", &MinimaxAgent::get_nodes_searched)
        .def("get_tt_hits", &MinimaxAgent::get_tt_hits)
        .def("clear_tt", &MinimaxAgent::clear_tt);

    py::class_<TranspositionTable>(m, "TranspositionTable")
        .def(py::init<>())
        .def("compute_hash", &TranspositionTable::compute_hash,
             py::arg("board"), py::arg("is_black_turn"))
        .def("update_hash", &TranspositionTable::update_hash,
             py::arg("hash"), py::arg("pos"), py::arg("is_black"))
        .def("clear", &TranspositionTable::clear)
        .def("size", &TranspositionTable::size);

    m.def("set_board_size", [](int n) {
        if (n < 5 || n > MAX_SIZE)
            throw std::out_of_range("board size must be 5..19");
        g_config.board_size = n;
    }, py::arg("n"));
    m.def("get_board_size", []() { return g_config.board_size; });
    m.def("algebraic_to_index", &algebraic_to_index,
          py::arg("input"), py::arg("size") = 0);
}