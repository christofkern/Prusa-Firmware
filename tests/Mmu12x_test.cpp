#include "catch2/catch_test_macros.hpp"

#include "../Firmware/mmu_slot.h"

#include <cstring>
#include <string>

static std::string label_of(uint8_t tool) {
    char buf[3];
    mmu_format_filament_label(buf, tool);
    return std::string(buf);
}

static std::string status_of(uint8_t current, uint8_t changing) {
    char buf[8];
    const uint8_t n = mmu_format_extruder_status(buf, current, changing);
    REQUIRE(n == std::strlen(buf));
    REQUIRE(n <= MMU_EXTRUDER_STATUS_COLUMNS);
    return std::string(buf);
}

static uint8_t walk_menu_to_end(int8_t items_no) {
    MmuMenuCursor cursor = {0, 1};
    uint8_t last = mmu_choose_menu_index(cursor.cursor_pos, cursor.first);
    for (int step = 0; step < 64; ++step) {
        const MmuMenuMotion motion = mmu_menu_apply_encoder(cursor, +1, items_no);
        last = mmu_choose_menu_index(cursor.cursor_pos, cursor.first);
        if (motion == MmuMenuMotion::Limit)
            return last;
    }
    FAIL("menu did not reach its last entry");
    return last;
}

TEST_CASE("T-codes accept tools 0 through 11 and reject 12", "[mmu12x]") {
    for (uint8_t tool = 0; tool < 12; ++tool) {
        char command[8];
        command[0] = 'T';
        if (tool < 10) {
            command[1] = (char)('0' + tool);
            command[2] = '\0';
        } else {
            command[1] = (char)('0' + tool / 10);
            command[2] = (char)('0' + tool % 10);
            command[3] = '\0';
        }

        const MmuTCode parsed = mmu_parse_tcode(command, 12);
        REQUIRE(parsed.kind == MmuTCodeKind::Tool);
        REQUIRE(parsed.tool == tool);
    }

    REQUIRE(mmu_parse_tcode("T12", 12).kind == MmuTCodeKind::Invalid);
    REQUIRE(mmu_parse_tcode("T13", 12).kind == MmuTCodeKind::Invalid);
    REQUIRE(mmu_parse_tcode("T99", 12).kind == MmuTCodeKind::Invalid);
    REQUIRE(mmu_parse_tcode("T100", 12).kind == MmuTCodeKind::Invalid);
    REQUIRE(mmu_parse_tcode("T", 12).kind == MmuTCodeKind::Invalid);
    REQUIRE(mmu_parse_tcode("T-1", 12).kind == MmuTCodeKind::Invalid);
    REQUIRE(mmu_parse_tcode("T+1", 12).kind == MmuTCodeKind::Invalid);
    REQUIRE(mmu_parse_tcode("T1.1", 12).kind == MmuTCodeKind::Invalid);
    REQUIRE(mmu_parse_tcode("T01", 12).kind == MmuTCodeKind::Invalid);
    REQUIRE(mmu_parse_tcode("T00", 12).kind == MmuTCodeKind::Invalid);
    REQUIRE(mmu_parse_tcode("T10x", 12).kind == MmuTCodeKind::Invalid);
    REQUIRE(mmu_parse_tcode("T1a", 12).kind == MmuTCodeKind::Invalid);
    REQUIRE(mmu_parse_tcode("T11 ", 12).tool == 11);
    REQUIRE(mmu_parse_tcode("T 10", 12).tool == 10);
    REQUIRE(mmu_parse_tcode("t11", 12).tool == 11);
}

TEST_CASE("T-code specials and a five-slot count stay distinct", "[mmu12x]") {
    const MmuTCode load_gears = mmu_parse_tcode("Tx", 12);
    REQUIRE(load_gears.kind == MmuTCodeKind::LoadToGears);
    REQUIRE(load_gears.special == 'x');
    REQUIRE(mmu_parse_tcode("TX", 12).kind == MmuTCodeKind::LoadToGears);

    const MmuTCode load_nozzle = mmu_parse_tcode("T?", 12);
    REQUIRE(load_nozzle.kind == MmuTCodeKind::LoadToNozzle);
    REQUIRE(load_nozzle.special == '?');

    const MmuTCode cont = mmu_parse_tcode("Tc", 12);
    REQUIRE(cont.kind == MmuTCodeKind::ContinueLoad);
    REQUIRE(cont.special == 'c');
    REQUIRE(mmu_parse_tcode("TC", 12).kind == MmuTCodeKind::ContinueLoad);
    REQUIRE(mmu_parse_tcode("T?1", 12).kind == MmuTCodeKind::Invalid);
    REQUIRE(mmu_parse_tcode("Tx ", 12).kind == MmuTCodeKind::LoadToGears);

    REQUIRE(mmu_parse_tcode("T4", 5).tool == 4);
    REQUIRE(mmu_parse_tcode("T5", 5).kind == MmuTCodeKind::Invalid);
    REQUIRE(mmu_parse_tcode("T11", 5).kind == MmuTCodeKind::Invalid);
}

