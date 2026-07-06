import sys, os
sys.path.insert(0, os.path.dirname(__file__))

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F
from collections import deque
import random

from main import GomokuEnv


class QNet(nn.Module):
    def __init__(self, board_size):
        super().__init__()
        self.conv1 = nn.Conv2d(2, 64, 3, padding=1)
        self.conv2 = nn.Conv2d(64, 64, 3, padding=1)
        self.head = nn.Conv2d(64, 1, 1)
        self.board_size = board_size

    def forward(self, x):
        x = F.relu(self.conv1(x))
        x = F.relu(self.conv2(x))
        x = self.head(x)
        return x.flatten(1)


def masked_argmax(q, mask):
    q = q.clone()
    q[mask == 0] = -1e9
    return int(q.argmax())


def train(episodes=2000, board_size=15, gamma=0.99, lr=1e-3, batch=64):
    device = "cuda" if torch.cuda.is_available() else "cpu"
    env = GomokuEnv(board_size=board_size, opponent="random")

    net = QNet(board_size).to(device)
    tgt = QNet(board_size).to(device)
    tgt.load_state_dict(net.state_dict())
    opt = torch.optim.Adam(net.parameters(), lr=lr)

    buf = deque(maxlen=50000)
    eps = 1.0

    for ep in range(episodes):
        obs, info = env.reset()
        done = False
        total = 0.0

        while not done:
            mask = torch.tensor(info["action_mask"], device=device)
            if random.random() < eps:
                legal = np.flatnonzero(info["action_mask"])
                action = int(np.random.choice(legal))
            else:
                with torch.no_grad():
                    q = net(torch.tensor(obs, dtype=torch.float32, device=device).unsqueeze(0))[0]
                action = masked_argmax(q, mask)

            nobs, reward, term, trunc, ninfo = env.step(action)
            done = term or trunc
            buf.append((obs.copy(), action, reward, nobs.copy(), done, ninfo["action_mask"].copy()))
            obs, info = nobs, ninfo
            total += reward

            if len(buf) >= batch:
                b = random.sample(buf, batch)
                s, a, r, ns, d, nm = zip(*b)
                s = torch.tensor(np.array(s), dtype=torch.float32, device=device)
                ns = torch.tensor(np.array(ns), dtype=torch.float32, device=device)
                a = torch.tensor(a, device=device)
                r = torch.tensor(r, dtype=torch.float32, device=device)
                d = torch.tensor(d, dtype=torch.float32, device=device)
                nm = torch.tensor(np.array(nm), device=device)

                q = net(s).gather(1, a.unsqueeze(1)).squeeze(1)
                with torch.no_grad():
                    nq = tgt(ns)
                    nq[nm == 0] = -1e9
                    nq_max = nq.max(1).values
                    target = r + gamma * nq_max * (1 - d)
                loss = F.smooth_l1_loss(q, target)
                opt.zero_grad()
                loss.backward()
                opt.step()

        eps = max(0.05, eps * 0.995)
        if ep % 50 == 0:
            tgt.load_state_dict(net.state_dict())
            print(f"ep {ep} eps {eps:.3f} reward {total:.1f}")

    torch.save(net.state_dict(), "gomoku_dqn.pt")


if __name__ == "__main__":
    train()