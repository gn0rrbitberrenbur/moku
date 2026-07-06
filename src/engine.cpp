#include "../include/engine.hpp"
#include "../include/utils.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "config.hpp"

/**
 * This file implements the Engine class defined in ../include/engine.hpp.
 * The Engine class manages the overall game flow in engine mode and interacts with the Board and AI agents.
 * The engine processes commands according to the Gomocup Protocol, 
 * see https://plastovicka.github.io/protocl2en.htm
 */

/**
 * This function runs the main loop of the engine.
 * It continuously reads commands from standard input, processes them, and responds 
 * according to the Gomocup Protocol.
 * The loop continues until an END command is received or the input stream is closed.
 * @param None
 * @return void
 */
void Engine::run() {
    std::string line;
    while (running && std::getline(std::cin, line)) {
        if (!line.empty()) {
            process_command(line);
            std::cout.flush();  // important: flush after every response
        }
    }
}

void Engine::process_command(const std::string& line) {
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;
    
    // convert to uppercase
    std::string cmd_upper = cmd;
    for (auto& c : cmd_upper) c = toupper(c);
    
    if (cmd_upper == "START") {
        cmd_start(iss);
    }
    else if (cmd_upper == "BEGIN") {
        cmd_begin();
    }
    else if (cmd_upper == "TURN") {
        cmd_turn(iss);
    }
    else if (cmd_upper == "BOARD") {
        cmd_board();
    }
    else if (cmd_upper == "INFO") {
        cmd_info(iss);
    }
    else if (cmd_upper == "END") {
        running = false;
    }
    else if (cmd_upper == "RESTART") {
        board = Board();
        std::cout << "OK" << std::endl;
    }
    else if (cmd_upper == "TAKEBACK") {
        cmd_takeback(iss);
    }
    else if (cmd_upper == "RECTSTART") {
        // rectangular board not supported in current implementation
        std::cout << "ERROR rectangular board not supported" << std::endl;
    }
    // debugging commands, not part of gomocup protocol
    else if (cmd_upper == "DISPLAY" || cmd_upper == "D") {
        cmd_display();
    }
    else if (cmd_upper == "HELP") {
        cmd_help();
    }
    else {
        std::cout << "UNKNOWN " << cmd << std::endl;
    }
}

/**
 * This function processes the START command, which initializes the board with a given size.
 * START [size] - Initialize board with given size
 * Must respond with OK or ERROR
 * @param iss An input string stream containing the command arguments
 * @return void
 */
void Engine::cmd_start(std::istringstream& iss) {
    int size;
    if (!(iss >> size)) {
        size = 15;  // Default
    }
    
    // check for valid size (5-20)
    if (size < 5 || size > 20) {
        std::cout << "ERROR unsupported board size " << size << std::endl;
        std::cout.flush();  // FLUSH
        return;
    }
    
    // set board size in global config and reinitialize board
    g_config.board_size = size;
    board = Board();
    
    std::cout << "OK" << std::endl;
    std::cout.flush();  // FLUSH
}

/**
 * This fucntion processes the BEGIN command, which signals that the engine 
 * should play the first move (as black).
 * The engine must respond with the coordinates of its move in the format X,Y.
 * BEGIN - Brain plays first move (opens the game)
 * Must respond with X,Y coordinates
 * @param None
 * @return void
 */
void Engine::cmd_begin() {
    int center = g_config.board_size / 2;
    int best_move;
    
    if (board.black.count() == 0 && board.white.count() == 0) {
        best_move = center * g_config.board_size + center;
    } else {
        int search_time = (timeout_turn > 0) ? std::max(100, timeout_turn - 500) : 5000;
        best_move = agent.get_best_move_timed(board, true, search_time);
        if (best_move < 0) {
            best_move = center * g_config.board_size + center;
        }
    }
    
    int x = best_move % g_config.board_size;
    int y = best_move / g_config.board_size;
    
    board.make_move(best_move, true);
    
    std::cout << x << "," << y << std::endl;
    std::cout.flush();
}

