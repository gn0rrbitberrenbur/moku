#include "../include/board.hpp"

/**
 * This file implements the Board class defined in board.hpp, which represents the game state of Gomoku.
 * The Board class uses two bitsets to track the positions of black and white pieces on a 15x15 board.
 */

/**
 * Outputs the current state of the board to the console. Black pieces are represented by 'X', white pieces by 'O', and empty spaces by '.'.
 * @param None
 * @return void
 */
void Board::output_board() const
{
    std::cout << "   ";
    for (int i = 0; i < SIZE; i++) {
        char c = 'A' + i;
        std::cout << c << " ";
    }
    std::cout << std::endl;

    for (int row = 0; row < SIZE; row++) {
        if (row + 1 < 10) {
            std::cout << " " << row + 1 << " ";
        } else {
            std::cout << row + 1 << " ";
        }

        for (int col = 0; col < SIZE; col++) {
            int pos = row * SIZE + col;
            if (black[pos]) {
                std::cout << "X ";
            } else if (white[pos]) {
                std::cout << "O ";
            } else {
                std::cout << ". ";
            }
        }
        std::cout << std::endl;
    }
}

/**
 * Makes a move on the board at the specified position for either black or white.
 * @param pos The position on the board, represented as an index (0-224) where the move is to.
 * @param is_black A boolean indicating whether the move is for black (true) or white (false).
 * @return void
 */
void Board::make_move(int pos, bool is_black)
{
    if (pos < 0 || pos >= SIZE * SIZE) {
        return;
    }

    if (is_black) {
        black.set(pos);
    } else {
        white.set(pos);
    }
}

/**
 * Undoes a move on the board at the specified position, clearing both black and white pieces from that position.
 * @param pos The position on the board, represented as an index (0-224) where the move is to be undone.
 * @return void
 */
void Board::undo_move(int pos) {
        if (pos >= 0 && pos < SIZE * SIZE) {
            black.reset(pos);
            white.reset(pos);
    }
}

/**
 * Tests whether a given position on the board is occupied by either a black or white piece.
 * @param pos The position on the board, represented as an index (0-224) to be tested.
 * @return boolean ; true if the position is occupied by either a black or white piece, false otherwise
 */
bool Board::test_pos(int pos) const
{
    if (pos < 0 || pos >= SIZE * SIZE) {
        return false;
    }
    return black[pos] || white[pos];
}

/**
 * Checks if either player has won the game by having five pieces in a row. 
 * This function checks both the black and white bitsets for any sequence of five 
 * consecutive pieces in any direction (horizontal, vertical, diagonal).
 * @param None
 * @return boolean ; true if either player has won, false otherwise
 */
bool Board::check_win() const
{
    return check_five(black) || check_five(white);
}

/**
 * Checks if the given bitset (representing either black or white pieces) contains a sequence of five consecutive pieces in any direction.
 * @param board A bitset representing the positions of either black or white pieces on the board.
 * @return boolean ; true if there is a sequence of five consecutive pieces, false otherwise.
 */
bool Board::check_five(const std::bitset<SIZE * SIZE>& board) const
{
    for (int row = 0; row < SIZE; row++) {
        for (int col = 0; col < SIZE; col++) {
            int pos = row * SIZE + col;
            if (!board[pos]) continue;
            
            // horizontal
            if (col <= SIZE - 5) {
                bool win = true;
                for (int i = 0; i < 5; i++) {
                    if (!board[pos + i]) { win = false; break; }
                }
                if (win) return true;
            }
            
            // vertical
            if (row <= SIZE - 5) {
                bool win = true;
                for (int i = 0; i < 5; i++) {
                    if (!board[pos + i * SIZE]) { win = false; break; }
                }
                if (win) return true;
            }
            
            // diagonal right-down
            if (col <= SIZE - 5 && row <= SIZE - 5) {
                bool win = true;
                for (int i = 0; i < 5; i++) {
                    if (!board[pos + i * SIZE + i]) { win = false; break; }
                }
                if (win) return true;
            }
            
            // diagonal left-down
            if (col >= 4 && row <= SIZE - 5) {
                bool win = true;
                for (int i = 0; i < 5; i++) {
                    if (!board[pos + i * SIZE - i]) { win = false; break; }
                }
                if (win) return true;
            }
        }
    }
    return false;
}