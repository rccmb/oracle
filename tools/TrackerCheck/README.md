# TrackerCheck

Drives `GameTracker` through the situations a real game puts it in.

The tracker receives only what the vision pipeline can give it: sixty-four
characters, with no side to move, no castling rights and no history. Its job is
to work out which legal move, if any, explains that board. These cases check that
it does:

- a normal move, and the same frame again not being read as a second move
- two plies inside one frame, which happens on a premove or a busy machine
- castling, where two pieces move at once and only the rules say it is one move
- en passant, where a pawn disappears from a square nothing moved to
- a takeback, where the position matches an earlier one and the moves after it
  are dropped
- a new game
- a board that no legal move explains, which is held rather than believed, and
  then adopted once it persists

Expectations are written as FEN strings by hand, so a change that makes the
tracker agree with itself but disagree with chess still fails here.

## Build and run

```
build.bat
TrackerCheck.exe
```

No OpenCV required. Exit code is 0 when every case passes.

See also [PerftCheck](../PerftCheck/README.md), which proves the move generator
underneath this. A tracker cannot be more correct than the move list it chooses
from.