/**
 * This function processes the TURN command, which indicates that the opponent has 
 * made a move and its now the engines turn to respond.
 * The command includes the opponent's move coordinates, which the engine must parse 
 * and update its internal board state with.
 * TURN X,Y - Opponent made a move, now it's brain's turn
 * Must respond with X,Y coordinates
 * @param iss An input string stream containing the command arguments
 * @return void
 */
void Engine::cmd_turn(std::istringstream& iss) {
    std::string coords;
    iss >> coords;
    
    int x, y;
    char comma;
    std::istringstream coord_stream(coords);
    if (!(coord_stream >> x >> comma >> y) || comma != ',') {
        std::cout << "ERROR invalid coordinates" << std::endl;
        std::cout.flush();
        return;
    }
    
    int pos = y * g_config.board_size + x;
    
    if (g_config.debug_output == true) {
        std::cerr << "[DEBUG TURN] pos=" << pos << " board_size=" << g_config.board_size << std::endl;
    }
    
    if (pos < 0 || pos >= g_config.squares()) {
        std::cout << "ERROR coordinates out of bounds" << std::endl;
        std::cout.flush();
        return;
    }

    if (board.test_pos(pos)) {
        std::cout << "ERROR position already occupied" << std::endl;
        std::cout.flush();
        return;
    }
    
    bool is_black = (board.black.count() == board.white.count());
    if (g_config.debug_output == true) {
        std::cerr << "[DEBUG TURN] is_black=" << is_black 
                << " black_count=" << board.black.count()
                << " white_count=" << board.white.count() << std::endl;
    }

    
    board.make_move(pos, is_black);
    
    if (g_config.debug_output == true) {
        std::cerr << "[DEBUG TURN] after opponent move - black=" << board.black.count()
                << " white=" << board.white.count() << std::endl;
    }
    
    
    bool my_color = !is_black;
    int search_time = (timeout_turn > 0) ? std::max(100, timeout_turn - 500) : 5000;
    int best_move = agent.get_best_move_timed(board, my_color, search_time);
    
    if (best_move < 0) {
        std::cout << "ERROR no valid move found" << std::endl;
        std::cout.flush();
        return;
    }
    
    board.make_move(best_move, my_color);
    
    int out_x = best_move % g_config.board_size;
    int out_y = best_move / g_config.board_size;
    
    std::cout << out_x << "," << out_y << std::endl;
    std::cout.flush();  // FLUSH
}

/**
 * This function processes the BOARD command, which provides the complete current board state.
 * BOARD - Receive complete board state
 * @param None
 * @return void
 */
void Engine::cmd_board() {
    board = Board();
    
    std::vector<std::pair<int, int>> own_moves;
    std::vector<std::pair<int, int>> opp_moves;
    
    std::string line;
    while (std::getline(std::cin, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
            line.pop_back();
        }
        
        if (line == "DONE") {
            break;
        }
        
        int x, y, field;
        char c1, c2;
        std::istringstream lss(line);
        if (lss >> x >> c1 >> y >> c2 >> field) {
            int pos = y * g_config.board_size + x;
            if (pos >= 0 && pos < g_config.squares()) {
                if (field == 1) {
                    own_moves.push_back({pos, 1});
                } else if (field == 2) {
                    opp_moves.push_back({pos, 2});
                }
            }
        }
    }
    
    bool we_are_black = (own_moves.size() > opp_moves.size()) || 
                        (own_moves.size() == opp_moves.size());
    
    for (const auto& move : own_moves) {
        board.make_move(move.first, we_are_black);
    }
    for (const auto& move : opp_moves) {
        board.make_move(move.first, !we_are_black);
    }
    
    bool is_black = (board.black.count() == board.white.count());
    int search_time = (timeout_turn > 0) ? std::max(100, timeout_turn - 500) : 5000;
    int best_move = agent.get_best_move_timed(board, is_black, search_time);
    
    if (best_move >= 0) {
        board.make_move(best_move, is_black);
        int out_x = best_move % g_config.board_size;
        int out_y = best_move / g_config.board_size;
        std::cout << out_x << "," << out_y << std::endl;
    } else {
        std::cout << "ERROR no valid move" << std::endl;
    }
    std::cout.flush();  // FLUSH
}

