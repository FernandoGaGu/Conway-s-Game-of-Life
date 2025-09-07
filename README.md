# Conway's Game of Life


Conway's Game of Life is a cellular automaton devised by mathematician John Horton Conway in 1970. It is a zero-player game, meaning its evolution is determined entirely by its initial state, without further input from players.

The "game" consists of a grid of cells, each of which can be in one of two states: alive or dead. At each step in time (a generation), the state of the grid evolves based on a simple set of rules applied to every cell simultaneously:

- **Underpopulation**: A live cell with fewer than two live neighbors dies.

- **Survival**: A live cell with two or three live neighbors survives.

- **Overpopulation**: A live cell with more than three live neighbors dies.

- **Reproduction**: A dead cell with exactly three live neighbors becomes a live cell.

These simple rules can produce surprisingly complex and beautiful patterns, including oscillators, spaceships, and self-replicating structures.


> This repository contains a C implementation of Conway’s Game of Life with real-time visualization using SDL2. The program reads an initial configuration file, simulates the cellular automaton according to the standard rules (underpopulation, survival, overpopulation, and reproduction), and renders each generation to a window so you can watch patterns evolve — from simple still lifes and oscillators to gliders and more complex emergent behavior.


## Requirements


- SDL2 must be installed on your system. (In tests performed, SDL2 was installed with Homebrew on macOS)

### Build
```bash
gcc -o ./src/gol ./src/game_of_life.c -I/opt/homebrew/include -L/opt/homebrew/lib -lSDL2
```

### Usage
Run the compiled executable and pass a configuration file containing the initial cell layout:

```
./src/gol <configuration_file>
```

To try different starting conditions, simply pass different configuration files to the executable. Some example configurations and presets are provided in `./config`


> **Note**: Testing has been performed only on macOS running on an Apple M4 processor. Cross-platform compatibility, especially on other CPU architectures (x86_64, ARMv7, etc.) or different SDL2 builds, has not been validated. Build or runtime issues on other systems are possible.


## Examples
