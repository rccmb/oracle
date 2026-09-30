# Architecture

This document describes the internal architecture of Oracle - how the modules are organized, how data flows between them, and the threading model that drives real-time analysis.

## High-Level Overview

Oracle is a single-process Windows desktop application composed of two concurrent threads and a shared global state layer:

```
┌──────────────────────────────────────────────────────────────────────┐
│                          Main Thread                                 │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────────────────┐   │
│  │ Overlay  │  │ Direct3D │  │  ImGui   │  │  InitialConfig      │   │
│  │ (Win32)  │──│ (D3D11)  │──│  Menu    │──│  (Calibration)      │   │
│  └──────────┘  └──────────┘  └──────────┘  └─────────────────────┘   │
│         ▲                          │                                 │
│         │                          ▼                                 │
│         │                 ┌──────────────────┐                       │
│         │                 │  Global State    │                       │
│         │                 │  (Globals.h/cpp) │                       │
│         │                 └────────┬─────────┘                       │
│         │                          │                                 │
├─────────┼──────────────────────────┼─────────────────────────────────┤
│         │                 Background Threads                         │
│         │                          │                                 │
│  ┌──────┴───────────┐                                                │
│  │ ChessboardDetect │                                                │
│  │     Thread       │                                                │
│  │ (Vision + FEN)   │                                                │
│  └───────┬──────────┘                                                │
│          │                                                           │
│  ┌───────┴──────────┐                                                │
│  │ StockfishHandler │                                                │
│  │ (UCI via pipes)  │                                                │
│  └──────────────────┘                                                │
└──────────────────────────────────────────────────────────────────────┘
```

## Module Breakdown

### Core Modules

#### `main.cpp` - Entry Point & Render Loop

| Responsibility | Details |
|---|---|
| Application lifecycle | Creates the overlay window, D3D11 device, and launches Stockfish |
| Render loop | Calls `RenderFrame()` every ~10ms, which draws `DrawBoardOverlay()` and then the menu |
| Hotkey handling | Listens for `Ctrl+F1` (or `Numpad+/-`) to toggle the menu |
| Calibration input | Forwards `Ctrl+LMB` clicks to `SetBoardClicks()` during configuration |
| Thread management | Spawns the `ChessboardDetectionThread` once analysis begins |

The main loop is a classic Win32 message pump combined with an immediate-mode render cycle:

```
WinMain()
  ├── CreateOverlayWindow()
  ├── CreateDeviceD3D()
  ├── CreateRenderTarget()
  ├── InitializeImGui()
  ├── LaunchStockfish()
  └── while(true)
       ├── Handle hotkeys (toggle menu)
       ├── Handle Ctrl+LMB clicks (calibration)
       ├── PeekMessage() / DispatchMessage()
       ├── Spawn ChessboardDetectionThread (once)
       ├── RenderFrame()
       └── Sleep(10)
```

---

#### `Globals.h / Globals.cpp` - Shared State

All inter-module communication flows through global variables declared in `Globals.h`. This is the central state store of the application.

**Key state categories:**

| Category | Variables | Purpose |
|---|---|---|
| Board geometry | `g_boardRect`, `g_clicks` | Detected board bounding box and calibration clicks |
| Configuration flags | `g_isConfiguringSamplePoints`, `g_isConfiguringCropRegion`, `g_hasAnalysisStarted` | Track which phase the app is in |
| Color references | `g_refBlackPiece`, `g_refWhitePiece`, `g_refBoardColor1/2` | Grayscale intensity values for piece/board identification |
| Color references (BGR) | `g_refBlackPieceColor`, `g_refWhitePieceColor`, `g_refBoardColor1/2Color` | Full color values for palette masking |
| Detection state | `g_detectedLetters`, `g_boardGridRows`, `g_lastValidFEN` | Current board position as detected by vision |
| Debug/tuning | `g_debugPatchSize`, `g_debugOffsetX/Y`, `g_cropPatchSize`, `g_cropOffsetX/Y`, `g_analysisTolerance` | User-adjustable vision parameters |
| Stockfish | `g_sfElo`, `g_sfPlayWhite`, `g_sfMoveDepth`, `g_sfNumberMoves` | Engine configuration |
| Threading | `g_boardChanged`, `g_boardChangedMutex`, `g_analysisStateMutex` | Cross-thread change notification and the lock over shared analysis output |

