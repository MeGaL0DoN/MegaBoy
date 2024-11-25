#pragma once

#include <GLFW/glfw3.h>
#include <array>

enum class MegaBoyKey
{
    A = 0,
    B = 1,
    Select = 2,
    Start = 3,
    Right = 4,
    Left = 5,
    Up = 6,
    Down = 7,
    Pause = 8,
    Reset = 9,
    FastForward = 10,
    QuickSave = 11,
    LoadQuickSave = 12,
    ScaleUp = 13,
    ScaleDown = 14,
    Screenshot = 15,
    SaveStateModifier = 16,
    LoadStateModifier = 17
};

class KeyBindManager
{
public:
    static constexpr int TOTAL_BINDS = 18;
    static constexpr int TOTAL_KEYS = TOTAL_BINDS - 2; // 2 are modifiers

    static inline std::array<int, TOTAL_BINDS> defaultKeyBinds()
    {
        return std::array
        {
            GLFW_KEY_X,            // A
            GLFW_KEY_Z,            // B
            GLFW_KEY_BACKSPACE,    // Select
            GLFW_KEY_ENTER,        // Start
            GLFW_KEY_RIGHT,        // Right
            GLFW_KEY_LEFT,         // Left
            GLFW_KEY_UP,           // Up
            GLFW_KEY_DOWN,         // Down
            GLFW_KEY_TAB,          // Pause
            GLFW_KEY_R,            // Reset
            GLFW_KEY_SPACE,        // FastForward
            GLFW_KEY_Q,		       // QuikSave
            GLFW_KEY_GRAVE_ACCENT, // LoadQuickSave
            GLFW_KEY_PAGE_UP,      // ScaleUp
            GLFW_KEY_PAGE_DOWN,    // ScaleDown
            GLFW_KEY_T,            // Screenshot

            GLFW_MOD_ALT,          // SaveStateModifier
            GLFW_MOD_SHIFT         // LoadStateModifier
        };
    }

    static inline std::array keyBinds { defaultKeyBinds() };

    static inline int getBind(MegaBoyKey key) 
	{
		return keyBinds[static_cast<int>(key)];
	}
    static inline void setBind(MegaBoyKey key, int newBind)
	{
		keyBinds[static_cast<int>(key)] = newBind;
	}

    static constexpr const char* getMegaBoyKeyName(MegaBoyKey key)
    {
        switch (key)
        {
            case MegaBoyKey::A: return "A";
            case MegaBoyKey::B: return "B";
            case MegaBoyKey::Select: return "Select";
            case MegaBoyKey::Start: return "Start";
            case MegaBoyKey::Right: return "Right";
            case MegaBoyKey::Left: return "Left";
            case MegaBoyKey::Up: return "Up";
            case MegaBoyKey::Down: return "Down";
            case MegaBoyKey::Pause: return "Pause";
            case MegaBoyKey::Reset: return "Reset";
            case MegaBoyKey::FastForward: return "Fast Forward";
            case MegaBoyKey::QuickSave: return "Quick Save";
            case MegaBoyKey::LoadQuickSave: return "Load Quick";
            case MegaBoyKey::ScaleUp: return "Scale Up";
            case MegaBoyKey::ScaleDown: return "Scale Down";
            case MegaBoyKey::Screenshot: return "Screenshot";
            case MegaBoyKey::SaveStateModifier: return "Save State (... + 1-9)";
            case MegaBoyKey::LoadStateModifier : return "Load State (... + 1-9)";
        }
    }

