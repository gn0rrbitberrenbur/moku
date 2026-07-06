#include "../include/gameloop.hpp"
#include "../include/minimax/minimax.hpp"
#include "../include/utils.hpp"
#include <iostream>
#include <string>
#include "config.hpp"

/**
 * This file implements the Gameloop class defined in ../include/gameloop.hpp.
 * The Gameloop class contains the main game loop functions for both player vs player and 
 * player vs minimax agent modes.
 */

 /**
  * This function implements the game loop for player vs player mode. 
  * It alternates turns between two human players, accepting input for moves and 
  * updating the board state until one player wins or the game is quit.
  * @param board A reference to the Board object representing the game state.
  * @return void
  */
void game_loop_pvp(Board& board)
{
    bool is_black_turn = true;
    int move_count = 1;
    int SIZE = g_config.board_size;

    while (true)
    {   
        std::cout << "[info] Turn no. " << move_count << " " 
                  << (is_black_turn ? "Black" : "White") << " turn." << std::endl;
        
        board.output_board();
        std::string input;
        std::cout << "[input] Enter a move (e.g., A1, B2): ";
        std::cin >> input;
        
        if (input == "q" || input == "quit") {
            std::cout << "[info] Game ended." << std::endl;
            return;
        }
        
        int pos = algebraic_to_index(input, g_config.board_size);

        if (pos < 0 || pos >= SIZE * SIZE) {
            std::cout << "[error] Invalid input: " << input << std::endl;
            continue;
        } 
        
        if (board.test_pos(pos)) {
            std::cout << "[error] Position already occupied." << std::endl;
            continue;
        }
        
        board.make_move(pos, is_black_turn);
        
        if (board.check_win()) {
            board.output_board();
            std::cout << (is_black_turn ? "Black" : "White") << " wins!" << std::endl;
            return;
        }
        
        is_black_turn = !is_black_turn;
        move_count++;
    }
}

/**
 * This function implements the game loop for player vs minimax agent mode. 
 * It alternates turns between a human player and a minimax AI agent, accepting input for
 * human moves and calculating AI moves until one player wins or the game is quit.
 * @param board A reference to the Board object representing the game state.
 * @return void
 */
void game_loop_minimax(Board& board)
{
    bool is_black_turn = true;
    bool is_human_black = true;
    int move_count = 1;
    int SIZE = g_config.board_size;
    MinimaxAgent ai(g_config.max_depth);

    while (true)
    {
        std::cout << "[info] Turn no. " << move_count << " " 
                  << (is_black_turn ? "Black" : "White") << " turn." << std::endl;
        
        board.output_board();
        
        if ((is_black_turn && is_human_black) || (!is_black_turn && !is_human_black)) {
            // human turn
            std::string input;
            std::cout << "[input] Enter a move (e.g., A1, B2): ";
            std::cin >> input;
            
            if (input == "q" || input == "quit") {
                std::cout << "[info] Game ended." << std::endl;
                return;
            }
            
            int pos = algebraic_to_index(input, g_config.board_size);

            if (pos < 0 || pos >= SIZE * SIZE) {
                std::cout << "[error] Invalid input: " << input << std::endl;
                continue;
            } 
            
            if (board.test_pos(pos)) {
                std::cout << "[error] Position already occupied." << std::endl;
                continue;
            }
            
            board.make_move(pos, is_black_turn);
        } else {
            // ai turn with time limit
            int best_move = ai.get_best_move_timed(board, is_black_turn, g_config.time_limit_ms);
            
            std::cout << "[info] AI plays: " << (char)('A' + best_move % SIZE) << (best_move / SIZE + 1) << std::endl;
            board.make_move(best_move, is_black_turn);
        }
        
        // check for win
        if (board.check_win()) {
            board.output_board();
            std::cout << (is_black_turn ? "Black" : "White") << " wins!" << std::endl;
            return;
        }
        
        // next player
        is_black_turn = !is_black_turn;
        move_count++;
    }
}