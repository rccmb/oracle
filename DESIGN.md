---
name: Oracle
description: A real-time chess assistant drawn over the board you are playing on.
colors:
  panel: "#101216"
  surface: "#181B21"
  surface-hover: "#20242C"
  surface-active: "#282D37"
  line: "#262A33"
  line-strong: "#3A404D"
  text: "#E8EAEF"
  text-dim: "#9CA3AF"
  text-faint: "#808895"
  accent: "#2563EB"
  accent-hover: "#2D6CF2"
  accent-active: "#1E54CE"
  accent-text: "#7AAAFF"
  good: "#4ABE6E"
  warn: "#F2A630"
  bad: "#F05650"
  mate-for: "#824FD1"
  mate-against: "#BA2929"
  winning: "#1C803A"
  losing: "#C43535"
  level: "#576478"
  blunder: "#D63031"
  mistake: "#EC8B20"
  inaccuracy: "#F0C419"
  white-side: "#ECECE8"
  black-side: "#1C1E23"
  tracking: "#588EFF"
  chrome: "#14161B"
  chrome-border: "#363B46"
  arrow-ink: "#0C0C0E"
typography:
  display:
    fontFamily: "Segoe UI Semibold, Segoe UI Bold"
    fontSize: "30px"
    fontWeight: 600
    lineHeight: 1.1
  title:
    fontFamily: "Segoe UI Semibold"
    fontSize: "15px"
    fontWeight: 600
  body:
    fontFamily: "Segoe UI"
    fontSize: "14px"
    fontWeight: 400
    lineHeight: 1.33
  body-strong:
    fontFamily: "Segoe UI Semibold"
    fontSize: "14px"
    fontWeight: 600
  small:
    fontFamily: "Segoe UI"
    fontSize: "12.5px"
    fontWeight: 400
  label:
    fontFamily: "Segoe UI Semibold"
    fontSize: "12.5px"
    fontWeight: 600
  mono:
    fontFamily: "Cascadia Mono, Consolas"
    fontSize: "12.5px"
    fontWeight: 400
rounded:
  control: "6px"
  card: "8px"
  window: "12px"
  pill: "9999px"
spacing:
  xs: "4px"
  sm: "8px"
  md: "12px"
  lg: "16px"
components:
  button-primary:
    backgroundColor: "{colors.accent}"
    textColor: "#FFFFFF"
    typography: "{typography.body-strong}"
    rounded: "{rounded.control}"
    padding: "6px 10px"
  button-primary-hover:
    backgroundColor: "{colors.accent-hover}"
  button-primary-active:
    backgroundColor: "{colors.accent-active}"
  button-secondary:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.text}"
    typography: "{typography.body-strong}"
    rounded: "{rounded.control}"
    padding: "6px 10px"
  button-ghost:
    backgroundColor: "transparent"
    textColor: "{colors.text-dim}"
    typography: "{typography.body-strong}"
    rounded: "{rounded.control}"
  card:
    backgroundColor: "{colors.surface}"
    rounded: "{rounded.card}"
    padding: "10px 12px"
  pill-current:
    textColor: "{colors.accent-text}"
    typography: "{typography.label}"
    rounded: "{rounded.pill}"
    height: "22px"
  overlay-chrome:
    backgroundColor: "{colors.chrome}"
    textColor: "{colors.text}"
    typography: "{typography.body-strong}"
    rounded: "{rounded.pill}"
---

# Design System: Oracle

## Overview

**Creative North Star: "The Instrument Over the Board"**

Oracle lives in two places at once. The menu is a small dark instrument panel
floating beside a chess site; the overlay is drawn straight onto the site's own
board. Both answer to the same idea: a precise instrument that reads the
position and says only what a player needs, then gets out of the way. The panel
sits a step darker than the page so it never competes with the game, and
colour beyond a single blue is kept for meaning. When something on screen is
red, it is saying something.

The overlay is the product's signature and its most constrained surface. It is
drawn through a colour keyed window: pure black is transparent and every other
pixel is opaque. Nothing drawn over the board can be translucent, so every
colour there is chosen as the opaque colour it will actually be, and motion is
always movement, never a fade.

**Key Characteristics:**
- One dark panel, three tabs, sized to its content, fixed at 376 px wide.
- One accent, blue, for actions and the current state only.
- Evaluation colours shared exactly between the board and the menu.
- Chess notation everywhere a move is named: `Nf3`, `4... g6??`, never `g1f3`.
- Small, sharp marks on the board: 6 to 14 px discs, 2 px corners, black arrows.

## Colors

