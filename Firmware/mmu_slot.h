/// @file mmu_slot.h
/// @brief Host-testable helpers for MMU slot count, T-codes, labels and menus.
///
/// Tool indices are 0-based (T0 is the first slot). User-facing filament
/// numbers are 1-based. MMU_TOOL_UNKNOWN (0xFF) is the unloaded sentinel and
/// matches MMU2::FILAMENT_UNKNOWN.
#pragma once

#include <stdint.h>

static constexpr uint8_t MMU_TOOL_UNKNOWN = 0xFF;

/// Columns reserved for the status-screen filament field, including the
/// historical leading space. "10>11" is the widest legal value and fills it.
static constexpr uint8_t MMU_EXTRUDER_STATUS_COLUMNS = 5;

enum class MmuTCodeKind : uint8_t {
    Invalid,
    Tool,
    LoadToGears,  ///< Tx
    LoadToNozzle, ///< T?
    ContinueLoad  ///< Tc
};

struct MmuTCode {
    MmuTCodeKind kind;
    uint8_t tool; ///< 0-based when kind == Tool
    char special; ///< 'x', '?' or 'c' when the command is not a numbered tool
};

enum class MmuMenuMotion : uint8_t {
    Idle,
    Scrolled,
    Limit
};

struct MmuMenuCursor {
    int8_t first;
    int8_t cursor_pos;
};

enum class MmuSlotSuffix : uint8_t {
    SameLineWithSpace,
    NextLine
};

inline char mmu_ascii_lower(char c) {
    if (c >= 'A' && c <= 'Z')
        return (char)(c - 'A' + 'a');
    return c;
}

/// Stock single-extruder gate: only the first character 0-4, x, c or ? was
/// accepted. Kept so a disabled MMU still rejects T5-T9 as an invalid T-code.
inline bool mmu_tcode_rejected_when_mmu_disabled(char first) {
    const char c = mmu_ascii_lower(first);
    return (c < '0' || c > '4') && c != '?' && c != 'x' && c != 'c';
}

