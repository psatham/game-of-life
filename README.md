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

1. Run `cmake --build build`
2. Run `./build/game_of_life`


## Sources

1. [Conway's Game of Life](https://en.wikipedia.org/wiki/Conway%27s_Game_of_Life)