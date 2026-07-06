# moku
moku is a simple engine for the game of gomoku, supporting hottable pvp in one terminal, an engine mode and playing against a minimax agent.

## Run
To build, clone the repository and `cd` into it.

Make sure you have installed `cmake` on your system.

Then run:
```
mkdir build
cmake -B build
cmake --build build
```

Once the build is finished, run:

```
cd build
.\moku.exe [options]
```

### Build release
To build a release run:
```
mkdir build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Only build certain parts
#### Only build the excecutable
To only build the excecutable, run:
```
mkdir build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target moku
```

#### Only build the Python library
To only build the Python library, used for the RL training, run:
```
mkdir build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target pymoku
```

#### Only build the tests
To only build the tests, run:
```
mkdir build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target run_tests
```

## Usage
```
moku [options]
```
Possible options:
### Game modes
The engine currently offers the following three game modes:

#### Engine mode
In the engine mode, moku will act as a pure engine, following the ["Gomoku AI Protocol"](https://plastovicka.github.io/protocl2en.htm).
Run moku in engine mode:
```
moku --engine
moku -e
```

#### PvP mode
Run moku in PvP mode, to play a hottable game of gomoku:
```
moku --pvp
moku -p
```

#### Minimax mode
Run moku in minimax mode, to play against an minimax agent:
```
moku --minimax
moku -m
```

### Settings
In addition to the gamemode, you can set several of the games parameters with optional arguments, for example by running `moku --pvp --size 13 --time 10000`.

#### Board size
Set the size of the gameboard:
```
moku --size 19
moku -s 19
```

The size has to be an integer between 5 and 19.

#### Search depth
Set the search depth for the Minimax agent:
```
moku --depth 10
moku -d 10
```

#### Time limit
Set the time limit, in ms, for the move search for the minimax agent:
```
moku --time 5000
moku -t 5000
```

### Helper functions
Some small functions that do small things.
#### Version
Display the installed version of moku:
```
moku --version, -v
```

#### Help
Display a help screen:
```
moku --help, -h
```