inline bool mmu_tcode_tail_ok(const char *p) {
    const unsigned char c = (unsigned char)(*p);
    return c == '\0' || c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

inline MmuTCode mmu_tcode_make(MmuTCodeKind kind, uint8_t tool, char special) {
    MmuTCode code;
    code.kind = kind;
    code.tool = tool;
    code.special = special;
    return code;
}

/// Parse a T-code. `command` points at the leading 'T' or 't'.
/// Numbered tools must be within `[0, filament_count)`. T12 is rejected when
/// the count is 12. Leading zeros (T01), extra digits and trailing junk are
/// rejected. Tx, T? and Tc keep their previous meanings.
inline MmuTCode mmu_parse_tcode(const char *command, uint8_t filament_count) {
    const char *p = command + 1;
    while (*p == ' ' || *p == '\t')
        ++p;

    const char c = mmu_ascii_lower(*p);
    if (c == 'x' || c == '?' || c == 'c') {
        if (!mmu_tcode_tail_ok(p + 1))
            return mmu_tcode_make(MmuTCodeKind::Invalid, 0, 0);
        if (c == 'x')
            return mmu_tcode_make(MmuTCodeKind::LoadToGears, 0, 'x');
        if (c == '?')
            return mmu_tcode_make(MmuTCodeKind::LoadToNozzle, 0, '?');
        return mmu_tcode_make(MmuTCodeKind::ContinueLoad, 0, 'c');
    }

    if (c < '0' || c > '9')
        return mmu_tcode_make(MmuTCodeKind::Invalid, 0, 0);

    // "T0" is tool 0. "T01" and "T00" are not produced by PrusaSlicer and are
    // rejected so a second digit cannot hide behind a leading zero.
    if (c == '0' && p[1] >= '0' && p[1] <= '9')
        return mmu_tcode_make(MmuTCodeKind::Invalid, 0, 0);

    unsigned value = 0;
    uint8_t digits = 0;
    while (*p >= '0' && *p <= '9') {
        value = value * 10u + (unsigned)(*p - '0');
        ++digits;
        ++p;
        if (digits > 2)
            return mmu_tcode_make(MmuTCodeKind::Invalid, 0, 0);
    }

    if (digits == 0 || value >= filament_count || !mmu_tcode_tail_ok(p))
        return mmu_tcode_make(MmuTCodeKind::Invalid, 0, 0);

    return mmu_tcode_make(MmuTCodeKind::Tool, (uint8_t)value, 0);
}

/// Next SpoolJoin slot. The last configured slot wraps to 0.
inline uint8_t mmu_spooljoin_next(uint8_t current, uint8_t filament_count) {
    if (filament_count == 0)
        return 0;
    if (current >= (uint8_t)(filament_count - 1))
        return 0;
    return (uint8_t)(current + 1);
}

/// 1-based filament label, or "?" when the tool is unknown.
/// `dst` must hold at least 3 bytes. Returns the character count.
inline uint8_t mmu_format_filament_label(char *dst, uint8_t tool_index) {
    if (tool_index == MMU_TOOL_UNKNOWN) {
        dst[0] = '?';
        dst[1] = '\0';
        return 1;
    }

    const uint8_t display = (uint8_t)(tool_index + 1);
    if (display >= 10) {
        dst[0] = (char)('0' + display / 10);
        dst[1] = (char)('0' + display % 10);
        dst[2] = '\0';
        return 2;
    }

    dst[0] = (char)('0' + display);
    dst[1] = '\0';
    return 1;
}

inline uint8_t mmu_filament_number_width(uint8_t tool_index) {
    if (tool_index == MMU_TOOL_UNKNOWN)
        return 1;
    return (tool_index >= 9) ? 2 : 1;
}

/// Status-screen filament field. Always fits in MMU_EXTRUDER_STATUS_COLUMNS.
/// Same-tool form is " F1" or " F10". A tool change is " 1>2", " 9>10" or
/// "10>11". The leading space is omitted only when the text is already 5 wide.
inline uint8_t mmu_format_extruder_status(char *dst, uint8_t current, uint8_t changing) {
    char left[3];
    char right[3];
    const uint8_t left_len = mmu_format_filament_label(left, current);
    const uint8_t right_len = mmu_format_filament_label(right, changing);
    uint8_t n = 0;

    if (current == changing) {
        const uint8_t content = (uint8_t)(1 + left_len);
        if (content < MMU_EXTRUDER_STATUS_COLUMNS)
            dst[n++] = ' ';
        dst[n++] = 'F';
        for (uint8_t i = 0; i < left_len; ++i)
            dst[n++] = left[i];
    } else {
        const uint8_t content = (uint8_t)(left_len + 1 + right_len);
        if (content < MMU_EXTRUDER_STATUS_COLUMNS)
            dst[n++] = ' ';
        for (uint8_t i = 0; i < left_len; ++i)
            dst[n++] = left[i];
        dst[n++] = '>';
        for (uint8_t i = 0; i < right_len; ++i)
            dst[n++] = right[i];
    }

    dst[n] = '\0';
    return n;
}

/// Item count for choose_menu_P. A disabled MMU keeps the historical 4 items,
/// or 5 when a trailing cancel entry is present. An enabled MMU lists every
/// configured slot, plus the cancel entry when requested.
inline int8_t mmu_choose_menu_item_count(bool mmu_enabled, bool has_last_item, uint8_t filament_count) {
    if (!mmu_enabled)
        return has_last_item ? 5 : 4;
    return (int8_t)(filament_count + (has_last_item ? 1 : 0));
}

inline uint8_t mmu_choose_menu_index(int8_t cursor_pos, int8_t first) {
    return (uint8_t)(cursor_pos + first - 1);
}

/// One encoder step of choose_menu_P. The cursor lives on LCD rows 1-3.
/// Past either end, the window scrolls until the first or last item is visible.
inline MmuMenuMotion mmu_menu_apply_encoder(MmuMenuCursor &cursor, int16_t encoder, int8_t items_no) {
    if (encoder == 0)
        return MmuMenuMotion::Idle;

    if (encoder < 0)
        cursor.cursor_pos--;
    else
        cursor.cursor_pos++;

    if (cursor.cursor_pos > 3) {
        cursor.cursor_pos = 3;
        if (cursor.first < (int8_t)(items_no - 3)) {
            cursor.first++;
            return MmuMenuMotion::Scrolled;
        }
        return MmuMenuMotion::Limit;
    }

    if (cursor.cursor_pos < 1) {
        cursor.cursor_pos = 1;
        if (cursor.first > 0) {
            cursor.first--;
            return MmuMenuMotion::Scrolled;
        }
        return MmuMenuMotion::Limit;
    }

    return MmuMenuMotion::Idle;
}

/// Where to draw a 1-based slot number after a PROGMEM message.
/// A message that still has room keeps the historical "text N" form.
/// Otherwise the number moves to the next LCD row instead of wrapping.
inline MmuSlotSuffix mmu_slot_suffix_place(uint8_t message_cols, uint8_t tool_index, uint8_t lcd_width) {
    const uint8_t num_len = mmu_filament_number_width(tool_index);
    if ((uint16_t)message_cols + 1u + num_len <= lcd_width)
        return MmuSlotSuffix::SameLineWithSpace;
    return MmuSlotSuffix::NextLine;
}
