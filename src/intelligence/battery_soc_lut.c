#include "battery_soc_lut.h"
#include <battery_sdk/battery_status.h>
#include <stddef.h>

/*
 * CR2032 primary lithium cell discharge curve.
 * Entries sorted by voltage_mv descending.
 *
 * | mV   | SoC%  | soc_pct_x100 |
 * |------|-------|--------------|
 * | 3000 | 100   | 10000        |
 * | 2900 |  90   |  9000        |
 * | 2800 |  70   |  7000        |
 * | 2700 |  50   |  5000        |
 * | 2600 |  30   |  3000        |
 * | 2500 |  20   |  2000        |
 * | 2400 |  10   |  1000        |
 * | 2200 |   5   |   500        |
 * | 2000 |   0   |     0        |
 */
static const battery_soc_lut_entry_t cr2032_entries[] = {
    { 3000, 10000 },
    { 2900,  9000 },
    { 2800,  7000 },
    { 2700,  5000 },
    { 2600,  3000 },
    { 2500,  2000 },
    { 2400,  1000 },
    { 2200,   500 },
    { 2000,     0 },
};

const battery_soc_lut_t battery_soc_lut_cr2032 = {
    .entries = cr2032_entries,
    .count   = sizeof(cr2032_entries) / sizeof(cr2032_entries[0]),
};

int battery_soc_lut_interpolate(const battery_soc_lut_t *lut,
                                uint16_t voltage_mv,
                                uint16_t *soc_pct_x100)
{
    const battery_soc_lut_entry_t *upper;
    const battery_soc_lut_entry_t *lower;
    uint32_t dv;   /* voltage span between entries */
    uint32_t ds;   /* soc span between entries */
    uint32_t voff; /* offset of voltage_mv from lower entry */

    if (lut == NULL || lut->entries == NULL || lut->count == 0 ||
        soc_pct_x100 == NULL) {
        return BATTERY_STATUS_INVALID_ARG;
    }

    /* Clamp above highest entry */
    if (voltage_mv >= lut->entries[0].voltage_mv) {
        *soc_pct_x100 = lut->entries[0].soc_pct_x100;
        return BATTERY_STATUS_OK;
    }

    /* Clamp below lowest entry */
    if (voltage_mv <= lut->entries[lut->count - 1].voltage_mv) {
        *soc_pct_x100 = lut->entries[lut->count - 1].soc_pct_x100;
        return BATTERY_STATUS_OK;
    }

    /* Find the two bracketing entries.
     * Table is sorted descending: entries[i].voltage_mv > entries[i+1].voltage_mv
     * We look for the first entry where voltage_mv >= entries[i+1].voltage_mv
     */
    for (size_t i = 0; i < lut->count - 1; i++) {
        upper = &lut->entries[i];
        lower = &lut->entries[i + 1];

        if (voltage_mv <= upper->voltage_mv && voltage_mv >= lower->voltage_mv) {
            /* Linear interpolation using integer math:
             *
             * soc = lower_soc + (voltage - lower_v) * (upper_soc - lower_soc)
             *                   / (upper_v - lower_v)
             */
            dv = (uint32_t)(upper->voltage_mv - lower->voltage_mv);
            voff = (uint32_t)(voltage_mv - lower->voltage_mv);

            if (upper->soc_pct_x100 >= lower->soc_pct_x100) {
                ds = (uint32_t)(upper->soc_pct_x100 - lower->soc_pct_x100);
                *soc_pct_x100 = lower->soc_pct_x100 +
                                (uint16_t)((voff * ds) / dv);
            } else {
                ds = (uint32_t)(lower->soc_pct_x100 - upper->soc_pct_x100);
                *soc_pct_x100 = lower->soc_pct_x100 -
                                (uint16_t)((voff * ds) / dv);
            }
            return BATTERY_STATUS_OK;
        }
    }

    /* Should never reach here given the clamp checks above */
    return BATTERY_STATUS_ERROR;
}
