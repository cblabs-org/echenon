#include <gtest/gtest.h>
#include "echenon/PowerTable.h"
#include "echenon/PowerEngine.h"
#include "echenon/PowerEngine.cpp"
#include <string>
#include <cmath>

using namespace echenon;

// =============================================================================
// PowerEngine & PowerTable Unit Tests (WO-02.2)
// =============================================================================

TEST(PowerEngineTest, KnownGridPoint) {
    PowerEngine engine;
    PowerTable table;
    table.clear();

    // Level 16 (index 15), 80 RPM (index 16: 80 / 5 = 16)
    table.watts[15][16] = 175;
    engine.setTable(table);

    float power = engine.computePower(16.0f, 80.0f);
    EXPECT_FLOAT_EQ(power, 175.0f);
}

TEST(PowerEngineTest, BilinearInterpolation) {
    PowerEngine engine;
    PowerTable table;
    table.clear();

    // Four grid points:
    // Level 10 (idx 9), 60 RPM (idx 12) = 100 W
    // Level 10 (idx 9), 65 RPM (idx 13) = 120 W
    // Level 11 (idx 10), 60 RPM (idx 12) = 110 W
    // Level 11 (idx 10), 65 RPM (idx 13) = 140 W
    table.watts[9][12] = 100;
    table.watts[9][13] = 120;
    table.watts[10][12] = 110;
    table.watts[10][13] = 140;
    engine.setTable(table);

    // Midpoint: Level 10.5, 62.5 RPM -> exact center
    // Expected: 0.25 * (100 + 120 + 110 + 140) = 117.5 W
    float pMid = engine.computePower(10.5f, 62.5f);
    EXPECT_NEAR(pMid, 117.5f, 0.001f);

    // Quarter-point: Level 10.25 (tL = 0.25), 61.25 RPM (tR = 0.25)
    // w0 = 100 * 0.75 + 120 * 0.25 = 75 + 30 = 105.0
    // w1 = 110 * 0.75 + 140 * 0.25 = 82.5 + 35 = 117.5
    // p = 105.0 * 0.75 + 117.5 * 0.25 = 78.75 + 29.375 = 108.125
    float pQuarter = engine.computePower(10.25f, 61.25f);
    EXPECT_NEAR(pQuarter, 108.125f, 0.001f);
}

