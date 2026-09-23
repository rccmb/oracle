# Contributing to Oracle

Thank you for your interest in contributing to Oracle! This guide will help you get set up and outline the conventions we follow.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Environment](#development-environment)
- [Project Layout](#project-layout)
- [Code Style & Conventions](#code-style--conventions)
- [Branching Strategy](#branching-strategy)
- [Making Changes](#making-changes)
- [Commit Messages](#commit-messages)
- [Pull Requests](#pull-requests)
- [Reporting Bugs](#reporting-bugs)
- [Requesting Features](#requesting-features)
- [Areas Where Help Is Needed](#areas-where-help-is-needed)

---

## Code of Conduct

Be respectful, constructive, and inclusive. We follow the [Contributor Covenant](https://www.contributor-covenant.org/version/2/1/code_of_conduct/) code of conduct. Harassment, discrimination, or personal attacks will not be tolerated.

---

## Getting Started

1. **Fork** the repository on GitHub.
2. **Clone** your fork locally:
   ```bash
   git clone https://github.com/<your-username>/oracle.git
   cd oracle
   ```
3. **Create a branch** for your work:
   ```bash
   git checkout -b feature/your-feature-name
   ```
4. **Set up the development environment** (see below).
5. **Make your changes**, test them, and submit a pull request.

---

## Development Environment

### Required Software

| Tool | Version | Download |
|---|---|---|
| Visual Studio | 2022 (Community or higher) | [visualstudio.microsoft.com](https://visualstudio.microsoft.com/) |
| Desktop development with C++ workload | - | Installed via VS Installer |
| Windows 10/11 SDK | 10.0+ | Included with VS C++ workload |
| OpenCV | 4.12.0 | [opencv.org/releases](https://opencv.org/releases/) |
| Git | Latest | [git-scm.com](https://git-scm.com/) |

### Setup Steps

Follow the steps in the README.

### Bundled Dependencies (No Setup Needed)

- **Dear ImGui** - Source files are vendored in `third_party/imgui/`.
- **Stockfish** - The engine executable and sources are in `third_party/stockfish/`.

---

## Project Layout

```
oracle/
├── Oracle.sln                     # Visual Studio solution
├── Oracle.vcxproj                 # Visual Studio project
├── src/
│   ├── main.cpp                   # Entry point, overlay window, render loop
│   ├── Globals.cpp/h              # Shared state, declared once and used everywhere
│   ├── Structs.h                  # CLICK, SAMPLE, StockfishMove
│   ├── chess/                     # The rules. Standard library only.
│   │   ├── ChessRules.cpp/h       #   Positions, legal moves, FEN
│   │   └── GameTracker.cpp/h      #   Follows the game across frames
│   ├── vision/                    # Reading a board off the screen. OpenCV.
│   │   ├── BoardDetection.cpp/h   #   Automatic board, colour and orientation search
│   │   ├── ChessboardDetection.cpp/h  # Per-square matching, the analysis loop
│   │   └── InitialConfiguration.cpp/h # Manual calibration, reference capture
│   ├── engine/
│   │   └── StockfishHandler.cpp/h # UCI protocol, engine lifecycle
│   ├── platform/                  # Windows: capture, windowing, files.
│   │   ├── Utils.cpp/h            #   Screen capture, DPI, palette masking
│   │   ├── Overlay.cpp/h          #   Transparent Win32 overlay window
│   │   ├── Direct3D.cpp/h         #   D3D11 device, swap chain, render target
│   │   └── FileHandler.cpp/h      #   Reference piece image I/O
│   └── ui/
│       └── Menu.cpp/h             # ImGui: setup, settings, preview, evaluation
├── third_party/
│   ├── imgui/                     # Vendored Dear ImGui (do not modify)
│   └── stockfish/                 # Bundled Stockfish (do not modify)
├── tools/
│   ├── EngineCheck/               # Drives a UCI engine of your choice
│   ├── PerftCheck/                # Move generator, against the perft suite
│   ├── TrackerCheck/              # Game tracking across frames
│   ├── FenCheck/                  # Castling rights and position validation
│   └── BoardDetectionCheck/       # Offline check for the board detector
└── OpenCV/                        # OpenCV installation (not tracked in git)
```

The folders under `src/` run one way: `chess` depends on nothing, `vision`
depends on OpenCV, `platform` on Windows, and only `ui` and `main.cpp` depend on
everything. `chess` having no dependencies is not an aspiration, it is checked
every time `tools/PerftCheck` builds without OpenCV on the command line.

For a detailed description of each module, see [ARCHITECTURE.md](ARCHITECTURE.md).

---

## Code Style & Conventions

### General

- **Language**: C++17.
- **Line endings**: CRLF on Windows (the `.gitattributes` should handle this).
- **Indentation**: Tabs for indentation, spaces for alignment.
- **Max line length**: No hard limit, but keep lines readable (~120 characters as a soft guideline).
- **Comments**: Write meaningful comments explaining *why*, not just *what*. All public functions in headers should have `@brief`, `@param`, and `@return` Doxygen-style doc comments.

### Naming Conventions

| Element | Convention | Example |
|---|---|---|
| Functions | `PascalCase` | `DetectBoardDimensions()` |
| Local variables | `camelCase` | `cellWidth`, `bestChamfer` |
| Global variables | `g_camelCase` | `g_boardRect`, `g_sfElo` |
| Constants / macros | `UPPER_SNAKE_CASE` | `IMGUI_MENU_VISIBLE` |
| Structs | `UPPER_SNAKE_CASE` | `CLICK`, `SAMPLE` |
| Header guards | `#pragma once` | - |

### File Organization

- Each module should have a matching `.h` and `.cpp` file pair.
- Headers should contain only declarations, includes, and inline definitions.
- Avoid adding new global variables if possible - prefer passing state through function parameters or encapsulating in a struct/class.

### Dependencies

- **Do not modify** files in `ImGui/` or `stockfish/`. If an update is needed, open an issue to discuss.
- **Do not commit** OpenCV binaries, build outputs, or `.vs/` directories.
- When adding a new dependency, document it in the README and discuss in the PR.

---

## Branching Strategy

| Branch | Purpose |
|---|---|
| `main` | Stable, latest release |
| `develop` | Integration branch for upcoming features |
| `feature/*` | New features (`feature/en-passant-detection`) |
| `fix/*` | Bug fixes (`fix/fen-validation-crash`) |
| `refactor/*` | Code restructuring (`refactor/globals-to-classes`) |
| `docs/*` | Documentation changes (`docs/improve-readme`) |

Always branch from `main` (or `develop` once it exists) and submit PRs back to it.

---

## Making Changes

### Before You Start

1. **Check existing issues** - Someone might already be working on the same thing.
2. **Open an issue first** for non-trivial changes - this lets maintainers weigh in on the approach before you invest time.
3. **Read `ARCHITECTURE.md`** - Understanding the module structure and data flow will save you time.

### While Working

1. **Keep changes focused** - One PR per feature or fix. Don't mix unrelated changes.
2. **Test manually** - Since there is no automated test suite yet, verify your changes against at least one chess website (e.g., Lichess, Chess.com).
3. **Build in both Debug and Release** to catch configuration-specific issues.
4. **Check for regressions** - Make sure the calibration flow, board detection, and Stockfish integration still work end-to-end.

### Areas Marked with TODO

The codebase has several `// TODO:` comments indicating known gaps. These are great entry points for contributions:

- `// TODO: Documentation.` - Add missing Doxygen-style comments.
- `// TODO: En passant detection.` - Implement en passant move tracking.
- `// TODO: Implement board evaluation.` - Add a position evaluation bar.
- `// TODO: Make similarity editable with ImGui.` - Expose the chamfer threshold in the UI.
- `// TODO: Implement debug sample grouping` - Support multiple sample groups per cell.

---

## Commit Messages

Follow the [Conventional Commits](https://www.conventionalcommits.org/) format:

```
<type>: <short description>

[optional body]

[optional footer]
```

### Types

| Type | When to use |
|---|---|
| `feat` | Adding a new feature |
| `fix` | Fixing a bug |
| `refactor` | Code restructuring without behavior change |
| `docs` | Documentation only |
| `style` | Formatting, whitespace, etc. (no logic change) |
| `perf` | Performance improvement |
| `test` | Adding or updating tests |
| `chore` | Build config, CI, tooling |

### Examples

```
feat: implement en passant detection in ChessboardDetection

Tracks when two pawns disappear and one pawn appears on an adjacent
file. Updates the FEN en passant target square accordingly.

Closes #42
```

```
fix: prevent crash when Stockfish returns empty eval

Guard against empty response string before parsing the "Final evaluation"
line in GetBestMoves().
```

---

## Pull Requests

### Checklist

Before submitting a PR, make sure you have:

- [ ] Built the project successfully in both Debug and Release (x64).
- [ ] Manually tested the end-to-end flow (calibration → detection → Stockfish moves).
- [ ] Added or updated comments/documentation for any new or modified code.
- [ ] Followed the naming conventions and code style described above.
- [ ] Written a clear PR description explaining *what* changed and *why*.
- [ ] Linked any related issues (e.g., `Closes #12`).

### PR Description Template

```markdown
## What

Brief description of the change.

## Why

Motivation, context, or link to the issue.

## How

Technical approach or key decisions.

## Testing

How you tested this change.
```

### Review Process

1. At least one maintainer must approve the PR.
2. All review comments must be resolved.
3. The PR must build cleanly.
4. Squash-merge is preferred for clean git history.

---

## Reporting Bugs

Open a GitHub issue with the following template:

```markdown
### Description

What happened?

### Steps to Reproduce

1. Step one
2. Step two
3. ...

### Expected Behavior

What should have happened?

### Actual Behavior

What happened instead?

### Environment

- OS version: (e.g., Windows 11 23H2)
- Visual Studio version: (e.g., 2022 17.x)
- OpenCV version: (e.g., 4.12.0)
- Chess platform: (e.g., Lichess, Chess.com)
- Screen resolution: (e.g., 1920x1080)
- Board theme: (if relevant)

### Screenshots / Logs

Attach any relevant screenshots or console output.
```

---

## Requesting Features

Open a GitHub issue with the `feature-request` label and describe:

1. **What** you'd like to see.
2. **Why** it would be useful.
3. **How** you envision it working (optional).
4. **Alternatives** you've considered (optional).

Check the [Roadmap](ROADMAP.md) first - your idea might already be planned.

---

## Areas Where Help Is Needed

Here are some high-impact areas where contributions are especially welcome:

| Area | Difficulty | Description |
|---|---|---|
| 📝 Documentation | Easy | Fill in `// TODO: Documentation.` comments across all headers |
| 🧪 Test framework | Medium | Set up a unit/integration test framework (e.g., Google Test) |
| 🏗️ Refactor globals | Medium | Encapsulate global state into classes (see ARCHITECTURE.md) |
| ♟️ En passant | Medium | Detect en passant captures in `ChessboardDetection.cpp` |
| ♟️ Castling detection | Medium | Correctly detect and handle castling in FEN generation |
| 🎨 Board theme support | Medium | Support more chess website themes without re-calibration |
| 🔧 CMake build system | Medium | Add a CMake build alongside the `.sln` for easier cross-IDE support |
| 📊 Evaluation bar | Medium | Add a visual eval bar next to the board preview |
| 🧵 Thread safety | Hard | Replace loose `Sleep()`-based synchronization with proper primitives |
| 🖥️ Multi-monitor | Hard | Support board detection across multiple monitors |

---

Thank you for contributing! Every bug report, documentation fix, and feature implementation helps make Oracle better. 🙏
