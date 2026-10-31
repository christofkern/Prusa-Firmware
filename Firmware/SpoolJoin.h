/// @file
#pragma once
#include <stdint.h>

// See documentation here: https://help.prusa3d.com/article/spooljoin-mmu2s_134252

namespace SpoolJoin {

class SpoolJoin {
public:
    SpoolJoin();

    enum class EEPROM : uint8_t {
        Unknown, ///< SpoolJoin is unknown while printer is booting up
        Enabled, ///< SpoolJoin is enabled in EEPROM
        Disabled, ///< SpoolJoin is disabled in EEPROM
        Empty = 0xFF ///< EEPROM has not been set before and all bits are 1 (0xFF) - either a new printer or user erased the memory
    };

    /// @brief Called when EEPROM is ready to be read
    void initSpoolJoinStatus();

    /// @brief Toggle SpoolJoin
    static void toggleSpoolJoin();

    /// @brief Check if SpoolJoin is enabled
    /// @returns true if enabled, false if disabled
    bool isSpoolJoinEnabled();

    /// @brief Update the saved MMU slot number so SpoolJoin can determine the next slot to use
    /// @param slot number of the slot to set
    void setSlot(uint8_t slot);

    /// @brief Fetch the next slot number.
    /// Slots run from 0 through MMU_FILAMENT_COUNT - 1.
    /// After the last slot is depleted, the next slot is 0.
    /// @returns the next slot
    uint8_t nextSlot();

private:
    /// @brief Currently used slot, from 0 through MMU_FILAMENT_COUNT - 1
    uint8_t currentMMUSlot;
};

extern SpoolJoin spooljoin;

} // namespace SpoolJoin
