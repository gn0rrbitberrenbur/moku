#include "board/board.hpp"

#include <array>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace cli
{
/**
 * Board constructor that initializes the board with a given size and rule.
 * @param size The size of the board (must be between 5 and 19).
 * @param rule The rule to be used for the game (default is Freestyle).
 * @throws std::invalid_argument if the size is not between 5 and 19.
 */
Board::Board(int size, Rule rule) : size_(size), rule_(rule)
{
    if (size < 5 || size > 19)
    {
        throw std::invalid_argument("Board size must be between 5 and 19.");
    }
    cells_.assign(static_cast<std::size_t>(size * size), Stone::Empty);
}

/**
 * Returns the size of the board.
 * @return The size of the board.
 */
int Board::size() const
{
    return size_;
}

/**
 * Returns the number of moves played on the board.
 * @return The number of moves played.
 */
int Board::moves() const
{
    return moves_;
}

/**
 * Checks if a given move is within the bounds of the board.
 * @param move The move to check.
 * @return True if the move is within the bounds, false otherwise.
 */
bool Board::contains(Move move) const
{
    return move.x >= 0 && move.y >= 0 && move.x < size_ && move.y < size_;
}

/**
 * Returns the stone at a given move on the board.
 * @param move The move to check.
 * @return The stone at the given move.
 * @throws std::out_of_range if the move is outside the bounds of the board.
 */
Stone Board::at(Move move) const
{
    if (!contains(move))
    {
        throw std::out_of_range("Coordinate is outside the board.");
    }
    return cells_[static_cast<std::size_t>(move.y * size_ + move.x)];
}

/**
 * Checks if a given move is legal (i.e., within bounds and on an empty cell).
 * @param move The move to check.
 * @return True if the move is legal, false otherwise.
 */
bool Board::legal(Move move) const
{
    return contains(move) && at(move) == Stone::Empty;
}

/**
 * Plays a stone at a given move on the board.
 * @param move The move to play.
 * @param stone The stone to play.
 * @throws std::invalid_argument if the move is illegal or the stone is empty.
 */
void Board::play(Move move, Stone stone)
{
    if (stone == Stone::Empty || !legal(move))
    {
        throw std::invalid_argument("Illegal move.");
    }
    cells_[static_cast<std::size_t>(move.y * size_ + move.x)] = stone;
    ++moves_;
}

/**
 * Checks if a given move results in a win for the player who played it.
 * @param move The move to check.
 * @return True if the move results in a win, false otherwise.
 */
bool Board::wins(Move move) const
{
    const Stone stone = at(move);
    if (stone == Stone::Empty)
    {
        return false;
    }
    constexpr std::array<Move, 4> directions{{{1, 0}, {0, 1}, {1, 1}, {1, -1}}};
    for (const auto direction : directions)
    {
        int length = 1;
        for (const int sign : {-1, 1})
        {
            Move next{move.x + sign * direction.x, move.y + sign * direction.y};
            while (contains(next) && at(next) == stone)
            {
                ++length;
                next.x += sign * direction.x;
                next.y += sign * direction.y;
            }
        }
        if (rule_ == Rule::Freestyle ? length >= 5 : length == 5)
        {
            return true;
        }
    }
    return false;
}

/**
 * Checks if the board is full.
 * @return True if the board is full, false otherwise.
 */
bool Board::full() const
{
    return moves_ == size_ * size_;
}

/**
 * Returns the opposite stone of a given stone.
 * @param stone The stone to get the opposite of.
 * @return The opposite stone.
 * @throws std::invalid_argument if the stone is empty.
 */
Stone opposite(Stone stone)
{
    if (stone == Stone::Empty)
    {
        throw std::invalid_argument("Empty has no opponent.");
    }
    return stone == Stone::Black ? Stone::White : Stone::Black;
}

/**
 * Converts a move to a string representation in the format "A1", "B2", etc.
 * @param move The move to convert.
 * @return The string representation of the move.
 */
std::string coordinate(Move move)
{
    return std::string(1, static_cast<char>('A' + move.x)) + std::to_string(move.y + 1);
}

/**
 * Parses a string representation of a move in the format "A1", "B2", etc. into a Move object.
 * @param text The string representation of the move.
 * @param size The size of the board (used for validation).
 * @return An optional Move object if the parsing is successful, std::nullopt otherwise.
 */
std::optional<Move> parseCoordinate(const std::string& text, int size)
{
    std::istringstream input(text);
    char column = 0;
    int row = 0;
    if (!(input >> column >> row))
    {
        return std::nullopt;
    }
    input >> std::ws;
    const int x = std::toupper(static_cast<unsigned char>(column)) - 'A';
    if (!input.eof() || x < 0 || x >= size || row < 1 || row > size)
    {
        return std::nullopt;
    }
    return Move{x, row - 1};
}

/**
 * Parses a string representation of a move in the format "x,y" into a Move object.
 * @param text The string representation of the move.
 * @param size The size of the board (used for validation).
 * @return The Move object if the parsing is successful.
 */
Move parseProtocolMove(const std::string& text, int size)
{
    std::istringstream input(text);
    Move move{};
    char comma = 0;
    if (!(input >> move.x >> comma >> move.y) || comma != ',')
    {
        throw std::runtime_error("Invalid engine response: " + text);
    }
    input >> std::ws;
    if (!input.eof())
    {
        throw std::runtime_error("Invalid engine response: " + text);
    }
    if (move.x < 0 || move.y < 0 || move.x >= size || move.y >= size)
    {
        throw std::out_of_range("Engine coordinate outside the board: " + text);
    }
    return move;
}
} // namespace cli