> **Design Note:** The heavy reliance on globals is a known simplification. See the [Roadmap](ROADMAP.md) for plans to encapsulate state into proper classes.

---

#### `Structs.h` - Data Structures

Defines two lightweight POD structs used throughout the project:

- **`CLICK`** - Stores an `(x, y)` screen coordinate and the grayscale value at that pixel. Used during calibration to define board corners and color references.
- **`SAMPLE`** - Stores a rectangle `(x, y, width, height)`. Used to define per-cell sampling regions and crop areas.

---

### Rendering & UI

#### `Overlay.h / Overlay.cpp` - Transparent Overlay Window

Creates a fullscreen, transparent, always-on-top, click-through Win32 window. This window hosts the Direct3D rendering surface.

**Window styles:**
- `WS_EX_TOPMOST` - Always on top of other windows.
- `WS_EX_TRANSPARENT` - Clicks pass through to underlying windows (toggled when the menu is visible).
- `WS_EX_LAYERED` with a colour key of pure black - Black pixels are transparent and every other pixel is opaque.

That last point decides how anything drawn over the board must be coloured.
There is no per-pixel alpha between the overlay and the page beneath it: a colour
drawn with alpha is blended with the black the frame was cleared to, and the
result is shown opaque. A translucent yellow therefore arrives on screen as an
opaque brown. Overlay colours are chosen as the opaque colours they will be, and
pure black is never drawn on purpose, since it would punch a hole.

The window is also excluded from screen capture, so Oracle never reads its own
drawing back as part of the board, and a screenshot never shows it either.
`tools/UiPreview` renders what it draws offscreen, composited the same way.

---

#### `Direct3D.h / Direct3D.cpp` - DirectX 11 Backend

Manages the D3D11 rendering pipeline:

| Function | Purpose |
|---|---|
| `CreateDeviceD3D()` | Creates the D3D11 device, device context, and DXGI swap chain |
| `CreateRenderTarget()` | Creates a render target view from the swap chain back buffer |
| `CleanupRenderTarget()` | Releases the render target |
| `CleanupDirect3D()` | Full cleanup of all D3D11 resources |

The swap chain uses `DXGI_FORMAT_R8G8B8A8_UNORM` and is cleared to black every
frame, which the overlay window's colour key turns into see-through.

---

#### `BoardOverlay.h / BoardOverlay.cpp` - Drawing Over the Board

Everything drawn on top of the board rather than in the menu. `DrawBoardOverlay()`
takes the draw list to use and reads the shared analysis state, copying it under
`g_analysisStateMutex` first.

| Element | What it shows |
|---|---|
| Tracking corners | Which board is being followed: blue while the game makes sense, amber once it does not, grey while calibrating |
| Evaluation bar | Beside the board, the side being played at the bottom, easing towards each depth's score; the score rides the boundary, with a spinner while the position on the board is still being searched |
| Callout | Above the board: the last move's verdict as `??`, `?` or `?!`, who played it, what it cost and what was best, or how the game ended |
| Badges and arrows | Each suggestion's rank at both ends of the move and a black arrow between them, drawn only for the position on the board and only on our turn |
| Calibration guides | Sample points and crop regions while calibrating by hand |

Checkmate and stalemate are read off the tracked position, not from the engine,
which has no score to give once the game is over.

Nothing here relies on alpha to look translucent, because the window is colour
keyed (see above), and motion is movement, never a fade: arrows extend from the
piece, badges settle into place, the callout slides in. Suggestions animate once
per new set, keyed on the position and the moves.

It is a function of its own, rather than part of the render loop, so it can be
drawn somewhere other than the live overlay: `tools/UiPreview` draws it offscreen.

---

#### `Menu.h / Menu.cpp` - The Panel

One window with a header and three tabs. The header carries the mark, the
engine's status and name, and buttons to hide the panel and to quit.

| Tab | Contents |
|---|---|
| **Play**, before a board | The engine first, then the board, as a checklist, and **Detect board** |
| **Play**, during a game | The evaluation in figures and words with its bar, whose move it is, the last move and any verdict, the board as Oracle reads it beside the ranked moves in SAN, and which side you play |
| **Engine** | Path, file picker and Load; depth and moves shown; strength limit and Elo; move arrows |
| **Calibrate** | The detected board and Rescan; match threshold and tolerance; learned highlight colours; reference colours; manual calibration and sampling geometry, folded away |

