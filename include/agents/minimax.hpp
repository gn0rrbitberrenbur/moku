#pragma once
#include "../board.hpp"
#include "evaluation.hpp"
#include <vector>
#include <limits>
#include <algorithm>

class MinimaxAgent 
{   
    int max_depth;
    size_t nodes_searched = 0;
    int current_depth = 0;
    Evaluator evaluator;
    
public:
    MinimaxAgent(int depth = 4) : max_depth(depth) {}
    int get_best_move(Board &board, bool is_black);
    
private:
    float minimax(Board &board, int depth, float alpha, float beta, bool is_maximizing);
};