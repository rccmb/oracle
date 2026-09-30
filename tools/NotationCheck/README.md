# NotationCheck

Checks the standard algebraic notation (SAN) Oracle shows moves in: `Nf3`
rather than `g1f3`.

Every expected move is written by hand, the way a player would write it, so the
check fails on notation that is merely consistent with itself. It covers pawn and
piece captures, en passant, castling both ways for both sides, promotion with and
without a capture, check and mate, and each way a piece is told apart from
another: by file (`Rad1`), by rank (`R1a2`), by both (`Qa1b2`), and not at all
when the other piece is pinned (`Nd4`).

## Build and run

```
build.bat
NotationCheck.exe
```

Standard library only, like `src/chess` itself. Exit code is 0 when every case
passes.
