# Roadmap

Whatever comes to mind 🤷‍♂️

## Completed

Features and improvements that have already been implemented:

| Status | Item | Version |
|---|---|---|
| ✅ | Real-time screen capture via Win32 `BitBlt` | v0.1 |
| ✅ | Manual board calibration with click-based corners | v0.1 |
| ✅ | Chessboard validation via junction pattern scanning | v0.1 |
| ✅ | Piece detection via chamfer distance matching | v0.1 |
| ✅ | Stockfish integration with UCI protocol | v0.1 |
| ✅ | Transparent DirectX 11 overlay | v0.1 |
| ✅ | ImGui configuration and analysis panels | v0.1 |
| ✅ | Live board preview with Unicode chess pieces | v0.1 |
| ✅ | Color-coded move suggestions (advantage/balanced/disadvantage) | v0.1 |
| ✅ | Configurable engine ELO, depth, and MultiPV | v0.1 |
| ✅ | Board orientation detection (white/black at bottom) | v0.1 |
| ✅ | Side-to-move inference from piece changes | v0.1 |
| ✅ | Reference piece generation from starting position | v0.1 |
| ✅ | Palette masking for improved detection on themed boards | v0.1 |
| ✅ | Adjustable sample point and crop region parameters | v0.1 |
| ✅ | Automatic board detection, no clicks or sliders | v0.2 |
| ✅ | Square, piece and orientation detection from the board itself | v0.2 |
| ✅ | Sampling geometry and tolerance derived from the detected cell size | v0.2 |
| ✅ | Per-monitor DPI awareness and multi-monitor capture | v0.2 |
| ✅ | Offline check for the board detector (`tools/BoardDetectionCheck`) | v0.2 |
| ✅ | Board-only capture, with unchanged frames skipped | v0.2 |
| ✅ | One lock over the state shared by the detection and render threads | v0.2 |
| ✅ | Debug and Release both build from a clean clone | v0.2 |
| ✅ | Chess rules engine, proven against the perft suite | v0.3 |
| ✅ | Game tracking: frames matched against legal moves | v0.3 |
| ✅ | Castling, en passant, promotion, takebacks and premoves | v0.3 |
| ✅ | Numbered move overlays and arrows | v0.3 |
| ✅ | Evaluation bar that glides as the search deepens | v0.3 |
| ✅ | Blunder, mistake and inaccuracy callouts for both sides | v0.3 |

---

## Next

Ideas worth taking on, roughly in order of what they unlock:

| Item | Why |
|---|---|
| Reference pieces per square colour | Templates are captured once, on whichever square colour a piece started on, so the same piece on the opposite shade matches worse. Twenty-four templates instead of twelve. |
| Re-detect while running | The board is found once. Moving or resizing the window leaves the grid stale until the user presses Detect again. |
| Threat view | Ask the engine what the opponent plays if you pass, and mark that square. Now cheap: the opponent's turn is already analysed. |
| Principal variation | Only the first move of each line is kept. Showing three or four ply is a parsing change, not a feature. |
| Export the game | The tracker holds the full move list. PGN out is a formatting job. |
| Replace template matching | A small classifier trained on the piece sets the major sites ship would drop the starting-position requirement and most remaining theme sensitivity. |
| One reader for the engine pipe | Liveness checks and searches both read the same pipe, so one can consume the other's output. A single reader thread dispatching lines removes the whole class of problem. |

---

## How to Contribute to the Roadmap

- **Want to work on something?** Open an issue referencing the roadmap item and describe your approach.
- **Have a new idea?** Open a feature request issue — if it aligns with the project vision, it'll be added here.
- **Disagree with a priority?** Open a discussion — roadmap ordering is open to community input.

See [CONTRIBUTING.md](CONTRIBUTING.md) for the full contribution workflow.
