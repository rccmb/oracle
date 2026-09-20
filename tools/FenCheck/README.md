# FenCheck

Feeds synthetic boards through the real `BoardToFEN()` and checks the FEN that
comes out, in particular its castling field.

## Why this exists

Oracle used to write `KQkq` into every FEN it produced, regardless of the board.
Stockfish **segfaults** on a position that claims a castling right when the side
has no rook to castle with: its setup code scans the back rank for the rook and
walks off the board when there is none.

That is fatal in exactly the positions a game ends in. Both of these kill the
engine outright:

```
7k/8/8/8/8/8/6Q1/6K1 w KQkq - 0 1     king and queen against king
7k/8/8/8/8/8/8/R5K1 w KQkq - 0 1      king and rook against king
```

Which is why the engine appeared to "die at checkmate": not the mate, the hidden
board or the new game, but the reduced material that mates happen in. Rights are
now read off the board, so both of those come out as `-` and the engine survives.

## Build and run

```
build.bat
FenCheck.exe
```

Exit code is 0 when every case passes, 1 otherwise. Each case prints the FEN it
produced, so the output can be piped straight into the engine as a second check:

```
FenCheck.exe | findstr "FEN:"
```

## Adding cases

Boards are written as eight rows of eight characters, top row first, as they
appear **on screen**, using FEN letters with `.` for an empty square. Pass the
orientation (0 when white is at the bottom) and the side to move. Expect either a
castling string, or `nullptr` for a board that should be rejected outright.

Rejected boards matter as much as accepted ones. A frame captured while a dialog
covers the board, or one with a square misread, must not reach the engine at all.
