# UiPreview

Renders Oracle's menu and board overlay offscreen and writes them out as PNGs.

The overlay window is excluded from screen capture, which is what stops Oracle
reading its own arrows back as part of the board, so a screenshot or a screen
recording never shows it. This runs the same drawing code into a texture over a
board instead, and composites the result exactly the way the overlay window is
composited on screen.

That compositing is the point. The overlay is colour keyed on pure black: black
is transparent and every other pixel is opaque. Nothing drawn over the board is
ever alpha blended with the page, only with black, which is how a translucent
yellow arrives on screen as brown. A preview that blended normally would hide
exactly the mistakes worth catching.

## Build and run

```
build.bat
UiPreview.exe                    every scene, into out\
UiPreview.exe out\after verdict  only scenes whose name contains "verdict"
```

Everything in `src\` except `main.cpp` is compiled in, and the bundled engine is
started, so the menu reports what it would really report.

## Scenes

| Scene | What it shows |
|---|---|
| `setup` | The menu before any board has been detected |
| `game` | A game in progress, playing Black, with the menu open |
| `overlay` | The same game with the menu closed: the overlay alone |
| `verdict` | A lichess board, just after the opponent blundered |
| `lost` | The board no longer matching the tracked game |
| `mate` | Checkmate |
| `screenshot` | The overlay over `screenshot.png` from the repository root, if there is one |

The boards behind the scenes are drawn, in chess.com and lichess colours, with
coordinates, a last move marked and player bars above and below, so the overlay
is judged against the things it actually has to sit beside.

Each scene writes the whole screen, `<scene>.png`, and full size crops of the
board, `<scene>.board.png`, and the menu, `<scene>.menu.png`.