TEST_CASE("disabled MMU still uses the original first-character gate", "[mmu12x]") {
    for (char c = '0'; c <= '4'; ++c)
        REQUIRE_FALSE(mmu_tcode_rejected_when_mmu_disabled(c));
    REQUIRE_FALSE(mmu_tcode_rejected_when_mmu_disabled('x'));
    REQUIRE_FALSE(mmu_tcode_rejected_when_mmu_disabled('X'));
    REQUIRE_FALSE(mmu_tcode_rejected_when_mmu_disabled('c'));
    REQUIRE_FALSE(mmu_tcode_rejected_when_mmu_disabled('?'));
    // T10 starts with '1', so the disabled path still treats it as numeric.
    REQUIRE_FALSE(mmu_tcode_rejected_when_mmu_disabled('1'));
    for (char c = '5'; c <= '9'; ++c)
        REQUIRE(mmu_tcode_rejected_when_mmu_disabled(c));
    REQUIRE(mmu_tcode_rejected_when_mmu_disabled('a'));
    REQUIRE(mmu_tcode_rejected_when_mmu_disabled('\0'));
}

TEST_CASE("filament labels are 1-based through 12", "[mmu12x]") {
    REQUIRE(label_of(0) == "1");
    REQUIRE(label_of(8) == "9");
    REQUIRE(label_of(9) == "10");
    REQUIRE(label_of(10) == "11");
    REQUIRE(label_of(11) == "12");
    REQUIRE(label_of(MMU_TOOL_UNKNOWN) == "?");
    REQUIRE(mmu_filament_number_width(0) == 1);
    REQUIRE(mmu_filament_number_width(8) == 1);
    REQUIRE(mmu_filament_number_width(9) == 2);
    REQUIRE(mmu_filament_number_width(11) == 2);
}

TEST_CASE("extruder status stays inside five columns", "[mmu12x]") {
    REQUIRE(status_of(0, 0) == " F1");
    REQUIRE(status_of(9, 9) == " F10");
    REQUIRE(status_of(11, 11) == " F12");
    REQUIRE(status_of(MMU_TOOL_UNKNOWN, MMU_TOOL_UNKNOWN) == " F?");
    REQUIRE(status_of(0, 1) == " 1>2");
    REQUIRE(status_of(8, 9) == " 9>10");
    REQUIRE(status_of(9, 8) == " 10>9");
    REQUIRE(status_of(9, 10) == "10>11");
    REQUIRE(status_of(10, 11) == "11>12");
    REQUIRE(status_of(MMU_TOOL_UNKNOWN, 10) == " ?>11");
    REQUIRE(status_of(10, MMU_TOOL_UNKNOWN) == " 11>?");
    REQUIRE(status_of(0, MMU_TOOL_UNKNOWN) == " 1>?");

    const uint8_t tools[] = {0, 1, 8, 9, 10, 11, MMU_TOOL_UNKNOWN};
    for (uint8_t current : tools) {
        for (uint8_t changing : tools) {
            const std::string field = status_of(current, changing);
            REQUIRE(field.size() >= 2);
            REQUIRE(field.size() <= MMU_EXTRUDER_STATUS_COLUMNS);
        }
    }
}

TEST_CASE("SpoolJoin walks every slot and wraps", "[mmu12x]") {
    uint8_t slot = 0;
    for (uint8_t expected = 1; expected < 12; ++expected) {
        slot = mmu_spooljoin_next(slot, 12);
        REQUIRE(slot == expected);
    }
    REQUIRE(mmu_spooljoin_next(11, 12) == 0);
    REQUIRE(mmu_spooljoin_next(12, 12) == 0);
    REQUIRE(mmu_spooljoin_next(4, 5) == 0);
    REQUIRE(mmu_spooljoin_next(3, 5) == 4);
    REQUIRE(mmu_spooljoin_next(0, 5) == 1);
}

TEST_CASE("filament menu can select the last of twelve slots", "[mmu12x]") {
    REQUIRE(mmu_choose_menu_item_count(false, false, 12) == 4);
    REQUIRE(mmu_choose_menu_item_count(false, true, 12) == 5);
    REQUIRE(mmu_choose_menu_item_count(true, false, 12) == 12);
    REQUIRE(mmu_choose_menu_item_count(true, true, 12) == 13);
    REQUIRE(mmu_choose_menu_item_count(true, false, 5) == 5);

    REQUIRE(walk_menu_to_end(12) == 11);
    REQUIRE(walk_menu_to_end(13) == 12);
    REQUIRE(walk_menu_to_end(4) == 3);
    REQUIRE(walk_menu_to_end(5) == 4);

    MmuMenuCursor cursor = {0, 1};
    bool seen[12] = {};
    seen[mmu_choose_menu_index(cursor.cursor_pos, cursor.first)] = true;
    for (int step = 0; step < 40; ++step) {
        if (mmu_menu_apply_encoder(cursor, +1, 12) == MmuMenuMotion::Limit)
            break;
        const uint8_t index = mmu_choose_menu_index(cursor.cursor_pos, cursor.first);
        REQUIRE(index < 12);
        seen[index] = true;
    }
    for (bool visited : seen)
        REQUIRE(visited);

    while (mmu_menu_apply_encoder(cursor, -1, 12) != MmuMenuMotion::Limit) {
    }
    REQUIRE(mmu_choose_menu_index(cursor.cursor_pos, cursor.first) == 0);
}

TEST_CASE("long unload text does not share its row with a two-digit slot", "[mmu12x]") {
    REQUIRE(mmu_slot_suffix_place(18, 4, 20) == MmuSlotSuffix::SameLineWithSpace);
    REQUIRE(mmu_slot_suffix_place(18, 9, 20) == MmuSlotSuffix::NextLine);
    REQUIRE(mmu_slot_suffix_place(16, 11, 20) == MmuSlotSuffix::SameLineWithSpace);
    REQUIRE(mmu_slot_suffix_place(20, 0, 20) == MmuSlotSuffix::NextLine);
}
