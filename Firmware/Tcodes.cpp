#include "Tcodes.h"
#include "SpoolJoin.h"
#include "Marlin.h"
#include "language.h"
#include "messages.h"
#include "mmu2.h"
#include "mmu_slot.h"
#include "ultralcd.h"
#include <avr/pgmspace.h>
#include <stdint.h>
#include <stdio.h>

static const char duplicate_Tcode_ignored[] PROGMEM = "Duplicate T-code ignored.";

inline void TCodeInvalid() {
    SERIAL_ECHOLNPGM("Invalid T code.");
}

static void TCodeToolChange(uint8_t tool) {
    if (tool == MMU2::mmu2.get_current_tool()) {
        // don't execute the same T-code twice in a row
        puts_P(duplicate_Tcode_ignored);
        return;
    }
#if defined(MMU_HAS_CUTTER) && defined(MMU_ALWAYS_CUT)
    if (EEPROM_MMU_CUTTER_ENABLED_always == eeprom_read_byte((uint8_t *)EEPROM_MMU_CUTTER_ENABLED)) {
        MMU2::mmu2.cut_filament(tool);
    }
#endif // defined(MMU_HAS_CUTTER) && defined(MMU_ALWAYS_CUT)
    MMU2::mmu2.tool_change(tool);
}

void TCodes(char *const strchr_pointer, const uint8_t codeValue) {
    if (!MMU2::mmu2.Enabled()) {
        // Single-extruder behavior is unchanged. The first character must be
        // 0-4, '?' , 'x' or 'c'. T5 and above stay "Invalid T code."
        uint8_t index = 1;
        for ( /*nothing*/ ; strchr_pointer[index] == ' ' || strchr_pointer[index] == '\t'; index++)
            ;

        strchr_pointer[index] = mmu_ascii_lower(strchr_pointer[index]);

        if (mmu_tcode_rejected_when_mmu_disabled(strchr_pointer[index])) {
            TCodeInvalid();
        } else if (strchr_pointer[index] == 'x' || strchr_pointer[index] == '?' || strchr_pointer[index] == 'c') {
            // MMU commands do nothing while the MMU is disabled.
        } else {
            SERIAL_ECHO_START;
            if (codeValue >= EXTRUDERS) {
                SERIAL_ECHO('T');
                SERIAL_ECHOLN(codeValue + '0');
                SERIAL_ECHOLNRPGM(_n("Invalid extruder")); ////MSG_INVALID_EXTRUDER
            } else {
// @@TODO               if (code_seen('F')) {
//                    next_feedrate = code_value();
//                    if (next_feedrate > 0.0) {
//                        feedrate = next_feedrate;
//                    }
//                }
                SERIAL_ECHORPGM(_n("Active Extruder: 0")); ////MSG_ACTIVE_EXTRUDER
            }
        }
        return;
    }

    // codeValue comes from strtol and would accept "T10x" or "T12". The parser
    // below is the authority for an enabled MMU and forwards the full tool index.
    (void)codeValue;
    const MmuTCode parsed = mmu_parse_tcode(strchr_pointer, MMU_FILAMENT_COUNT);
    switch (parsed.kind) {
    case MmuTCodeKind::Invalid:
        TCodeInvalid();
        break;
    case MmuTCodeKind::LoadToGears:
    case MmuTCodeKind::LoadToNozzle:
        MMU2::mmu2.tool_change(parsed.special, choose_menu_P(_T(MSG_SELECT_FILAMENT), MSG_FILAMENT));
        break;
    case MmuTCodeKind::ContinueLoad:
        MMU2::mmu2.tool_change(parsed.special, MMU2::mmu2.get_current_tool());
        break;
    case MmuTCodeKind::Tool:
        TCodeToolChange(parsed.tool);
        break;
    }
}