TEST(PowerEngineTest, ZeroCadenceAlwaysReturnsZero) {
    PowerEngine engine;
    PowerTable table;
    table.clear();

    // Even with non-zero in column 0
    for (int l = 0; l < POWER_TABLE_LEVELS; ++l) {
        table.watts[l][0] = 50;
    }
    engine.setTable(table);

    EXPECT_FLOAT_EQ(engine.computePower(1.0f, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(engine.computePower(16.0f, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(engine.computePower(32.0f, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(engine.computePower(16.0f, -5.0f), 0.0f);
}

TEST(PowerEngineTest, BoundaryLevel1Rpm0) {
    PowerEngine engine;
    EXPECT_FLOAT_EQ(engine.computePower(1.0f, 0.0f), 0.0f);
    // Below bounds clamped to level 1, rpm 0
    EXPECT_FLOAT_EQ(engine.computePower(0.5f, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(engine.computePower(-10.0f, -2.0f), 0.0f);
}

TEST(PowerEngineTest, BoundaryLevel32Rpm150) {
    PowerEngine engine;
    PowerTable table;
    table.clear();

    // Corner cell: level 32 (idx 31), 150 RPM (idx 30)
    table.watts[31][30] = 920;
    engine.setTable(table);

    EXPECT_FLOAT_EQ(engine.computePower(32.0f, 150.0f), 920.0f);

    // Above bounds clamped to level 32, 150 RPM
    EXPECT_FLOAT_EQ(engine.computePower(35.0f, 160.0f), 920.0f);
    EXPECT_FLOAT_EQ(engine.computePower(50.0f, 200.0f), 920.0f);
}

TEST(PowerEngineTest, ClampingNegativePower) {
    PowerEngine engine;
    PowerTable table;
    table.clear();

    // Negative watt value in table
    table.watts[15][16] = -75;
    engine.setTable(table);

    float power = engine.computePower(16.0f, 80.0f);
    EXPECT_FLOAT_EQ(power, 0.0f); // Clamped to max(0, P)
}

TEST(PowerEngineTest, LevelInterpolation) {
    PowerEngine engine;
    PowerTable table;
    table.clear();

    // Level 16 (idx 15), 80 RPM (idx 16) = 120 W
    // Level 17 (idx 16), 80 RPM (idx 16) = 140 W
    table.watts[15][16] = 120;
    table.watts[16][16] = 140;
    engine.setTable(table);

    // Continuous level 16.5 at 80 RPM
    EXPECT_NEAR(engine.computePower(16.5f, 80.0f), 130.0f, 0.01f);
    EXPECT_NEAR(engine.computePower(16.25f, 80.0f), 125.0f, 0.01f);
    EXPECT_NEAR(engine.computePower(16.75f, 80.0f), 135.0f, 0.01f);
}

TEST(PowerEngineTest, CSVParseValid) {
    PowerTable table;

    // Generate valid 32-level CSV with 31 RPM bins per row
    std::string csv = "level";
    for (int rpm = 0; rpm <= 150; rpm += 5) {
        csv += ",rpm_" + std::to_string(rpm);
    }
    csv += "\r\n";

    for (int lvl = 1; lvl <= 32; ++lvl) {
        csv += std::to_string(lvl);
        for (int rpm = 0; rpm <= 150; rpm += 5) {
            int w = lvl * 10 + rpm;
            csv += "," + std::to_string(w);
        }
        csv += "\r\n";
    }

    bool success = table.parseCSV(csv.c_str(), csv.length());
    EXPECT_TRUE(success);
    EXPECT_EQ(table.tier, PowerTableTier::SELECTED);

    // Verify parsed values at level 1, 16, 32
    EXPECT_EQ(table.watts[0][0], 10);      // Level 1, 0 RPM
    EXPECT_EQ(table.watts[0][16], 10 + 80); // Level 1, 80 RPM
    EXPECT_EQ(table.watts[15][16], 160 + 80); // Level 16, 80 RPM = 240
    EXPECT_EQ(table.watts[31][30], 320 + 150); // Level 32, 150 RPM = 470
}

TEST(PowerEngineTest, CSVParseInvalidCases) {
    PowerTable table;

    // Null or empty
    EXPECT_FALSE(table.parseCSV(nullptr, 0));
    EXPECT_FALSE(table.parseCSV("", 0));

    // Incomplete table (only 1 row instead of 32)
    std::string incompleteCsv = "level";
    for (int rpm = 0; rpm <= 150; rpm += 5) incompleteCsv += ",rpm_" + std::to_string(rpm);
    incompleteCsv += "\n1";
    for (int rpm = 0; rpm <= 150; rpm += 5) incompleteCsv += ",10";
    incompleteCsv += "\n";
    EXPECT_FALSE(table.parseCSV(incompleteCsv.c_str(), incompleteCsv.length()));

    // Truncated row (missing columns)
    std::string truncatedRowCsv = "level,rpm_0,rpm_5\n1,10,20\n";
    EXPECT_FALSE(table.parseCSV(truncatedRowCsv.c_str(), truncatedRowCsv.length()));

    // Out-of-bounds level
    std::string badLevelCsv = "level";
    for (int rpm = 0; rpm <= 150; rpm += 5) badLevelCsv += ",rpm_" + std::to_string(rpm);
    badLevelCsv += "\n33";
    for (int rpm = 0; rpm <= 150; rpm += 5) badLevelCsv += ",10";
    badLevelCsv += "\n";
    EXPECT_FALSE(table.parseCSV(badLevelCsv.c_str(), badLevelCsv.length()));
}

TEST(PowerEngineTest, SerialiseDeserialiseRoundTrip) {
    PowerTable src;
    src.clear();
    src.tier = PowerTableTier::SELECTED;
    std::strncpy(src.id, "ex3-custom-v1", sizeof(src.id) - 1);

    for (int l = 0; l < POWER_TABLE_LEVELS; ++l) {
        for (int r = 0; r < POWER_TABLE_RPM_BINS; ++r) {
            src.watts[l][r] = static_cast<int16_t>((l + 1) * 100 + r * 5);
        }
    }

    uint8_t buffer[2048] = {};
    size_t written = src.serialise(buffer, sizeof(buffer));
    EXPECT_EQ(written, SERIALISED_TABLE_SIZE);
    EXPECT_EQ(written, 2024u);

    PowerTable dst;
    dst.clear();
    bool ok = dst.deserialise(buffer, written);
    EXPECT_TRUE(ok);

    EXPECT_EQ(dst.tier, PowerTableTier::SELECTED);
    EXPECT_STREQ(dst.id, "ex3-custom-v1");

    for (int l = 0; l < POWER_TABLE_LEVELS; ++l) {
        for (int r = 0; r < POWER_TABLE_RPM_BINS; ++r) {
            EXPECT_EQ(dst.watts[l][r], src.watts[l][r])
                << "Mismatch at level " << (l + 1) << " bin " << r;
        }
    }
}

TEST(PowerEngineTest, SerialiseDeserialiseCorruptBuffer) {
    PowerTable table;
    uint8_t buffer[2048] = {};

    // Buffer too small for serialise
    EXPECT_EQ(table.serialise(buffer, 100), 0u);

    // Buffer too small for deserialise
    EXPECT_FALSE(table.deserialise(buffer, 100));

    // Serialise valid table first
    size_t written = table.serialise(buffer, sizeof(buffer));
    EXPECT_EQ(written, SERIALISED_TABLE_SIZE);

    // Corrupt magic byte
    buffer[0] = 0x00;
    EXPECT_FALSE(table.deserialise(buffer, written));

    // Restore magic, corrupt version
    buffer[0] = static_cast<uint8_t>(POWER_TABLE_MAGIC & 0xFF);
    buffer[4] = 99; // Version 99 != 1
    EXPECT_FALSE(table.deserialise(buffer, written));
}

TEST(PowerEngineTest, DefaultTableSanity) {
    PowerEngine engine;
    engine.loadDefault();

    const PowerTable& table = engine.getTable();
    EXPECT_EQ(table.tier, PowerTableTier::DEFAULT);
    EXPECT_STREQ(table.id, "ex3-default");

    // Level 16 / 80 RPM sanity check (Physics P3r: 80.56 W, empirical sweep: 80 W)
    float p16_80 = engine.computePower(16.0f, 80.0f);
    EXPECT_GE(p16_80, 50.0f);
    EXPECT_LE(p16_80, 200.0f);
    EXPECT_NEAR(p16_80, 80.56f, 0.1f);

    // Strict monotonicity across levels at 80 RPM under continuous Physics P3r
    float prevPower = 0.0f;
    for (int lvl = 1; lvl <= 32; ++lvl) {
        float p = engine.computePower(static_cast<float>(lvl), 80.0f);
        EXPECT_GT(p, prevPower) << "Power must strictly increase with level at 80 RPM (level " << lvl << ")";
        prevPower = p;
    }

    // Strict monotonicity across cadence at level 16 under continuous Physics P3r
    prevPower = 0.0f;
    for (float rpm = 20.0f; rpm <= 150.0f; rpm += 10.0f) {
        float p = engine.computePower(16.0f, rpm);
        EXPECT_GT(p, prevPower) << "Power must strictly increase with cadence at level 16 (rpm " << rpm << ")";
        prevPower = p;
    }
}

TEST(PowerEngineTest, PhysicsP3rReferenceEnvelope) {
    // Verify reference points from docs/power-physics-model.md §7
    // Level 1 (p_norm = 0.0)
    EXPECT_NEAR(PowerEngine::computePowerPhysics(1.0f, 60.0f), 21.28f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(1.0f, 80.0f), 31.63f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(1.0f, 90.0f), 36.78f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(1.0f, 100.0f), 41.78f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(1.0f, 120.0f), 51.18f, 0.05f);

    // Level 5 (p_norm = 4/31)
    EXPECT_NEAR(PowerEngine::computePowerPhysics(5.0f, 60.0f), 22.69f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(5.0f, 80.0f), 33.83f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(5.0f, 100.0f), 44.75f, 0.05f);

    // Level 10 (p_norm = 9/31)
    EXPECT_NEAR(PowerEngine::computePowerPhysics(10.0f, 60.0f), 29.57f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(10.0f, 80.0f), 44.57f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(10.0f, 90.0f), 52.01f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(10.0f, 100.0f), 59.24f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(10.0f, 120.0f), 72.74f, 0.05f);

    // Level 16 (p_norm = 15/31)
    EXPECT_NEAR(PowerEngine::computePowerPhysics(16.0f, 60.0f), 52.65f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(16.0f, 80.0f), 80.56f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(16.0f, 90.0f), 94.41f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(16.0f, 100.0f), 107.81f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(16.0f, 120.0f), 132.70f, 0.05f);

    // Level 20 (p_norm = 19/31)
    EXPECT_NEAR(PowerEngine::computePowerPhysics(20.0f, 60.0f), 86.02f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(20.0f, 80.0f), 132.61f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(20.0f, 90.0f), 155.71f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(20.0f, 100.0f), 178.06f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(20.0f, 120.0f), 219.41f, 0.05f);

    // Level 24 (p_norm = 23/31)
    EXPECT_NEAR(PowerEngine::computePowerPhysics(24.0f, 60.0f), 142.57f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(24.0f, 80.0f), 220.82f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(24.0f, 90.0f), 259.61f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(24.0f, 100.0f), 297.11f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(24.0f, 120.0f), 366.36f, 0.05f);

    // Level 28 (p_norm = 27/31)
    EXPECT_NEAR(PowerEngine::computePowerPhysics(28.0f, 60.0f), 223.80f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(28.0f, 80.0f), 347.50f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(28.0f, 90.0f), 408.83f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(28.0f, 100.0f), 468.08f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(28.0f, 120.0f), 577.42f, 0.05f);

    // Level 32 (p_norm = 1.0)
    EXPECT_NEAR(PowerEngine::computePowerPhysics(32.0f, 60.0f), 313.26f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(32.0f, 80.0f), 487.03f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(32.0f, 90.0f), 573.19f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(32.0f, 100.0f), 656.40f, 0.05f);
    EXPECT_NEAR(PowerEngine::computePowerPhysics(32.0f, 120.0f), 809.88f, 0.05f);
}

TEST(PowerEngineTest, PhysicsP3rPotentiometerMapping) {
    // At POT_MIN_DEFAULT (171.0) -> matches Level 1.0
    EXPECT_FLOAT_EQ(
        PowerEngine::computePowerFromPot(171.0f, 80.0f),
        PowerEngine::computePowerPhysics(1.0f, 80.0f));

    // At POT_MAX_DEFAULT (1333.0) -> matches Level 32.0
    EXPECT_FLOAT_EQ(
        PowerEngine::computePowerFromPot(1333.0f, 90.0f),
        PowerEngine::computePowerPhysics(32.0f, 90.0f));

    // Midpoint ADC (752.0 -> p_norm = 0.5 -> continuous level = 16.5)
    EXPECT_NEAR(
        PowerEngine::computePowerFromPot(752.0f, 85.0f),
        PowerEngine::computePowerPhysics(16.5f, 85.0f),
        0.001f);

    // Edge cases: zero cadence, negative cadence, invalid pot range
    EXPECT_FLOAT_EQ(PowerEngine::computePowerFromPot(752.0f, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(PowerEngine::computePowerFromPot(752.0f, -10.0f), 0.0f);
    EXPECT_FLOAT_EQ(PowerEngine::computePowerFromPot(752.0f, 80.0f, 1000.0f, 500.0f), 0.0f);
    EXPECT_FLOAT_EQ(PowerEngine::computePowerPhysics(NAN, 80.0f), 0.0f);
    EXPECT_FLOAT_EQ(PowerEngine::computePowerPhysics(16.0f, NAN), 0.0f);
}

TEST(PowerEngineTest, TierOverrideSwitchesBetweenPhysicsAndTable) {
    PowerEngine engine;
    engine.loadDefault();

    // Tier 1 DEFAULT evaluates Physics P3r (~80.56 W at L16 / 80 RPM)
    EXPECT_NEAR(engine.computePower(16.0f, 80.0f), 80.56f, 0.05f);

    // Tier 2 SELECTED custom table override uses bilinear interpolation
    PowerTable custom;
    custom.clear();
    custom.tier = PowerTableTier::SELECTED;
    std::strncpy(custom.id, "custom-test", sizeof(custom.id) - 1);
    custom.watts[15][16] = 222;
    engine.setTable(custom);
    EXPECT_FLOAT_EQ(engine.computePower(16.0f, 80.0f), 222.0f);

    // Tier 3 RECALIBRATED override uses bilinear interpolation on recalibrated grid
    custom.tier = PowerTableTier::RECALIBRATED;
    custom.watts[15][16] = 245;
    engine.setTable(custom);
    EXPECT_FLOAT_EQ(engine.computePower(16.0f, 80.0f), 245.0f);

    // Restoring default re-enables Physics P3r evaluation
    engine.loadDefault();
    EXPECT_NEAR(engine.computePower(16.0f, 80.0f), 80.56f, 0.05f);
}

TEST(PowerEngineTest, TickCachesLastPower) {
    PowerEngine engine;
    engine.loadDefault();

    engine.tick(16.0f, 80.0f);
    float lastP = engine.getLastPower();
    EXPECT_FLOAT_EQ(lastP, engine.computePower(16.0f, 80.0f));

    engine.tick(20.0f, 90.0f);
    EXPECT_FLOAT_EQ(engine.getLastPower(), engine.computePower(20.0f, 90.0f));

    // Zero cadence tick sets last power to 0
    engine.tick(20.0f, 0.0f);
    EXPECT_FLOAT_EQ(engine.getLastPower(), 0.0f);
}
