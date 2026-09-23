# EngineCheck

Checks that Oracle can drive a given UCI engine, without launching the overlay.

Oracle speaks plain UCI over a pipe and knows nothing about Stockfish in
particular, so any engine that answers the protocol will work. This runs the
same code the overlay does, against whichever engine you point it at, so you
find out before a game rather than during one.

## Build and run

```
build.bat

EngineCheck.exe                        the bundled engine
EngineCheck.exe path\to\engine.exe     your own
```

It prints where the engine was resolved to, the `id name` the engine reports,
and the moves it returns from the opening position. Exit code is 0 when the
engine answered.

## What Oracle asks of an engine

| Sent | Required |
|---|---|
| `uci`, and `uciok` in reply | Yes. A binary that does not answer within five seconds is rejected. |
| `isready` / `readyok` | Yes |
| `position fen <fen>` | Yes |
| `go depth <n>` | Yes |
| `bestmove` in reply, including `bestmove (none)` | Yes. `(none)` is how checkmate and stalemate are recognised. |
| `info … score cp\|mate … pv <move>` | Yes, that is where the suggestions come from |
| `setoption name MultiPV` | No, but without it only one suggestion is ever shown |
| `setoption name UCI_LimitStrength` / `UCI_Elo` | No. Only sent when strength limiting is switched on, and the protocol requires an engine to ignore options it does not know. |

## Choosing an engine in Oracle

Either pass it on the command line:

```
Oracle.exe --engine "C:\engines\myengine.exe"
```

or type the path into the Engine box in the menu and press Load. Oracle reports
the engine's own name once it has answered, so you can see which one is running.

A relative path is resolved against the executable rather than the working
directory, so dropping an engine beside `Oracle.exe` and passing just its name
works.