Detecting a board sets the side played to the side at the bottom. The menu reads
the analysis state once per frame, under the lock, and never waits on the engine:
see `g_sfMutex` below. `ConsumeMenuHideRequest()` lets the render loop, which owns
showing and hiding, act on the panel's own hide button.

---

#### `Theme.h / Theme.cpp` and `Widgets.h / Widgets.cpp` - The Look

`Theme` holds the palette, the type sizes and the fonts, and applies the ImGui
style: Segoe UI and Segoe UI Semibold with Segoe Fluent Icons merged in, Cascadia
Mono for data, Segoe UI Symbol for pieces, all scaled by the display's DPI.
`EvaluationColor()` and `VerdictColor()` live here so the board and the menu can
never disagree about what a colour means.

`Widgets` builds the menu's controls from ImGui primitives: buttons in three
kinds, icon buttons, toggles, sliders, a segmented control, tabs with a sliding
underline, cards, banners, and the rank disc the overlay draws on the board.

[DESIGN.md](DESIGN.md) records the system these implement.

---

### Computer Vision Pipeline

#### `ChessRules.h / ChessRules.cpp` - The Rules

Positions, legal move generation, FEN in and out. Depends on nothing but the
standard library and knows nothing about screens or OpenCV. Squares are indexed
in FEN reading order, 0 for a8 through 63 for h1, which matches the order a
detected grid is walked in, so no second coordinate convention is needed.

Proven by `tools/PerftCheck` against the published perft suite: six positions,
thirty-two counts, covering en passant, under-promotion, castling through an
attacked square, pinned pieces and rights lost to a captured rook.

`ToSan()` writes a move the way players read it, `Nf3` rather than `g1f3`,
disambiguating by file, then rank, then both, only as far as the position needs,
and never against a pinned piece that could not legally make the move. Every
move `GameTracker` applies records its SAN, worked out in the position it was
played in. `tools/NotationCheck` holds the hand written cases.

---

#### `GameTracker.h / GameTracker.cpp` - Following the Game

Holds the position and reconciles each observed board against it.

| Outcome | Meaning |
|---|---|
| `Unchanged` | The board matches the position already held |
| `Advanced` | One or two legal moves account for the change |
| `Restarted` | The starting position: a new game |
| `TookBack` | An earlier position; the moves after it are dropped |
| `Adopted` | Resynchronised onto a position that could not be reached |
| `Unreadable` | Nothing explains the frame; the held position stands |

The search runs one ply, then two, then the starting position, then the history.
Two plies matter because a premove, or simply a busy machine, produces a frame in
which both sides have moved. A board that stays unexplained is held rather than
believed, and adopted only once it has persisted; side to move is then found by
elimination, since the side **not** to move cannot be standing in check.

This is what makes the rest correct rather than lucky. A position carried forward
knows its own castling rights, en passant square and clocks, none of which are
visible on a board, and a misread square fails to match any legal move instead of
quietly becoming a plausible wrong position.

`tools/TrackerCheck` covers each outcome against hand written FENs.

---

#### `BoardDetection.h / BoardDetection.cpp` - Automatic Setup

Locates a chessboard anywhere in a desktop capture with no user input, and reads
everything calibration used to ask for: both square colours, both piece colours,
the orientation, and the sampling geometry.

| Function | Purpose |
|---|---|
| `DetectChessboard()` | Finds the board, its cell size and its two square colours |
| `EstimatePieceColors()` | Reads the two piece colours and the orientation from a located board |
| `ApplyDerivedSampleGeometry()` | Sizes sample patches, crop regions and tolerance from the board |
| `LuminanceOf()` | Perceived brightness, used wherever a colour needs a grayscale twin |

**Search:**

1. **Square candidates.** Canny, then contours whose bounding box is square and
   whose every point lies on that box's border, touching all four sides.
   Rectangularity cannot be tested by area here: a board square appears in an
   edge map as a one pixel ring, which OpenCV traces by running around it and
   back, so the shoelace sum cancels and `contourArea` returns zero for exactly
   the squares being sought.
2. **Cell size and phase.** For each commonly occurring candidate size, vote the
   candidates' positions modulo that size to find the grid phase both axes agree
   on, and snap candidates onto the resulting lattice.
