# moku
moku is a simple engine for the game of gomoku, supporting hottable pvp on one  terminal, an engine mode and playing against a minimax agent.

## Run
to build, clone the repository and `cd`into it.

Then run:
```
cmake -B build
cmake --build build
```

Once the build is finished, run:

```
.\moku.exe [options]
```

## Usage
```
moku [options]
```
Possible options:
### Engine mode
Run moku in engine mode:
```
moku --engine, -e
```

### PvP mode
Run moku in PvP mode, to play a hottable game of gomoku:
```
moku --pvp
```

### Minimax mode
Run moku in minimax mode, to play against an minimax agent:
```
moku --minimax 
```

### Version
Display the installed version of moku:
```
moku --version, -v
```

### Help
Display a help screen:
```
moku --help, -h
```