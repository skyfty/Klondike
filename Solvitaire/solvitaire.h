#pragma once

struct solve_state {
    enum class type { TIMEOUT, SOLVED, UNSOLVABLE };
    type sol_type = type::UNSOLVABLE;
    uint64_t dominance_moves = 0;
    std::chrono::milliseconds time;
};
typedef void (*gen_game_deal_callback)(const char* buf, size_t, solve_state*);
void gen_game_deal(int seed, const char* rule_file, gen_game_deal_callback cb, solve_state*);

typedef bool (*solve_callback)(solve_state state, solve_state*);
void solve_buffer(const char* rule_file, const char* buff, size_t number, uint64_t timeout, solve_callback cb, solve_state*);
void sigint_handler(int i);
