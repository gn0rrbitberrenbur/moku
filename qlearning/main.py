import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "build"))
os.add_dll_directory(r"C:\Util\Strawberry\c\bin")
os.add_dll_directory(os.path.dirname(sys.executable))

import numpy as np
import gymnasium as gym
from gymnasium import spaces
import pymoku


class GomokuEnv(gym.Env):
    metadata = {"render_modes": ["human"], "render_fps": 4}

    def __init__(self, board_size=15, opponent="random", render_mode=None):
        super().__init__()
        pymoku.set_board_size(board_size)
        self.board_size = board_size
        self.n_squares = board_size * board_size
        self.opponent = opponent
        self.render_mode = render_mode

        self.action_space = spaces.Discrete(self.n_squares)
        self.observation_space = spaces.Box(
            low=0, high=1, shape=(2, board_size, board_size), dtype=np.int8
        )

        self.board = None
        self.agent_is_black = True
        self._minimax = None

    def _get_obs(self):
        obs = self.board.to_numpy()
        if not self.agent_is_black:
            obs = obs[::-1].copy()
        return obs

    def _action_mask(self):
        mask = np.zeros(self.n_squares, dtype=np.int8)
        for m in self.board.legal_moves():
            mask[m] = 1
        return mask

    def _get_info(self):
        return {"action_mask": self._action_mask()}

    def _opponent_move(self):
        legal = self.board.legal_moves()
        if not legal:
            return None
        if self.opponent == "random":
            return int(self.np_random.choice(legal))
        if self.opponent == "minimax":
            if self._minimax is None:
                self._minimax = pymoku.MinimaxAgent(2)
            return self._minimax.get_best_move(self.board, not self.agent_is_black)
        raise ValueError(f"unknown opponent: {self.opponent}")

    def reset(self, seed=None, options=None):
        super().reset(seed=seed)
        self.board = pymoku.Board()
        self.agent_is_black = True if options is None else options.get("agent_is_black", True)

        if not self.agent_is_black:
            opp = self._opponent_move()
            if opp is not None:
                self.board.make_move(opp, not self.agent_is_black)

        return self._get_obs(), self._get_info()

    def step(self, action):
        action = int(action)

        if self.board.test_pos(action):
            return self._get_obs(), -1.0, True, False, self._get_info()

        self.board.make_move(action, self.agent_is_black)

        if self.board.check_win():
            return self._get_obs(), 1.0, True, False, self._get_info()

        if not self.board.legal_moves():
            return self._get_obs(), 0.0, True, False, self._get_info()

        opp = self._opponent_move()
        if opp is not None:
            self.board.make_move(opp, not self.agent_is_black)
            if self.board.check_win():
                return self._get_obs(), -1.0, True, False, self._get_info()

        if not self.board.legal_moves():
            return self._get_obs(), 0.0, True, False, self._get_info()

        return self._get_obs(), 0.0, False, False, self._get_info()

    def render(self):
        if self.render_mode == "human":
            self.board.output_board()

    def close(self):
        pass


if __name__ == "__main__":
    env = GomokuEnv(opponent="random", render_mode="human")
    obs, info = env.reset(seed=42)
    terminated = False
    while not terminated:
        legal = np.flatnonzero(info["action_mask"])
        action = int(env.np_random.choice(legal))
        obs, reward, terminated, truncated, info = env.step(action)
    env.render()
    print("reward:", reward)