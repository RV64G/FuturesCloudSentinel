import QtQuick

QtObject {
    id: theme

    // --- Configuration ---
    property color seedColor: "#0078d4" 
    property bool isDark: false

    // --- Helper: Color Blending & Utilities ---
    
    // Helper to ensure we have a color object, not a string
    function ensureColor(c) {
        // Qt.lighter(c, 1.0) is a trick to convert string to color object in QML JS
        return (typeof c === "string") ? Qt.lighter(c, 1.0) : c
    }

    function blend(c1, c2, ratio) {
        var col1 = ensureColor(c1)
        var col2 = ensureColor(c2)
        return Qt.rgba(
            col1.r * (1 - ratio) + col2.r * ratio,
            col1.g * (1 - ratio) + col2.g * ratio,
            col1.b * (1 - ratio) + col2.b * ratio,
            1.0
        )
    }

    function luminance(c) {
        var col = ensureColor(c)
        return 0.2126 * col.r + 0.7152 * col.g + 0.0722 * col.b
    }

    // Returns white or black based on background contrast
    function contrastText(bg) {
        return luminance(bg) > 0.5 ? "#000000" : "#FFFFFF"
    }

    // --- Palette Generation (Monet-like) ---
    
    // Define base colors as properties to ensure they are Color objects
    readonly property color _black: "#000000"
    readonly property color _white: "#FFFFFF"
    readonly property color _darkBase: "#121212" // Material Design Dark Base

    // Monet Magic:
    // To ensure backgrounds are subtle regardless of how vibrant the seed is,
    // we create a "Neutral Seed" which keeps the Hue but drastically reduces Saturation.
    // This mimics the "Neutral" tonal palette in Material Design 3.
    property color neutralSeed: Qt.hsla(seedColor.hslHue, 0.1, 0.5, 1.0)

    // 1. Backgrounds
    // Use neutralSeed for blending to avoid "neon" backgrounds.
    // Both Light and Dark modes blended at 10% (0.10) as requested.
    property color background: isDark ? blend(_darkBase, neutralSeed, 0.10) : blend(_white, neutralSeed, 0.15)
    property color colorOnBackground: isDark ? "#E6E1E5" : "#191C1D"

    // 2. Surface (Cards, Sheets)
    // Use neutralSeed.
    property color surface: isDark ? blend(background, _white, 0.05) : blend(background, neutralSeed, 0.05)
    property color colorOnSurface: colorOnBackground
    
    // Surface Variant (Used for Buttons/Input backgrounds)
    // Use neutralSeed.
    property color surfaceVariant: isDark ? blend(background, _white, 0.12) : blend(background, neutralSeed, 0.15)
    property color colorOnSurfaceVariant: isDark ? "#CAC4D0" : "#49454F"

    // 3. Primary
    // Use the ORIGINAL vibrant seedColor here.
    // Dark mode: Pastel/Lighter version of seed.
    property color primary: isDark ? blend(seedColor, _white, 0.3) : seedColor
    // Ensure text on primary is readable.
    property color colorOnPrimary: contrastText(primary)

    // 4. Primary Container (Active states, highlights)
    property color primaryContainer: isDark ? blend(seedColor, _black, 0.6) : blend(seedColor, _white, 0.8)
    property color colorOnPrimaryContainer: isDark ? blend(seedColor, _white, 0.9) : blend(seedColor, _black, 0.8)

    // 5. Secondary
    property color secondary: blend(primary, isDark ? _white : _black, 0.2)
    property color colorOnSecondary: contrastText(secondary)
    
    property color secondaryContainer: isDark ? blend(secondary, _black, 0.6) : blend(secondary, _white, 0.8)
    property color colorOnSecondaryContainer: isDark ? "#E8DEF8" : "#1D192B"

    // 6. Functional
    property color colorError: "#BA1A1A"
    property color colorOnError: "#FFFFFF"
    property color colorErrorContainer: "#FFDAD6"
    property color colorOnErrorContainer: "#410002"

    property color colorSuccess: "#006e1c"
    property color colorOnSuccess: "#ffffff"
    property color colorSuccessContainer: "#94f990"
    property color colorOnSuccessContainer: "#002204"

    property color colorOutline: isDark ? "#938F99" : "#79747E"

    // --- Legacy/Compat Properties ---
    property color baseColor: background
    property color onBaseColor: colorOnBackground

    // --- Actions ---
    function setSeedColor(color) {
        seedColor = color
    }

    function toggleDarkMode() {
        isDark = !isDark
    }
}
