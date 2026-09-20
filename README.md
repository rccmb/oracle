<p align="center">
  <h1 align="center">♟ Oracle</h1>
  <p align="center">
    A real-time chess assistant that uses computer vision to detect chessboard positions from your screen and suggests optimal moves powered by Stockfish.
  </p>
</p>

<p align="center">
  <a href="https://youtu.be/QZ0bH_iGHVM">
    <img src="https://img.youtube.com/vi/QZ0bH_iGHVM/maxresdefault.jpg" alt="Oracle Trailer" width="800">
  </a>
</p>

<p align="center">
  <a href="#features">Features</a> •
  <a href="#how-it-works">How It Works</a> •
  <a href="#prerequisites">Prerequisites</a> •
  <a href="#building-from-source">Build</a> •
  <a href="#usage">Usage</a> •
  <a href="ARCHITECTURE.md">Architecture</a> •
  <a href="CONTRIBUTING.md">Contributing</a> •
  <a href="ROADMAP.md">Roadmap</a>
</p>

---

## Features

- **One-Click Setup** - Finds the board on screen by itself, along with both square colours, both piece colours and the orientation. No clicking corners, no sliders.
- **Real-Time Screen Capture** - Watches the board for changes, no browser extension or plugin needed.
- **Multi-Monitor and High-DPI** - Per-monitor DPI aware, and the board can sit on any display.
- **Follows the Game** - Tracks the real position across moves rather than re-reading the board each frame, so castling, en passant, takebacks and premoves are all handled, and a frame caught mid-animation is rejected instead of believed.
- **Numbered Moves and Arrows** - Every suggestion is drawn on the board, ranked and colour-coded by evaluation.
- **Live Evaluation Bar** - Glides as the search deepens instead of jumping when it finishes.
- **Blunder Callouts** - Says what the last move cost, for either side, above the board.
- **Piece Recognition** - Identifies pieces using chamfer distance matching against reference images generated during calibration.
- **Stockfish Integration** - Communicates with a bundled Stockfish engine via the UCI protocol to compute the best moves in real time.
- **Transparent Overlay** - Renders a DirectX 11 overlay on top of your screen, drawing the detected board rectangle and configuration guides without blocking your view.
- **ImGui Control Panel** - A fully interactive menu for calibration, analysis tuning, Stockfish settings, and a live board preview with color-coded move suggestions.
- **Platform-Independent Board Support** - Works with any chess website or desktop application - if you can see the board on your screen, Oracle can detect it.

## How It Works

Oracle operates in two phases:

### 1. Setup

Press **Detect Board**. Oracle searches the screen for the one thing that is a
grid of equally sized, axis-aligned squares in two alternating colours, reads the
square and piece colours from it, works out which way round the board is, sizes
its sampling geometry from the detected squares, captures reference pieces from
the starting rows, and starts analysing.

The board must be at the **starting position**, since that is where the reference
pieces come from. Nothing else is required.

If a board cannot be read automatically, **Manual calibration** still offers the
original flow: screenshot, `Ctrl + LMB` on a8 then b8, and the sampling sliders.

### 2. Analysis

- A background thread captures the board, skipping frames in which nothing
  changed, and reads each square by template matching.
- What it read is an **observation, not an answer**. Oracle asks which of the
  moves legal in the position it already holds explains that board. Almost always
  exactly one does. When none does, the frame is rejected, so a piece caught
  mid-animation or a square behind a dialog costs nothing.
- Because the position is carried forward rather than rebuilt, castling rights,
  en passant and the move clocks are simply correct, and castling, promotion,
  takebacks, premoves and a new game are all recognised for what they are.
- The position goes to Stockfish, whichever side is to move. Suggestions are
  offered on your turn; analysing the opponent's is what lets Oracle say what
  their move cost them.
- Results are drawn on the board: an arrow and a rank number per suggestion,
  coloured by evaluation, with an evaluation bar and a callout when the last move
  was an inaccuracy, a mistake or a blunder.

## Prerequisites

| Dependency | Version | Notes |
|---|---|---|
| **Visual Studio** | 2022 (v143 toolset) | Community edition or higher |
| **Windows SDK** | 10.0+ | Included with Visual Studio |
| **OpenCV** | 4.12.0 | Pre-built binaries (`opencv_world4120`, put in `OpenCV` directory at the root of the repo) |
| **DirectX 11** | - | Ships with Windows SDK |
| **Dear ImGui** | - | Vendored in `Oracle/ImGui/` (no setup needed) |
| **Stockfish** | - | Bundled in `Oracle/stockfish/` (no setup needed) |
| **C++ Standard** | C++17 | Set in the project configuration |

## Building from Source

### 1. Clone the Repository

```bash
git clone https://github.com/rccmb/oracle.git
cd oracle
```