A near black graphite ramp with one electric blue, and a small set of chess
meanings layered on top.

### Primary
- **Signal Blue** (#2563EB): filled primary buttons, the switched on toggle, the
  slider's filled track. White text on it clears 5:1.
- **Signal Blue, as a mark** (#7AAAFF): the same hue lifted for use as text and
  thin marks on dark: the active tab's underline, "Your move", links, hover edges
  on primary buttons.
- **Tracking Blue** (#588EFF): the corner marks around a board being followed,
  and the searching spinner in the overlay.

### Neutral
- **Graphite Panel** (#101216): the window.
- **Raised Graphite** (#181B21): cards, fields, tracks. Hover #20242C, pressed #282D37.
- **Hairline** (#262A33) and **Strong Hairline** (#3A404D): borders and dividers.
- **Paper** (#E8EAEF), **Pewter** (#9CA3AF), **Slate Text** (#808895): primary,
  secondary and faint text. Each clears 4.5:1 on both the panel and a card.

### Meaning
- **Evaluation**, on every rank badge and every row of the move list: Winning
  #1C803A, Losing #C43535, Level #576478, Mate For #824FD1, Mate Against #BA2929.
  These are the opaque colours the original translucent badges landed on after
  the colour key, kept, except Winning, taken one step deeper so the white
  figures on it clear 5:1.
- **Verdicts**, on the callout's glyph and the menu's banner: Blunder #D63031
  (`??`), Mistake #EC8B20 (`?`), Inaccuracy #F0C419 (`?!`).
- **States**: Good #4ABE6E (engine ready), Warn #F2A630 (lost the game, engine
  starting), Bad #F05650 (engine missing). The quit button turns the solid red of
  a window's close button, #C42B1C, on hover.
- **The eval bar**: White #ECECE8 and Black #1C1E23, with a #08090B rim.

### Named Rules
**The Opaque Overlay Rule.** Anything drawn over the board is an opaque colour.
Alpha on the overlay only ever blends with black, which is how a translucent
yellow arrived on screen as brown. The arrows keep their rank based alpha
because black blended with black stays black.

**The Never Pure Black Rule.** Pure black punches a hole in the overlay. The
darkest ink anywhere is #0C0C0E.

**The One Blue Rule.** Blue is for what to do next and where things stand now.
It never decorates.

**The No Mud Rule.** A warm colour is never mixed towards the dark: not as a
tint, not as a fade, not dimmed for a disabled state. Amber, orange and yellow
pulled towards this panel all turn brown. They appear at full strength, on an
edge, an icon or a chip, or not at all.

## Typography

**Body Font:** Segoe UI, with Segoe UI Semibold for weight.
**Data Font:** Cascadia Mono, falling back to Consolas.
**Chess Glyphs:** Segoe UI Symbol. **Icons:** Segoe Fluent Icons, falling back to
Segoe MDL2 Assets, merged into both text fonts so an icon can sit in any label.

**Character:** the operating system's own voice, set crisply at every size. ImGui
1.92 rasterises each size on demand, so nothing is a scaled bitmap.

### Hierarchy
- **Display** (Semibold, 30 px): the evaluation, "+0.66", or "Checkmate".
- **Title** (Semibold, 15 px): the wordmark.
- **Body** (Regular and Semibold, 14 px): labels, move SAN, buttons, card titles, the tabs.
- **Small** (Regular, 12.5 px): captions, secondary lines, scores in the move list.
- **Label** (Semibold, 12.5 px): section headings and pills.
- **Mono** (Regular, 11.5 to 12.5 px): engine paths, click coordinates, colour
  values. Only things read as data.

### Named Rules
**The Real Minus Rule.** Negative scores use U+2212, never a hyphen: "−0.62".
Figures are Segoe UI's tabular ones, so scores line up in a column.

## Layout

A single column 344 px wide inside 16 by 14 px of window padding. The header
(mark, wordmark, engine status, hide, quit) sits over three tabs with a sliding
underline. Rhythm is 4, 8, 12 and 16 px; items are 8 px apart, 6 px inside cards.
Settings are rows: label left, control right, or label and value over a
full width slider. The game view reads top to bottom in order of urgency: the
evaluation, whose move it is, the last move and its verdict, then the mini board
beside the ranked moves, and last which side you play.

Sizes are given at 96 DPI and multiplied by the display's scale.

## Elevation & Depth

Flat, by necessity as much as choice: the overlay window cannot draw a shadow,
since a shadow is a translucent black that the colour key turns into either
nothing or a solid smear. Depth is tonal. The panel is darker than the page,
cards are one step lighter than the panel, and every raised thing carries a 1 px
hairline. Over the board, the chrome behind text is a near black fill inside a
1 px lighter rim, so it holds its edge on light and dark sites alike.

### Named Rules
**The Hairline Rule.** Elevation is a border one step lighter than the fill,
never a shadow.

## Shapes

Rounded, never pill shaped where it holds text in a block: 6 px on controls,
8 px on cards, 12 px on the window. Pills (the turn, the score tag, the callout)
are fully round, and so are rank badges and the knob of every switch and
slider. The board's own marks are square: 2 px corner brackets 3 px outside the
board, arms sized to 28 percent of a square.

## Components

### Buttons
- **Shape:** 6 px radius, 6 by 10 px padding, Semibold label.
- **Primary:** Signal Blue with white text. One per view: Detect board, Load,
  Find board from clicks.
- **Secondary:** Raised Graphite with a Strong Hairline border.
- **Ghost:** text only in Pewter until hovered.
- **Hover:** a lighter edge, #7AAAFF on primary and #586173 on secondary, so the
  label's contrast never drops. Pressed darkens the fill.
- **Icon buttons:** 28 px squares, the icon in Pewter, a Raised Graphite square
  on hover; the quit button's hover is red.

### Tabs
Semibold 14 px labels, 20 px apart, over a full width hairline. The current tab
is Paper with a 2 px blue underline that slides to it; the others are Slate Text,
Pewter on hover.

### Settings Rows
- **Toggle:** a 34 by 20 px track, Raised Graphite off and Signal Blue on, with a
  white knob that eases across. The whole row is the hit target.
- **Slider:** label and value on one line, then a 4 px track, blue to the knob,
  and a 7 px white knob with a soft blue halo on hover. Dragging, clicking, the
  wheel and the arrow keys all move it.
- **Segmented control:** a Raised Graphite pill with a sliding lighter selection.

### Cards and Banners
Cards are Raised Graphite, 8 px radius, a hairline border, 12 by 10 px padding.
Banners are the same neutral card with a 1 px border and an icon in their tone
at full strength, and an optional secondary action on the right. They are never
tinted: see the No Mud Rule.

### Status Dot
An 8 px dot in the state's colour. While something is under way, starting an
engine or searching, it breathes in size rather than fading.

### Verdict Chip
The move's annotation, `??`, `?` or `?!`, in white on the Blunder red and in
near black on the Mistake orange and the Inaccuracy yellow, as a small pill. The
same chip marks the move on the board and in the menu, beside the verdict's name
and what it cost, written unsigned: "Lost 3.2".

### Rank Disc
The one component shared by the board and the menu. Solid in its evaluation
colour where a move lands, on a 1 px ink keyline that keeps it apart from a
square of a similar tone and shows which of two crossing marks is on top; a
#0E0E10 disc with a coloured ring where it starts; the rank in white Semibold at
1.3 times the radius, centred on the cap height. The menu's mini board draws the
top move the same way: a near black line from the piece to its rank disc.
On the board: 6 to 14 px radius, anchored 20 percent into the top left of the
square, side by side when several share a square.

### Eval Bar (overlay)
An 8 to 14 px bar beside the board, the side being played at the bottom, easing
towards each new depth's score with a 0.2 s time constant. The score rides the
boundary in a chrome pill, with a spinner in it while the engine is still
searching the position on the board.

### Callout (overlay)
One pill above the board, below it when there is no room, with a neutral 1 px
rim: the verdict chip, then "They played **g6** lost **3.2** · best **Nf6**". At
the end of a game the chip reads `#` or `½` and the line reads "Checkmate, you
lose", the same words the menu uses.

### Mark
A white lens, a ring and a pupil drawn as vector paths, on a 24 px Signal Blue
tile with a 6 px radius, beside the wordmark "oracle".

## Do's and Don'ts

### Do:
- **Do** choose overlay colours as the opaque colours they will be on screen.
- **Do** write moves in SAN and scores from White's side with a real minus sign.
- **Do** keep the arrows black, the badges small, and each arrow on its own badge.
- **Do** animate by moving things (the tab underline, a callout sliding in,
  arrows extending, badges settling), 150 to 250 ms, and only when state changes.
- **Do** check any change to the menu or overlay in `tools/UiPreview` before and after.

### Don't:
- **Don't** use alpha to make anything over the board look translucent.
- **Don't** use muddy yellows, browns or olives anywhere Oracle draws.
- **Don't** use the accent for decoration, or a second accent at all.
- **Don't** fade anything in or out on the overlay.
- **Don't** write en dashes or em dashes in interface copy.
