# game-of-life

This project simulates Conway's Game of Life, with the option of fine-tuning the dimensions of the board (rows x columns), frames-per-second (fps), pre-defined patterns, and initial board with an optional seed for randomization.

## Basic Rules

1. A live cell with fewer than two neighbors dies (underpopulation).
2. A live cell with more than three neighbors dies (overpopulation).
3. A dead cell with exactly three neighbors becomes a live cell (reproduction).

### Key takeaways

Any live cell with two or three neighbors lives to the next generation.
Any dead cell with exactly three neighbors is born into the next generation.
For any generation n, the only state that matters when determining the next generation (n+1), is the current generation (n).

## Basic setup

1. Make sure you have `cmake` installed.

`brew install cmake`

## How to run

1. Run `cmake -B build`
2. Run `cmake --build build`
3. Run `./build/game_of_life`

Step 1 configures the build and only needs to be run once.

## Options

Every flag takes the form `--name=value`. Run `./build/game_of_life --help` to print this list in the terminal.

| Flag | Default | Description |
|---|---|---|
| `--rows=N` | 24 | Board height in cells |
| `--cols=N` | 40 | Board width in cells |
| `--fps=N` | 15 | Frames per second |
| `--seed=N` | random | Seed for the random start; the same seed gives the same board |
| `--patterns=a,b` | — | Comma-separated preset patterns to place instead of a random start |
| `--help` | — | Print usage and exit |

### Patterns

1. Still lifes: `block`, `beehive`, `loaf`, `boat`, `tub`
2. Oscillators: `blinker`, `toad`, `beacon`, `pulsar`, `pentadecathlon`
3. Spaceships: `glider`, `lwss`, `mwss`, `hwss`

### Examples

1. A larger, faster board — `./build/game_of_life --rows=30 --cols=60 --fps=20`
2. A reproducible random start — `./build/game_of_life --seed=42`
3. Two preset patterns — `./build/game_of_life --patterns=glider,pulsar`
4. A board of oscillators — `./build/game_of_life --patterns=blinker,toad,beacon --rows=20`

## Behavior

1. The board is a torus. Edges wrap on both axes, so a glider leaving the right edge re-enters on the left.
2. `--patterns` replaces the random start, and `--seed` is ignored when it is given.
3. Random starts fill at a fixed 50% density; this is not configurable.
4. The simulation exits when a generation is identical to the one before it. That catches still lifes; oscillators and spaceships never compare equal, so they run until interrupted.
5. Each cell renders two characters wide, so `--cols=40` needs an 80-column terminal. This is the most likely cause of a board that looks wrapped or garbled.
6. Patterns are laid out left-to-right with a 2-cell margin, wrapping onto new rows. Any pattern too large for the board is skipped with a message on stderr.
7. Ctrl+C restores the cursor before exiting.

## Sources

1. [Conway's Game of Life](https://en.wikipedia.org/wiki/Conway%27s_Game_of_Life)
