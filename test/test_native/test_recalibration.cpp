#include <gtest/gtest.h>
#include "echenon/CalibrationWizard.h"
#include "echenon/CalibrationWizard.cpp"
#include "echenon/RecalibrationUI.h"
#include "echenon/RecalibrationUI.cpp"
#include "echenon/ResistanceSensor.h"
#include "echenon/CadenceSensor.h"
#include "echenon/LevelMapping.h"
#include "echenon/PowerTable.h"
#include <cmath>

using namespace echenon;

// =============================================================================
// CalibrationWizard Tests (WO-02.3)
// =============================================================================

TEST(CalibrationWizardTest, StartStop) {
    CalibrationWizard wizard;
    EXPECT_FALSE(wizard.isActive());
    EXPECT_FALSE(wizard.isComplete());
    EXPECT_EQ(wizard.getCurrentStep(), 0);
    EXPECT_EQ(wizard.getState(), CalibrationWizard::State::IDLE);

    wizard.start();
    EXPECT_TRUE(wizard.isActive());
    EXPECT_FALSE(wizard.isComplete());
    EXPECT_EQ(wizard.getCurrentStep(), 1);
    EXPECT_EQ(wizard.getState(), CalibrationWizard::State::STARTED);

    wizard.stop();
    EXPECT_FALSE(wizard.isActive());
    EXPECT_FALSE(wizard.isComplete());
    EXPECT_EQ(wizard.getCurrentStep(), 0);
    EXPECT_EQ(wizard.getState(), CalibrationWizard::State::IDLE);
}

TEST(CalibrationWizardTest, RecordAll32Levels) {
    ResistanceSensor sensor(1);
    CalibrationWizard wizard(&sensor);

    wizard.start();

    for (uint8_t lvl = 1; lvl <= 32; ++lvl) {
        EXPECT_TRUE(wizard.isActive());
        EXPECT_EQ(wizard.getCurrentStep(), lvl);

        // Inject simulated wiper value for this level
        uint16_t simWiper = static_cast<uint16_t>(100 + lvl * 35);
        sensor.injectSample(simWiper);

        EXPECT_TRUE(wizard.recordLevel(lvl));
    }

    EXPECT_FALSE(wizard.isActive());
    EXPECT_TRUE(wizard.isComplete());
    EXPECT_TRUE(wizard.isPosDone());
    EXPECT_EQ(wizard.getState(), CalibrationWizard::State::COMPLETED);
}

TEST(CalibrationWizardTest, TableValuesMatchInjected) {
    ResistanceSensor sensor(1);
    CalibrationWizard wizard(&sensor);

    wizard.start();

    uint16_t injected[32];
    for (int i = 0; i < 32; ++i) {
        injected[i] = static_cast<uint16_t>(165 + i * 36);
        sensor.reset(static_cast<float>(injected[i]));
        EXPECT_TRUE(wizard.recordLevel(static_cast<uint8_t>(i + 1)));
    }

    const uint16_t* tbl = wizard.getTable();
    ASSERT_NE(tbl, nullptr);
    for (int i = 0; i < 32; ++i) {
        EXPECT_EQ(tbl[i], injected[i]);
        EXPECT_EQ(wizard.getTableEntry(static_cast<uint8_t>(i + 1)), injected[i]);
    }

    // Boundary checks
    EXPECT_EQ(wizard.getTableEntry(0), 0);
    EXPECT_EQ(wizard.getTableEntry(33), 0);
}

TEST(CalibrationWizardTest, OutOfOrderRejection) {
    ResistanceSensor sensor(1);
    CalibrationWizard wizard(&sensor);

    wizard.start();
    EXPECT_EQ(wizard.getCurrentStep(), 1);

    // Attempt to record level 2 when expecting level 1
    EXPECT_FALSE(wizard.recordLevel(2));
    EXPECT_EQ(wizard.getCurrentStep(), 1);

    // Record level 1 correctly
    sensor.injectSample(200);
    EXPECT_TRUE(wizard.recordLevel(1));
    EXPECT_EQ(wizard.getCurrentStep(), 2);

    // Attempt to re-record level 1
    EXPECT_FALSE(wizard.recordLevel(1));
    EXPECT_EQ(wizard.getCurrentStep(), 2);
}

TEST(CalibrationWizardTest, ResetRestoresIdle) {
    ResistanceSensor sensor(1);
    CalibrationWizard wizard(&sensor);

    wizard.start();
    sensor.injectSample(200);
    wizard.recordLevel(1);
    EXPECT_EQ(wizard.getTableEntry(1), 200);

    wizard.reset();
    EXPECT_FALSE(wizard.isActive());
    EXPECT_FALSE(wizard.isComplete());
    EXPECT_FALSE(wizard.isPosDone());
    EXPECT_EQ(wizard.getCurrentStep(), 0);
    EXPECT_EQ(wizard.getTableEntry(1), 0);
}

