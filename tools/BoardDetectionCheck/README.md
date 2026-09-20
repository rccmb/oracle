# BoardDetectionCheck

Runs `DetectChessboard` over a screenshot and prints what it found, without
launching the overlay.

Detection is the one part of Oracle that can be verified offline. Give it a PNG
of a desktop and it either locates the board or it does not, so a change that
breaks a site or a theme shows up here rather than in the middle of a game.

## Build

```
build.bat
```

Set `ORACLE_OPENCV_ROOT` to override the OpenCV location; it defaults to the
in-repo `OpenCV\opencv\build` that the main [README](../../README.md) describes.

## Use

Report what was found:

```
BoardDetectionCheck.exe board.png
```

Assert a known answer, for use in a script:

```
BoardDetectionCheck.exe board.png 273 152 107
```

The three numbers are the expected board origin and cell size, matched within a
tenth of a cell. Exit code is 0 on success, 1 on a mismatch or when no board was
found, 2 on a usage or file error.

Output includes the trace of each detection stage, which says where the search
gave up when it finds nothing: how many square candidates were seen, which cell
sizes were tried, and the confidence of each 8x8 placement that was verified.

## Building a corpus

Detection quality is worth measuring rather than guessing at. Keep a directory of
screenshots covering the sites, themes, piece sets, board sizes and light and
dark modes you care about, record the expected origin and cell size for each, and
run them in a loop:

```
for %f in (corpus\*.png) do BoardDetectionCheck.exe "%f"
```

Include at least one screenshot with no board on it. Detecting nothing is the
correct answer there, and a detector that starts finding boards in page
furniture is as broken as one that stops finding real ones.

Screenshots are not tracked in this repository; they are large, and the useful
ones are whichever sites you actually play on.