	static const char* getKeyName(int key)
	{
        switch (key) 
        {
        case GLFW_KEY_SPACE: return "Space";
        case GLFW_KEY_APOSTROPHE: return "'";
        case GLFW_KEY_COMMA: return ",";
        case GLFW_KEY_MINUS: return "-";
        case GLFW_KEY_PERIOD: return ".";
        case GLFW_KEY_SLASH: return "/";
        case GLFW_KEY_0: return "0";
        case GLFW_KEY_1: return "1";
        case GLFW_KEY_2: return "2";
        case GLFW_KEY_3: return "3";
        case GLFW_KEY_4: return "4";
        case GLFW_KEY_5: return "5";
        case GLFW_KEY_6: return "6";
        case GLFW_KEY_7: return "7";
        case GLFW_KEY_8: return "8";
        case GLFW_KEY_9: return "9";
        case GLFW_KEY_SEMICOLON: return ";";
        case GLFW_KEY_EQUAL: return "=";
        case GLFW_KEY_A: return "A";
        case GLFW_KEY_B: return "B";
        case GLFW_KEY_C: return "C";
        case GLFW_KEY_D: return "D";
        case GLFW_KEY_E: return "E";
        case GLFW_KEY_F: return "F";
        case GLFW_KEY_G: return "G";
        case GLFW_KEY_H: return "H";
        case GLFW_KEY_I: return "I";
        case GLFW_KEY_J: return "J";
        case GLFW_KEY_K: return "K";
        case GLFW_KEY_L: return "L";
        case GLFW_KEY_M: return "M";
        case GLFW_KEY_N: return "N";
        case GLFW_KEY_O: return "O";
        case GLFW_KEY_P: return "P";
        case GLFW_KEY_Q: return "Q";
        case GLFW_KEY_R: return "R";
        case GLFW_KEY_S: return "S";
        case GLFW_KEY_T: return "T";
        case GLFW_KEY_U: return "U";
        case GLFW_KEY_V: return "V";
        case GLFW_KEY_W: return "W";
        case GLFW_KEY_X: return "X";
        case GLFW_KEY_Y: return "Y";
        case GLFW_KEY_Z: return "Z";
        case GLFW_KEY_LEFT_BRACKET: return "[";
        case GLFW_KEY_BACKSLASH: return "\\";
        case GLFW_KEY_RIGHT_BRACKET: return "]";
        case GLFW_KEY_GRAVE_ACCENT: return "`";
        case GLFW_KEY_WORLD_1: return "World1";
        case GLFW_KEY_WORLD_2: return "World2";

        case GLFW_KEY_ESCAPE: return "Esc";
        case GLFW_KEY_ENTER: return "Enter";
        case GLFW_KEY_TAB: return "Tab";
        case GLFW_KEY_BACKSPACE: return "Backspace";
        case GLFW_KEY_INSERT: return "Insert";
        case GLFW_KEY_DELETE: return "Delete";
        case GLFW_KEY_RIGHT: return "Right";
        case GLFW_KEY_LEFT: return "Left";
        case GLFW_KEY_DOWN: return "Down";
        case GLFW_KEY_UP: return "Up";
        case GLFW_KEY_PAGE_UP: return "Page Up";
        case GLFW_KEY_PAGE_DOWN: return "Page Down";
        case GLFW_KEY_HOME: return "Home";
        case GLFW_KEY_END: return "End";
        case GLFW_KEY_CAPS_LOCK: return "CapsLock";
        case GLFW_KEY_SCROLL_LOCK: return "ScrollLock";
        case GLFW_KEY_NUM_LOCK: return "NumLock";
        case GLFW_KEY_PRINT_SCREEN: return "PrtSc";
        case GLFW_KEY_PAUSE: return "Pause";
        case GLFW_KEY_F1: return "F1";
        case GLFW_KEY_F2: return "F2";
        case GLFW_KEY_F3: return "F3";
        case GLFW_KEY_F4: return "F4";
        case GLFW_KEY_F5: return "F5";
        case GLFW_KEY_F6: return "F6";
        case GLFW_KEY_F7: return "F7";
        case GLFW_KEY_F8: return "F8";
        case GLFW_KEY_F9: return "F9";
        case GLFW_KEY_F10: return "F10";
        case GLFW_KEY_F11: return "F11";
        case GLFW_KEY_F12: return "F12";
        case GLFW_KEY_F13: return "F13";
        case GLFW_KEY_F14: return "F14";
        case GLFW_KEY_F15: return "F15";
        case GLFW_KEY_F16: return "F16";
        case GLFW_KEY_F17: return "F17";
        case GLFW_KEY_F18: return "F18";
        case GLFW_KEY_F19: return "F19";
        case GLFW_KEY_F20: return "F20";
        case GLFW_KEY_F21: return "F21";
        case GLFW_KEY_F22: return "F22";
        case GLFW_KEY_F23: return "F23";
        case GLFW_KEY_F24: return "F24";
        case GLFW_KEY_F25: return "F25";

        case GLFW_KEY_KP_0: return "Num0";
        case GLFW_KEY_KP_1: return "Num1";
        case GLFW_KEY_KP_2: return "Num2";
        case GLFW_KEY_KP_3: return "Num3";
        case GLFW_KEY_KP_4: return "Num4";
        case GLFW_KEY_KP_5: return "Num5";
        case GLFW_KEY_KP_6: return "Num6";
        case GLFW_KEY_KP_7: return "Num7";
        case GLFW_KEY_KP_8: return "Num8";
        case GLFW_KEY_KP_9: return "Num9";
        case GLFW_KEY_KP_DECIMAL: return "Num .";
        case GLFW_KEY_KP_DIVIDE: return "Num /";
        case GLFW_KEY_KP_MULTIPLY: return "Num *";
        case GLFW_KEY_KP_SUBTRACT: return "Num -";
        case GLFW_KEY_KP_ADD: return "Num +";
        case GLFW_KEY_KP_ENTER: return "NumEnter";
        case GLFW_KEY_KP_EQUAL: return "Num =";

        case GLFW_KEY_LEFT_SHIFT: return "L Shift";
        case GLFW_KEY_LEFT_CONTROL: return "L Ctrl";
        case GLFW_KEY_LEFT_ALT: return "L Alt";
        case GLFW_KEY_LEFT_SUPER: return "L Super";
        case GLFW_KEY_RIGHT_SHIFT: return "R Shift";
        case GLFW_KEY_RIGHT_CONTROL: return "R Ctrl";
        case GLFW_KEY_RIGHT_ALT: return "R Alt";
        case GLFW_KEY_RIGHT_SUPER: return "R Super";
        case GLFW_KEY_MENU: return "Menu";

        case GLFW_KEY_UNKNOWN: return "UNSET";
        default: return "UNKNOWN";
        }
	}
};