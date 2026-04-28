#include "../include/engine.hpp"
#include "../include/utils.hpp"
#include <iostream>
#include <sstream>
#include <string>

void Engine::run() {
    std::cout << "Gomoku Engine v1.0" << std::endl;
    std::cout << "Type 'help' for commands" << std::endl;
    
    std::string line;
    while (running && std::getline(std::cin, line)) {
        if (!line.empty()) {
            process_command(line);
        }
    }
}

void Engine::process_command(const std::string& line) {
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;
    
    if (cmd == "ugi") {
        // Universal Gomoku Interface - Identifikation
        std::cout << "id name GomokuEngine" << std::endl;
        std::cout << "id author YourName" << std::endl;
        std::cout << "ugiok" << std::endl;
    }
    else if (cmd == "isready") {
        std::cout << "readyok" << std::endl;
    }
    else if (cmd == "uginewgame" || cmd == "newgame") {
        board = Board();
        std::cout << "info string New game started" << std::endl;
    }
    else if (cmd == "position") {
        cmd_position(iss);
    }
    else if (cmd == "go") {
        cmd_go();
    }
    else if (cmd == "move" || cmd == "play") {
        std::string move_str;
        iss >> move_str;
        cmd_move(move_str);
    }
    else if (cmd == "display" || cmd == "d" || cmd == "print") {
        cmd_display();
    }
    else if (cmd == "depth") {
        int d;
        if (iss >> d && d > 0 && d <= 10) {
            search_depth = d;
            agent = MinimaxAgent(search_depth);
            std::cout << "info string Depth set to " << d << std::endl;
        }
    }
    else if (cmd == "undo") {
        // Benötigt Move-History im Board
        std::cout << "info string Undo not implemented" << std::endl;
    }
    else if (cmd == "quit" || cmd == "exit") {
        running = false;
    }
    else if (cmd == "help") {
        cmd_help();
    }
    else {
        // Versuche als Zug zu interpretieren
        cmd_move(cmd);
    }
}

void Engine::cmd_position(std::istringstream& iss) {
    std::string token;
    iss >> token;
    
    if (token == "startpos") {
        board = Board();
        iss >> token;  // "moves" überspringen falls vorhanden
    }
    
    if (token == "moves") {
        bool is_black = true;
        std::string move_str;
        while (iss >> move_str) {
            int pos = algebraic_to_index(move_str);
            if (pos >= 0 && pos < 225) {
                board.make_move(pos, is_black);
                is_black = !is_black;
            }
        }
    }
    std::cout << "info string Position set" << std::endl;
}

void Engine::cmd_go() {
    // Bestimme wer dran ist (zähle Steine)
    bool is_black = (board.black.count() == board.white.count());
    
    std::cout << "info string Searching depth " << search_depth << std::endl;
    
    int best_move = agent.get_best_move(board, is_black);
    
    if (best_move >= 0) {
        char col = 'A' + (best_move % 15);
        int row = (best_move / 15) + 1;
        std::cout << "bestmove " << col << row << std::endl;
    } else {
        std::cout << "bestmove none" << std::endl;
    }
}

void Engine::cmd_move(const std::string& move_str) {
    int pos = algebraic_to_index(move_str);
    
    if (pos < 0 || pos >= 225) {
        std::cout << "info string Invalid move: " << move_str << std::endl;
        return;
    }
    
    if (board.test_pos(pos)) {
        std::cout << "info string Position occupied: " << move_str << std::endl;
        return;
    }
    
    bool is_black = (board.black.count() == board.white.count());
    board.make_move(pos, is_black);
    
    std::cout << "info string Move " << move_str << " played" << std::endl;
    
    if (board.check_win()) {
        std::cout << "info string " << (is_black ? "Black" : "White") << " wins!" << std::endl;
    }
}

void Engine::cmd_display() {
    board.output_board();
    std::cout << "info string Black stones: " << board.black.count() << std::endl;
    std::cout << "info string White stones: " << board.white.count() << std::endl;
}

void Engine::cmd_help() {
    std::cout << "Commands:" << std::endl;
    std::cout << "  ugi          - Engine identification" << std::endl;
    std::cout << "  isready      - Check if ready" << std::endl;
    std::cout << "  newgame      - Start new game" << std::endl;
    std::cout << "  position startpos [moves ...]" << std::endl;
    std::cout << "  go           - Calculate best move" << std::endl;
    std::cout << "  move <pos>   - Play move (e.g., H8)" << std::endl;
    std::cout << "  <pos>        - Play move directly" << std::endl;
    std::cout << "  display      - Show board" << std::endl;
    std::cout << "  depth <n>    - Set search depth (1-10)" << std::endl;
    std::cout << "  quit         - Exit engine" << std::endl;
}