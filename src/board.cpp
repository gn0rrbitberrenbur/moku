#include "../include/board.hpp"

/**
 * This file implements the Board class defined in ../include/board.hpp, which represents the game state of Gomoku.
 * The Board class uses two bitsets to track the positions of black and white pieces on a 15x15 board.
 */

/**
 * Outputs the current state of the board to the console. Black pieces are represented by 'X', white pieces by 'O', and empty spaces by '.'.
 * @param None
 * @return void
 */
void Board::output_board() const
{
    int s = size();
    
    // Header
    std::cout << "   ";
    for (int c = 0; c < s; c++) {
        std::cout << (char)('A' + c) << " ";
    }
    std::cout << "\n";
    
    // Board
    for (int r = 0; r < s; r++) {
        std::cout << (r + 1 < 10 ? " " : "") << (r + 1) << " ";
        for (int c = 0; c < s; c++) {
            int p = pos(r, c);
            if (black[p]) std::cout << "X ";
            else if (white[p]) std::cout << "O ";
            else std::cout << ". ";
        }
        std::cout << "\n";
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
    if (is_black) black.set(pos);
    else white.set(pos);
}

/**
 * Undoes a move on the board at the specified position, clearing both black and white pieces from that position.
 * @param pos The position on the board, represented as an index (0-224) where the move is to be undone.
 * @return void
 */
void Board::undo_move(int pos) {
    black.reset(pos);
    white.reset(pos);
}

/**
 * Tests whether a given position on the board is occupied by either a black or white piece.
 * @param pos The position on the board, represented as an index (0-224) to be tested.
 * @return boolean ; true if the position is occupied by either a black or white piece, false otherwise
 */
bool Board::test_pos(int pos) const
{
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
bool Board::check_five(const std::bitset<MAX_SQUARES>& pieces) const
{
    for (int p = 0; p < squares(); p++) {
        if (!pieces[p]) continue;
        
        int r = row(p);
        int c = col(p);
        
        // 4 directions: horizontal, vertical, diagonal down-right, diagonal up-right
        static const int dx[] = {1, 0, 1, 1};
        static const int dy[] = {0, 1, 1, -1};
        
        for (int dir = 0; dir < 4; dir++) {
            int count = 1;
            
            // count forward
            for (int i = 1; i < 5; i++) {
                int nr = r + dy[dir] * i;
                int nc = c + dx[dir] * i;
                if (!in_bounds(nr, nc) || !pieces[pos(nr, nc)]) break;
                count++;
            }
            
            if (count >= 5) return true;
        }
    }
    return false;
}

// new
bool Board::wins_at(int move, bool is_black) const
{
    const std::bitset<MAX_SQUARES>& pieces = is_black ? black : white;
    int r = row(move);
    int c = col(move);

    static const int dx[] = {1, 0, 1, 1};
    static const int dy[] = {0, 1, 1, -1};

    for (int dir = 0; dir < 4; dir++) {
        int count = 1;
        for (int s = -1; s <= 1; s += 2) {
            int nr = r + dy[dir] * s;
            int nc = c + dx[dir] * s;
            while (in_bounds(nr, nc) && pieces[pos(nr, nc)]) {
                count++;
                nr += dy[dir] * s;
                nc += dx[dir] * s;
            }
        }
        if (count >= 5) return true;
    }
    return false;
}