/**
 * This function processes the TAKEBACK command, which allows undoing a move at a specified position.
 * TAKEBACK X,Y - Undo move at position
 * Must respond with OK or ERROR
 * @param iss An input string stream containing the command arguments
 * @return void
 */
void Engine::cmd_takeback(std::istringstream& iss) {
    std::string coords;
    iss >> coords;
    
    int x, y;
    char comma;
    std::istringstream coord_stream(coords);
    if (!(coord_stream >> x >> comma >> y) || comma != ',') {
        std::cout << "ERROR invalid coordinates" << std::endl;
        std::cout.flush();
        return;
    }
    
    int pos = y * g_config.board_size + x;
    
    if (board.black.test(pos)) {
        board.black.reset(pos);
        std::cout << "OK" << std::endl;
    } else if (board.white.test(pos)) {
        board.white.reset(pos);
        std::cout << "OK" << std::endl;
    } else {
        std::cout << "ERROR no stone at position" << std::endl;
    }
    std::cout.flush();  // FLUSH
}

/**
 * This function processes the INFO command, which provides various game information 
 * and parameters to the engine.
 * The command includes a key and value, which the engine must parse and update its internal state
 * INFO key value - Receive game information
 * No response expected
 * @param iss An input string stream containing the command arguments
 * @return void
 */
void Engine::cmd_info(std::istringstream& iss) {
    std::string key;
    iss >> key;
    
    if (key == "timeout_turn") {
        iss >> timeout_turn;
    }
    else if (key == "timeout_match") {
        iss >> timeout_match;
    }
    else if (key == "time_left") {
        iss >> time_left;
    }
    else if (key == "max_memory") {
        iss >> max_memory;
    }
    else if (key == "game_type") {
        iss >> game_type;
    }
    else if (key == "rule") {
        iss >> rule;
    }
    else if (key == "folder") {
        iss >> folder;
    }
    else if (key == "depth") {
        int depth;
        if (iss >> depth && depth > 0 && depth <= 20) {
            search_depth = depth;
            agent.set_max_depth(depth);
            std::cerr << "[info] Search depth set to " << depth << std::endl;
        }
    }
    // no answer expected for INFO command
}

/**
 * This function processes the DISPLAY command, which is a debug command to output the 
 * current board state and some internal information.
 * DISPLAY - Debug command to display board and internal info
 * No response expected
 * This is not part of the Gomocup Protocol.
 * @param None
 * @return void
 */
void Engine::cmd_display() {
    board.output_board();
    std::cout << "DEBUG Black stones: " << board.black.count() << std::endl;
    std::cout << "DEBUG White stones: " << board.white.count() << std::endl;
}

/**
 * This function processes the DISPLAY command, which is a debug command to output the current 
 * board state and some internal information.
 * DISPLAY - Debug command to display board and internal info
 * No response expected
 * This is not part of the Gomocup Protocol.
 * @param None
 * @return void
 */
void Engine::cmd_help() {
    std::cout << "MESSAGE Gomocup Protocol Commands:" << std::endl;
    std::cout << "MESSAGE   START [size] - Initialize board" << std::endl;
    std::cout << "MESSAGE   BEGIN - Play first move" << std::endl;
    std::cout << "MESSAGE   TURN X,Y - Opponent move, respond with own" << std::endl;
    std::cout << "MESSAGE   BOARD ... DONE - Set board state" << std::endl;
    std::cout << "MESSAGE   INFO key value - Set parameters" << std::endl;
    std::cout << "MESSAGE   ABOUT - Brain information" << std::endl;
    std::cout << "MESSAGE   END - Terminate" << std::endl;
}