// =============================================================================
// RecalibrationUI Tests (WO-02.3)
// =============================================================================

TEST(RecalibrationUITest, PointStorage) {
    RecalibrationUI ui;
    EXPECT_EQ(ui.getPointCount(), 0u);
    EXPECT_TRUE(ui.getPoints().empty());

    bool ok = ui.addPoint(5, 120.0f, 33, 75.0f, 300, 1000);
    EXPECT_TRUE(ok);
    EXPECT_EQ(ui.getPointCount(), 1u);

    const auto& pts = ui.getPoints();
    ASSERT_EQ(pts.size(), 1u);
    EXPECT_EQ(pts[0].echelonLevel, 5);
    EXPECT_FLOAT_EQ(pts[0].reportedPower, 120.0f);
    EXPECT_EQ(pts[0].internalLevel, 33);
    EXPECT_FLOAT_EQ(pts[0].cadenceRPM, 75.0f);
    EXPECT_EQ(pts[0].wiperCount, 300);
    EXPECT_EQ(pts[0].timestamp, 1000u);
}

TEST(RecalibrationUITest, MaxLimitBoundedTo100) {
    RecalibrationUI ui;
    EXPECT_EQ(ui.getMaxPoints(), 100u);

    // Add 105 distinct points across levels and cadence bins
    for (int i = 0; i < 105; ++i) {
        uint8_t lvl = static_cast<uint8_t>((i % 32) + 1);
        float rpm = static_cast<float>((i / 32) * 25 + 10);
        bool added = ui.addPoint(lvl, 100.0f + i, 10, rpm, 250, 1000 + i);
        if (i < 100) {
            EXPECT_TRUE(added) << "Failed to add point " << i;
        } else {
            EXPECT_FALSE(added) << "Should have rejected point " << i << " beyond max 100";
        }
    }

    EXPECT_EQ(ui.getPointCount(), 100u);
    EXPECT_EQ(ui.getPoints().size(), 100u);
}

TEST(RecalibrationUITest, ResetClearsPoints) {
    RecalibrationUI ui;
    ui.addPoint(1, 50.0f, 0, 60.0f, 150, 500);
    ui.addPoint(2, 60.0f, 8, 60.0f, 180, 600);
    ui.addPoint(3, 70.0f, 16, 60.0f, 210, 700);
    EXPECT_EQ(ui.getPointCount(), 3u);

    ui.reset();
    EXPECT_EQ(ui.getPointCount(), 0u);
    EXPECT_TRUE(ui.getPoints().empty());
}

TEST(RecalibrationUITest, AveragingReplacementRule) {
    RecalibrationUI ui;

    // Add point 1: Level 10, 150.0 W, 80 RPM (bin 16), timestamp 1000
    EXPECT_TRUE(ui.addPoint(10, 150.0f, 74, 80.0f, 488, 1000));
    EXPECT_EQ(ui.getPointCount(), 1u);
    EXPECT_FLOAT_EQ(ui.getPoints()[0].reportedPower, 150.0f);

    // Add point 2: Same level (10) and cadence in same RPM bin (81 RPM -> bin 16)
    // Should replace point 1 with new power and timestamp
    EXPECT_TRUE(ui.addPoint(10, 175.0f, 74, 81.0f, 488, 2000));
    EXPECT_EQ(ui.getPointCount(), 1u); // count remains 1
    EXPECT_FLOAT_EQ(ui.getPoints()[0].reportedPower, 175.0f); // replaced!
    EXPECT_FLOAT_EQ(ui.getPoints()[0].cadenceRPM, 81.0f);
    EXPECT_EQ(ui.getPoints()[0].timestamp, 2000u);
}

TEST(RecalibrationUITest, DifferentRpmBinAppends) {
    RecalibrationUI ui;

    // Point at 80 RPM (bin 16)
    EXPECT_TRUE(ui.addPoint(10, 150.0f, 74, 80.0f, 488, 1000));
    // Point at 90 RPM (bin 18) - different bin, same level
    EXPECT_TRUE(ui.addPoint(10, 190.0f, 74, 90.0f, 488, 2000));

    EXPECT_EQ(ui.getPointCount(), 2u);
}

TEST(RecalibrationUITest, ApplyToPowerTable) {
    PowerTable table;
    table.clear();
    EXPECT_EQ(table.tier, PowerTableTier::DEFAULT);

    RecalibrationUI ui;
    ui.addPoint(10, 175.0f, 74, 80.0f, 488, 2000); // Level 10 (idx 9), 80 RPM (bin 16)
    ui.addPoint(20, 320.0f, 156, 100.0f, 850, 3000); // Level 20 (idx 19), 100 RPM (bin 20)

    ui.applyToPowerTable(table);

    EXPECT_EQ(table.tier, PowerTableTier::RECALIBRATED);
    EXPECT_EQ(table.watts[9][16], 175);
    EXPECT_EQ(table.watts[19][20], 320);
}
