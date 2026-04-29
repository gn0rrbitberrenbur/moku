#include "../include/engine.hpp"
#include "../include/utils.hpp"
#include <iostream>
#include <sstream>
#include <string>

/**
 * This file implements the Engine class defined in ../include/engine.hpp.
 * The Engine class manages the overall game flow in engine mode and interacts with the Board and AI agents.
 */

/**
 * Main loop of the engine, processes commands from standard input and responds accordingly.
 * Supports UGI commands for interoperability with GUI clients, as well as custom commands for testing and debugging.
 * @param None
 * @return void
 */
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

/**
 * Processes a single command line input, parsing the command and its arguments, and executing the corresponding function.
 * Supports commands for engine identification, readiness check, game management, move input, board display,
 * searching for best move, and setting search depth. Also includes a help command to list available commands.
 * @param line The input command line to be processed.
 * @return void
 */
void Engine::process_command(const std::string& line) {
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;
    
    if (cmd == "ugi") {
        // Universal Gomoku Interface - Identification
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
        // Needs move history to implement properly, for now just print info
        std::cout << "info string Undo not implemented" << std::endl;
    }
    else if (cmd == "quit" || cmd == "exit") {
        running = false;
    }
    else if (cmd == "help") {
        cmd_help();
    }
    else {
        // try to interpret as move
        cmd_move(cmd);
    }
}

/**
 * Handles the 'position' command, which sets up the board state based on a series of moves.
 * The command can specify a starting position (startpos) and a sequence of moves to apply
 * to the board. This allows the engine to be set up in any arbitrary state for testing or analysis.
 * @param iss An input string stream containing the arguments of the position command.
 * @return void
 */
void Engine::cmd_position(std::istringstream& iss) {
    std::string token;
    iss >> token;
    
    if (token == "startpos") {
        board = Board();
        iss >> token;  // skip "moves" if present
    }
    
    if (token == "moves") {
        bool is_black = true;
        std::string move_str;
        while (iss >> move_str) {
            int pos = algebraic_to_index(move_str, Board::SIZE);
            if (pos >= 0 && pos < Board::SIZE * Board::SIZE) {
                board.make_move(pos, is_black);
                is_black = !is_black;
            }
        }
    }
    std::cout << "info string Position set" << std::endl;
}

/**
 * Handles the 'go' command, which triggers the engine to calculate the best move for the current player 
 * using the Minimax algorithm.
 * @param None
 * @return void
 */
void Engine::cmd_go() {
    // determine current player based on move counts
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

/**
 * Handles the 'move' command, which allows the user to input a move in algebraic notation (e.g., H8) 
 * and applies it to the board.
 * The function checks for move validity, updates the board state, and checks for a win condition after the move is made.
 * @param move_str The move in algebraic notation to be played.
 * @return void
 */
void Engine::cmd_move(const std::string& move_str) {
    int pos = algebraic_to_index(move_str, Board::SIZE);
    
    if (pos < 0 || pos >= Board::SIZE * Board::SIZE) {
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

/**
 * Handles the 'display' command, which outputs the current state of the board to the console in a human-readable format.
 * It also provides additional information about the number of black and white pieces currently on the board.
 * @param None
 * @return void
 */
void Engine::cmd_display() {
    board.output_board();
    std::cout << "info string Black stones: " << board.black.count() << std::endl;
    std::cout << "info string White stones: " << board.white.count() << std::endl;
}

/**
 * Handles the 'help' command, which lists all available commands and their descriptions to assist 
 * the user in interacting with the engine.
 * @param None
 * @return void
 */
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