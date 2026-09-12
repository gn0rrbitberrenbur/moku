# moku
moku is an engine for the game of gomoku, supporting hottable pvp in one terminal, an engine mode and playing against the engine.

## Build
### Build everything
To build the excecutable and the tests run:
```
cd moku
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Only build the excecutable
To only build the excecutable run:
```
cd moku
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target moku
```

Then run:
```
.\build\moku.exe
```

### Only build the tests
To only build the tests, run:
```
cd moku
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target tests
```

Then run:
```
.\build\tests.exe
```

## Usage
To excecute moku run:
```
.\build\moku.exe [options]
```

### Game modes
The engine currently offers the following three game modes:

#### Engine mode
In the engine mode, moku will act as a pure engine, following the ["Gomoku AI Protocol"](https://plastovicka.github.io/protocl2en.htm).
Run moku in engine mode:
```
.\build\moku.exe --engine, -e
```

#### PvP mode
Run moku in PvP mode, to play a hottable game of gomoku:
```
.\build\moku.exe --pvp, -p
```

#### Minimax mode
Run moku in minimax mode, to play against an minimax agent:
```
.\build\moku.exe --minimax, -m
```

### Settings
In addition to the gamemode, you can set several of the games parameters with optional arguments, for example by running:
```
.\build\moku.exe --pvp --size 13 --time 10000`
```

#### Board size
Set the size of the gameboard:
```
.\build\moku.exe --size, -s [5-19]
```
> If not set, defaults to `15`

The size has to be an integer between 5 and 19.

#### Search depth
Set the search depth for the Minimax agent:
```
.\build\moku.exe --depth, -d [int]
```
> If not set, defaults to `12`

#### Time limit
Set the time limit **(in ms)** for the move search for the minimax agent:
```
.\build\moku.exe --time, -t [int]
```
> If not set, defaults to `20000`

#### Cores
Set the amount of cores the engine can use:
```
.\build\moku.exe --cores, -c [int]
```
> If not set, defaults to the maximum amount of available cores

#### Debug
Toggle additional debugging information, displayed during the move generation:
```
.\build\moku.exe --debug
```

### Benchmark
Run a little benchmark:
```
.\build\moku.exe --benchmark, -b
```

### Helper functions
Some small functions that do small things.
#### Version
Display the installed version of moku:
```
.\build\moku.exe --version, -v
```

#### Help
Display a help screen:
```
.\build\moku.exe --help, -h
```