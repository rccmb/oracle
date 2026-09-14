# Architecture

This document describes the internal architecture of Oracle - how the modules are organized, how data flows between them, and the threading model that drives real-time analysis.

## High-Level Overview

Oracle is a single-process Windows desktop application composed of three concurrent threads and a shared global state layer:

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
│  ┌──────┴───────────┐    ┌─────────┴─────────┐                       │
│  │ ChessboardDetect │    │ BoardStateManager │                       │
│  │     Thread       │    │      Thread       │                       │
│  │ (Vision + FEN)   │    │ (Change Detect)   │                       │
│  └───────┬──────────┘    └───────────────────┘                       │
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
| Render loop | Calls `RenderFrame()` every ~10ms to draw the overlay and ImGui |
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
| Threading | `g_boardChanged`, `g_boardChangedMutex` | Cross-thread change notification |

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
- `WS_EX_LAYERED` - Required for per-pixel transparency.

The overlay draws:
- A green rectangle around the detected board.
- Cyan-filled rectangles for sample points (during configuration).
- Yellow rectangles for crop regions (during configuration).

---

#### `Direct3D.h / Direct3D.cpp` - DirectX 11 Backend

Manages the D3D11 rendering pipeline:

| Function | Purpose |
|---|---|
| `CreateDeviceD3D()` | Creates the D3D11 device, device context, and DXGI swap chain |
| `CreateRenderTarget()` | Creates a render target view from the swap chain back buffer |
| `CleanupRenderTarget()` | Releases the render target |
| `CleanupDirect3D()` | Full cleanup of all D3D11 resources |

The swap chain uses `DXGI_FORMAT_R8G8B8A8_UNORM` with a transparent clear color `(0, 0, 0, 0)` to achieve the see-through overlay effect.

---

#### `Menu.h / Menu.cpp` - ImGui User Interface

The largest module by line count. Renders the complete ImGui interface using two windows:

**Configuration Window (`oracle.pro`):**
1. Screenshot capture button
2. Board click status and reset
3. Rescan board button
4. Sample point sliders (patch size, X/Y offset)
5. Crop region sliders (crop size, X/Y offset)
6. Analysis tolerance slider
7. Reference value display (grayscale + color swatches)

**Analysis Window (`Real-Time Analysis`):**
1. Stockfish status indicator (alive/dead)
2. Engine sliders (ELO, depth, number of moves)
3. Side selection (playing as white/black)
4. Live 8×8 chessboard preview with Unicode pieces
5. Color-coded move suggestions table (advantage/balanced/disadvantage)

---

### Computer Vision Pipeline

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

#### `BoardStateManager.h / BoardStateManager.cpp` - Change Detection

A secondary background thread that:
1. Copies `g_detectedLetters` into `g_letterDrawQueue`.
2. Compares with `g_prevLetterDrawQueue`.
3. If a change is detected, updates `g_boardGridRows` and sets `g_boardChanged = true` under a mutex.
4. Includes debug logging of the 8×8 grids to stdout.

> **Note:** There is partial overlap between this module and the change detection in `ChessboardDetection.cpp`. Consolidation is planned (see [Roadmap](ROADMAP.md)).

---

### Engine Integration

#### `StockfishHandler.h / StockfishHandler.cpp` - Stockfish UCI

Manages the Stockfish process lifecycle and UCI communication:

| Function | Purpose |
|---|---|
| `LaunchStockfish()` | Spawns Stockfish as a child process with redirected stdin/stdout via Win32 pipes |
| `StockfishIsAlive()` | Sends `isready` and waits up to 500ms for `readyok` |
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
| **LetterRender** | `CreateThread()` (BoardStateManager) | Once analysis starts, runs indefinitely | Change detection, debug logging |

**Synchronization:**
- `g_boardChangedMutex` - Guards `g_boardChanged` between the detection and render threads.
- `g_sfMutex` - Serializes all Stockfish I/O.
- Most other globals are written by one thread and read by another, relying on the `Sleep(10)` throttle for loose synchronization. This is a known area for improvement.

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