### 2. Install OpenCV

Download the pre-built OpenCV 4.12.0 binaries from [opencv.org/releases](https://opencv.org/releases/) and extract them (e.g., to `OpenCV\` directory at the root of the repo).

### 3. Build

Open `Oracle/Oracle.sln` in Visual Studio 2022, select **x64**, and build
(`Ctrl + Shift + B`). Both Debug and Release are configured, no project
properties need editing, and the matching OpenCV DLL is copied next to the
executable automatically.

If OpenCV lives somewhere other than `OpenCV\opencv\build` inside this
repository, point `ORACLE_OPENCV_ROOT` at its build directory, or pass it
directly to MSBuild:

```
msbuild Oracle.sln -p:Configuration=Release -p:Platform=x64 -p:OracleOpenCVRoot=D:\opencv\build
```

### 4. Run

Launch from Visual Studio (`F5`) or run the compiled executable directly. Make sure `stockfish/stockfish.exe` is accessible relative to the executable's working directory.

## Usage

### Hotkeys

| Hotkey | Action |
|---|---|
| `Ctrl + F1` | Toggle the overlay menu on/off |
| `Numpad + / Numpad -` | Alternative toggle for the overlay menu |
| `Ctrl + LMB` | Set board corner clicks (manual calibration only) |

### Step-by-Step

1. **Launch Oracle** - The overlay starts hidden.
2. **Open a board** at the starting position, on any site or desktop app.
3. **Open the menu** - Press `Ctrl + F1`.
4. **Detect** - Click "Detect Board". Analysis begins as soon as it succeeds.
5. **View results** - The "Real-Time Analysis" window shows the live board and Stockfish's best moves.
6. **Adjust engine** - Depth and number of moves sit in the same window. The engine
   runs at full strength unless "Limit engine strength" is ticked.

If detection fails, open **Manual calibration** for the original corner-click
flow, and **Advanced: sampling geometry** for the sliders.

## Project Structure

```
oracle/
├── Oracle/                        # Main source directory
│   ├── main.cpp                   # Entry point, overlay window, render loop
│   ├── BoardDetection.cpp/h       # Automatic board, colour and orientation search
│   ├── ChessRules.cpp/h           # Positions, legal moves, FEN
│   ├── GameTracker.cpp/h          # Follows the game across frames
│   ├── ChessboardDetection.cpp/h  # Board scanning, FEN generation, piece matching
│   ├── StockfishHandler.cpp/h     # UCI protocol, engine lifecycle
│   ├── InitialConfiguration.cpp/h # Calibration: clicks, samples, crop, references
│   ├── Menu.cpp/h                 # ImGui UI: settings, live preview, move display
│   ├── Overlay.cpp/h              # Transparent fullscreen Win32 overlay
│   ├── Direct3D.cpp/h             # D3D11 device, swap chain, render target
│   ├── Utils.cpp/h                # Screen capture, DPI setup, palette masking
│   ├── FileHandler.cpp/h          # Reference piece image I/O
│   ├── Globals.cpp/h              # Shared global state
│   ├── Structs.h                  # CLICK and SAMPLE data structures
│   ├── ImGui/                     # Vendored Dear ImGui sources
│   ├── stockfish/                 # Bundled Stockfish engine + sources
│   ├── Oracle.sln                 # Visual Studio solution
│   └── Oracle.vcxproj             # Visual Studio project
├── tools/
│   ├── BoardDetectionCheck/       # Offline check for the board detector
│   ├── FenCheck/                  # Castling rights and position validation
│   ├── PerftCheck/                # Move generator, against the perft suite
│   └── TrackerCheck/              # Game tracking across frames
└── OpenCV/                        # OpenCV installation (not tracked in git)
```

> For a deep dive into the architecture and module responsibilities, see [ARCHITECTURE.md](ARCHITECTURE.md).

## Contributing

Contributions are welcome! Please read the [Contributing Guide](CONTRIBUTING.md) for details on:

- Setting up your development environment
- Code style and conventions
- Submitting pull requests
- Reporting bugs and requesting features

## Roadmap

See [ROADMAP.md](ROADMAP.md) for the planned features and improvements.

## License

Creative Commons Attribution-NonCommercial 4.0 International

See [LICENSE.md](LICENSE.md) for details.

## Acknowledgments

- [OpenCV](https://opencv.org/) - Computer vision and image processing.
- [Stockfish](https://stockfishchess.org/) - The strongest open-source chess engine.
- [Dear ImGui](https://github.com/ocornut/imgui) - Immediate-mode GUI for tools and debug UIs.
- [Microsoft DirectX 11](https://docs.microsoft.com/en-us/windows/win32/direct3d11/atoc-dx-graphics-direct3d-11) - Hardware-accelerated rendering.
