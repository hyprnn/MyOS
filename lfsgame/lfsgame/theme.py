"""Colors and fonts. Change the look here only."""

BG = "#0b0f0c"
FG = "#c9d1c4"
DIM = "#5c675a"
ACCENT = "#7ee787"
WARN = "#e3b341"
ERR = "#ff7b72"
SELECT_BG = "#1f3a28"
PANEL_BG = "#10160f"

# First family that exists on the system wins (tk falls back automatically).
FONT_FAMILY = "DejaVu Sans Mono"
FONT_SIZE = 13


def font(bold: bool = False):
    return (FONT_FAMILY, FONT_SIZE, "bold" if bold else "normal")
LINK = "#79c0ff"
DIR = "#58a6ff"
CMD_BG = "#15241a"
HANDLE_BG = SELECT_BG


def font_sized(delta: int = 0, bold: bool = False):
    return (FONT_FAMILY, FONT_SIZE + delta, "bold" if bold else "normal")
