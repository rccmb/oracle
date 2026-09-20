# PerftCheck

Proves the move generator in `src/chess/ChessRules.cpp` against the published perft
suite.

Perft counts leaf nodes at a fixed depth from a given position. The reference
numbers are widely published and exact, and each position in the suite is
sensitive to a different corner of the rules: en passant, under-promotion,
castling through an attacked square, pinned pieces, castling rights lost when a
rook is captured on its own corner.

This matters more here than in an engine. Oracle uses legal moves to decide what
happened on screen, so a generator that is subtly wrong does not play a slightly
weaker game, it misreads the board and desynchronises from the real one.

## Build and run

```
build.bat
PerftCheck.exe
```

No OpenCV required: `ChessRules` depends on nothing but the standard library.
Exit code is 0 when every count matches. Each position is also round-tripped
through FEN, since a position that cannot rewrite its own FEN would send the
engine something other than what it tracked.

Depth 5 on the starting position is 4,865,609 nodes and takes a few seconds in a
release build.
