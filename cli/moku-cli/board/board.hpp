#pragma once

#include <optional>
#include <string>
#include <vector>

namespace cli
{
enum class Stone
{
    Empty,
    Black,
    White
};

enum class Rule
{
    Freestyle,
    ExactFive
};

struct Move
{
    int x;
    int y;
};

class Board
{
public:
    explicit Board(int size, Rule rule = Rule::Freestyle);
    int size() const;
    int moves() const;
    bool contains(Move move) const;
    Stone at(Move move) const;
    bool legal(Move move) const;
    void play(Move move, Stone stone);
    bool wins(Move move) const;
    bool full() const;

private:
    int size_;
    Rule rule_;
    int moves_ = 0;
    std::vector<Stone> cells_;
};

Stone opposite(Stone stone);
std::string coordinate(Move move);
std::optional<Move> parseCoordinate(const std::string& text, int size);
Move parseProtocolMove(const std::string& text, int size);
} // namespace cli