3. **Placement.** Score every 8x8 window of the lattice with a summed-area table
   and keep the competitive ones. Ties are routine and are not broken here.
4. **Verification.** Score each surviving placement against the two-colour
   checker pattern and keep the best.

Two properties make this work mid-game. Only about half a board's squares yield a
clean contour, so the grid is fixed from a handful and pieces never need to be
absent. And verification reads all sixty-four squares from their **corners**,
which piece glyphs leave showing, rather than only the empty ones: a checkerboard
still alternates when shifted a whole rank, so scoring the visible squares alone
ranks a window slid off the board just as highly. What separates the true
placement is that all sixty-four of its squares are board.

`tools/BoardDetectionCheck` runs this over a screenshot offline, with a stage
trace, so a change that breaks a site or theme is visible without a live game.

---

#### `InitialConfiguration.h / InitialConfiguration.cpp` - Calibration

Handles the one-time setup before analysis can begin:

| Function | Purpose |
|---|---|
| `SetBoardClicks()` | Records two `Ctrl+LMB` clicks to define board corners |
| `DetectBoardDimensions()` | Uses the clicks as an ROI and validates the chessboard pattern |
| `ValidateChessboard()` | Scans for grid junction points by looking for alternating color patterns |
| `DetectPieceColorCoding()` | Samples the top-left and bottom-left cells to determine which side is black/white and detect board orientation |
| `UpdateDebugSamples()` | Generates 64 sample rectangles (one per cell) based on current slider values |
| `UpdateCropRects()` | Generates 64 crop rectangles for piece template extraction |
| `GenerateReferencePieceCrops()` | Extracts and saves Canny-edge reference images for all 12 piece types from the starting position |

**Board validation algorithm:**
1. Scan the user-defined ROI in a 2px stride.
2. At each point, sample a 2×2 neighborhood with a 5px offset.
3. Check if the four sampled intensities match one of two checkerboard patterns (using the clicked color references).
4. If ≥4 junction points are found, estimate cell size and extrapolate the full 8×8 board.

---

#### `ChessboardDetection.h / ChessboardDetection.cpp` - Real-Time Analysis

The core analysis engine, running in a dedicated background thread:

**Detection pipeline (per frame):**

```
Capture frame (HWND2MAT)
       │
       ▼
Convert to grayscale
       │
       ▼
For each of 64 cells:
  ├── Sample center brightness (SampleCellCenter)
  ├── Compare against reference values (± tolerance)
  ├── If occupied: extract crop, run Canny edge detection
  ├── For each reference template:
  │     ├── Resize candidate to template size
  │     ├── Compute chamfer distance (sum of distances to nearest edge)
  │     └── Track best match
  └── Accept if similarity ≥ 0.90 (exp(-chamfer / 3.0))
       │
       ▼
Detect state changes (compare with previous frame)
       │
       ▼
Infer side to move (who's piece disappeared/appeared)
       │
       ▼
Update g_boardGridRows → BoardToFEN()
```

**Key algorithm - Chamfer Distance Matching:**
1. Reference piece images are preprocessed once at startup: threshold → dilate → distance transform.
2. For each live cell, Canny edges are extracted and resized to the reference dimensions.
3. Edge points of the candidate are looked up in the reference's distance transform.
4. The mean distance gives the chamfer score; lower = better match.
5. Converted to similarity via `exp(-chamfer / 3.0)` and accepted at ≥ 0.90.

**FEN generation (`BoardToFEN`):**
- Reads `g_boardGridRows` respecting `g_orientation`.
- Validates that both kings are present.
- Appends side-to-move, castling rights (defaulting to `KQkq`), and move counters.
- Falls back to the starting FEN if validation fails.

---

### Engine Integration

#### `StockfishHandler.h / StockfishHandler.cpp` - Stockfish UCI

Manages the Stockfish process lifecycle and UCI communication:

| Function | Purpose |
|---|---|
| `LaunchStockfish()` | Spawns Stockfish as a child process with redirected stdin/stdout via Win32 pipes |
| `StockfishIsAlive()` | Sends `isready` and waits up to 500ms for `readyok`, restarting an engine that has died |
| `LaunchStockfishAsync()` | `LaunchStockfish()` on a background thread, queued behind any search in progress |
| `StockfishLooksAlive()` | Liveness without waiting: while a search holds the engine, the last known answer |
| `GetEngineStatus()` | A consistent copy of the engine's path, name and last error, safe from any thread |

