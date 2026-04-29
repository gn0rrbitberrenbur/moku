#include "../include/gameloop.hpp"
#include "../include/agents/minimax.hpp"
#include "../include/utils.hpp"
#include <iostream>
#include <string>

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
    int SIZE = 15;

    while (true)
    {   
        std::cout << "Turn no. " << move_count << " " 
                  << (is_black_turn ? "Black" : "White") << " turn." << std::endl;
        
        board.output_board();
        std::string input;
        std::cout << "Enter a move (e.g., A1, B2): ";
        std::cin >> input;
        
        if (input == "q" || input == "quit") {
            std::cout << "Game ended." << std::endl;
            return;
        }
        
        int pos = algebraic_to_index(input);

        if (pos < 0 || pos >= SIZE * SIZE) {
            std::cout << "Invalid input: " << input << std::endl;
            continue;
        } 
        
        if (board.test_pos(pos)) {
            std::cout << "Position already occupied." << std::endl;
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
    int SIZE = 15;
    MinimaxAgent ai(4);

    while (true)
    {
        std::cout << "Turn no. " << move_count << " " 
                  << (is_black_turn ? "Black" : "White") << " turn." << std::endl;
        
        board.output_board();
        
        if ((is_black_turn && is_human_black) || (!is_black_turn && !is_human_black)) {
            // human moves
            std::string input;
            std::cout << "Enter a move (e.g., A1, B2): ";
            std::cin >> input;
            
            if (input == "q" || input == "quit") {
                std::cout << "Game ended." << std::endl;
                return;
            }
            
            int pos = algebraic_to_index(input);

            if (pos < 0 || pos >= SIZE * SIZE) {
                std::cout << "Invalid input: " << input << std::endl;
                continue;
            } 
            
            if (board.test_pos(pos)) {
                std::cout << "Position already occupied." << std::endl;
                continue;
            }
            
            board.make_move(pos, is_black_turn);
        } else {
            // AI moves
            int ai_move = ai.get_best_move(board, is_black_turn);
            std::cout << "AI plays: " << (char)('A' + ai_move % SIZE) << (ai_move / SIZE + 1) << std::endl;
            board.make_move(ai_move, is_black_turn);
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