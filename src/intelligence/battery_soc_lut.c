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

/*
 * LiPo single-cell (3.7 V nominal) discharge curve.
 * Entries sorted by voltage_mv descending.
 *
 * Typical open-circuit voltage (OCV) profile for a LiCoO2/graphite
 * lithium-polymer cell at room temperature, 0.2 C discharge rate.
 *
 * Data synthesised from multiple sources:
 *   - Grepow LiPo battery voltage guide
 *   - RC community resting-voltage measurements
 *   - Adafruit Li-Ion/LiPoly documentation
 *
 * Operating range: 4200 mV (fully charged) to 3000 mV (cutoff).
 * The curve has three regions:
 *   1. Initial drop   (4200-4050 mV): ~100%-90% — fast voltage fall
 *   2. Flat plateau    (4050-3700 mV): ~90%-10%  — most usable energy
 *   3. Steep cliff     (3700-3000 mV): ~10%-0%   — rapid voltage collapse
 *
 * 11 points chosen to minimise interpolation error on the non-linear
 * curve — extra density in the steep knee region below 3700 mV.
 *
 * | mV   | SoC%  | soc_pct_x100 | Region      |
 * |------|-------|--------------|-------------|
 * | 4200 | 100   | 10000        | Full charge |
 * | 4150 |  95   |  9500        | Initial drop|
 * | 4060 |  85   |  8500        | Initial drop|
 * | 3980 |  75   |  7500        | Plateau     |
 * | 3920 |  65   |  6500        | Plateau     |
 * | 3870 |  55   |  5500        | Plateau     |
 * | 3830 |  45   |  4500        | Plateau     |
 * | 3790 |  30   |  3000        | Plateau end |
 * | 3700 |  10   |  1000        | Knee        |
 * | 3500 |   3   |   300        | Steep cliff |
 * | 3000 |   0   |     0        | Cutoff      |
 */
static const battery_soc_lut_entry_t lipo_1s_entries[] = {
    { 4200, 10000 },  /* 100% — fully charged */
    { 4150,  9500 },  /*  95% */
    { 4060,  8500 },  /*  85% */
    { 3980,  7500 },  /*  75% */
    { 3920,  6500 },  /*  65% */
    { 3870,  5500 },  /*  55% */
    { 3830,  4500 },  /*  45% */
    { 3790,  3000 },  /*  30% — plateau ends */
    { 3700,  1000 },  /*  10% — knee region */
    { 3500,   300 },  /*   3% — steep cliff */
    { 3000,     0 },  /*   0% — cutoff */
};

const battery_soc_lut_t battery_soc_lut_lipo_1s = {
    .entries = lipo_1s_entries,
    .count   = sizeof(lipo_1s_entries) / sizeof(lipo_1s_entries[0]),
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