The analysis loop searches again whenever anything that shapes the answer
changes, not only the position: the depth, the number of lines, the strength
limit, or the engine itself, which `g_sfEngineGeneration` counts launches of.
Moving a slider in the menu updates the suggestions on the board at once.
| `GetBestMoves()` | Sets UCI options (ELO, MultiPV), sends `position fen ...`, runs `go depth N`, and parses `info` lines |
| `ShutdownStockfish()` | Sends `quit` and cleans up process handles |

**UCI communication flow:**
```
Oracle                          Stockfish
  │                                │
  ├──── uci ──────────────────────►│
  │                                │
  ├──── setoption name ... ───────►│
  ├──── position fen ... ─────────►│
  ├──── eval ─────────────────────►│
  │◄─── Final evaluation ... ──────│
  ├──── go depth N ───────────────►│
  │◄─── info depth N ... pv ... ───│
  │◄─── bestmove ... ──────────────│
  │                                │
```

**Move categorization:**
- Parsed `info` lines extract the UCI move, centipawn score, and mate-in-N information.
- Moves are only returned when `g_sideToMove` matches the configured playing side.
- Previous results are cached and returned while waiting for the opponent's turn.

---

### Utilities

#### `Utils.h / Utils.cpp`

| Function | Purpose |
|---|---|
| `HWND2MAT()` | Captures a window's client area via `BitBlt` and returns an OpenCV `cv::Mat` in BGR format |
| `PieceToUnicode()` | Converts FEN piece characters to Unicode chess symbols for ImGui display |
| `ApplyPaletteMasking()` | Creates a BGR mask from reference colors (± tolerance), filters the image, and converts to grayscale |

#### `FileHandler.h / FileHandler.cpp`

| Function | Purpose |
|---|---|
| `LoadWithImdecode()` | Reads an image file via `std::ifstream` and decodes it with `cv::imdecode` (avoids path encoding issues) |
| `SaveReferencePiece()` | Crops, applies Canny edge detection, and saves a reference piece image to the temp directory |
| `LoadReferencePieces()` | Loads all saved reference images from the temp directory into a `std::map<string, Mat>` |

---

## Threading Model

| Thread | Created By | Lifetime | Purpose |
|---|---|---|---|
| **Main** | OS | App lifetime | Win32 message pump, D3D11 rendering, ImGui |
| **ChessboardDetection** | `CreateThread()` in main loop | Once analysis starts, runs indefinitely | Screen capture, piece detection, FEN generation, Stockfish queries |

**Synchronization:**
- `g_analysisStateMutex` - Guards the state the detection thread produces and the render thread consumes: `g_boardGridRows`, `g_detectedLetters`, `g_sfBestMoves` and `g_lastValidFEN`. These are vectors and strings the producer reallocates, so it is held on both sides, only long enough to copy in or out.
- `g_boardChangedMutex` - Guards `g_boardChanged`.
- `g_sfMutex` - Serializes all Stockfish I/O. A search holds it for as long as it runs, which at any real depth is seconds, so nothing on the render thread may wait for it: the menu asks `StockfishLooksAlive()`, loads engines with `LaunchStockfishAsync()`, and reads names and errors through `GetEngineStatus()`.
- The remaining globals are configuration written by the render thread and read by the detection thread. A torn read of an `int` slider costs one frame of analysis, so these are left unguarded deliberately; anything with an allocation behind it belongs under `g_analysisStateMutex`.

---

## Data Flow Summary

```
Screen Capture ──► Grayscale Conversion ──► Cell Sampling ──► Occupancy Check
                                                                    │
                                                               ┌────┴────┐
                                                               │ Empty   │ Occupied
                                                               │ (skip)  │
                                                               └─────────┘
                                                                    │
                                                          Crop + Canny Edges
                                                                    │
                                                          Chamfer Matching
                                                                    │
                                                          Piece Identified
                                                                    │
                                                          g_detectedLetters[]
                                                                    │
                                                          Change Detection
                                                                    │
                                                          BoardToFEN()
                                                                    │
                                                          GetBestMoves()
                                                                    │
                                                          ImGui Display
```
