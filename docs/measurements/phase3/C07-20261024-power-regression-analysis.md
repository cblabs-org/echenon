# C07 — Multivariate Power Surface Regression Analysis

**Date:** 2026-10-24  
**Board:** EX-3 on Waveshare ESP32-S3-Nano  
**Firmware:** `v0.3.0-dev.14` on branch `calibration-power/wo7-sweep-execution`  
**Data files:**
- [`evidence/sweep_20261024.csv`](evidence/sweep_20261024.csv) — 320-point empirical power sweep (32 resistance levels × 10 cadence steps, 40–130 RPM)
- [`evidence/power_regression_analysis.xlsx`](evidence/power_regression_analysis.xlsx) — 5-sheet workbook with raw dataset, model comparison, live Excel prediction formulas, 32×10 power surface heatmap, and residual scatter charts

---

## 1. Executive Summary

Can mechanical output power ($P$, in Watts) on the Echelon EX-3 be accurately predicted strictly from two direct hardware sensor inputs — the resistance motor potentiometer wiper (`pot_adc`, 12-bit ADC range `171–1333`) and flywheel/crank cadence (`cadence_rpm`, `40–130 RPM`) — without relying on the proprietary Echelon console's internal state or discrete level counter?

**Yes.** Across $N = 317$ clean empirical measurements spanning the entire $32 \times 10$ operating envelope ($12\text{ W}$ to $861\text{ W}$):

1. **4-Parameter First-Principles Electromechanical Model (`Physics P3r` — Production Selected ★):**
   $$P(\tilde{p}, \text{cad}) = 0.0048839 \cdot \exp\!\Big(1.630327 \cdot \big(1 - \cos(2.635447 \cdot \tilde{p})\big)\Big) \cdot \frac{\text{cad}^2}{1 + (\text{cad} / 128.1495)^2} + 0.114260 \cdot \text{cad}$$
   Achieves **$R^2 = 0.999522$** ($\text{Adjusted } R^2 = 0.999518$), **$\text{RMSE} = 3.77\text{ W}$**, **$\text{MAE} = 2.71\text{ W}$** (improving to **$R^2 = 0.999627$, $\text{RMSE} = 3.33\text{ W}$** on calibrated continuous level $\tilde{p} = (\ell - 1)/31$), and exact $P(\tilde{p}, 0) = 0\text{ W}$ using only $k = 4$ physical parameters. See [`docs/power-physics-model.md`](../../power-physics-model.md) for the complete derivation and firmware specification.
2. **10-Term Bivariate Cubic Polynomial (Model 8 — Best Empirical Polynomial):** Achieves **$R^2 = 0.997928$** ($\text{Adjusted } R^2 = 0.997867$), **$\text{RMSE} = 7.84\text{ W}$**, **$\text{MAE} = 5.81\text{ W}$** (median percentage error **$4.26\%$**), and maximum absolute residual **$31.80\text{ W}$** at $861\text{ W}$ peak load.
3. **5-Parameter Shifted Power Law (Model 10 — Best Compact Power-Law Model):**
   $$P = a \cdot (\text{pot} - p_0)^b \cdot \text{cad}^c + d$$
   Achieves **$R^2 = 0.990529$** ($\text{Adjusted } R^2 = 0.990408$), **$\text{RMSE} = 16.77\text{ W}$**, and **$\text{MAE} = 14.01\text{ W}$**, revealing the underlying physical scaling exponents: $b \approx 2.592$ (air-gap magnetic flux concentration) and $c \approx 1.431$ (eddy-current + console skin-effect scaling).
4. **4-Parameter Separable Physics Model (Model 4 — Best Ultralight Polynomial):**
   $$P = \text{cad} \times (a \cdot \text{pot}^2 + b \cdot \text{pot} + c) + d$$
   Achieves **$R^2 = 0.985096$** ($\text{Adjusted } R^2 = 0.984953$), **$\text{RMSE} = 21.04\text{ W}$**, and **$\text{MAE} = 17.34\text{ W}$** using only 4 floating-point constants and 4 multiply-accumulate operations.

Because the ESP32-S3 includes a single-precision hardware FPU, the **4-Parameter First-Principles Electromechanical Model (`Physics P3r`)** executes in $< 3\,\mu\text{s}$ using $16\text{ bytes}$ of `constexpr float` constants (`4 × 4B`), achieves less than half the RMSE of the 10-term Cubic Polynomial ($3.77\text{ W}$ vs $7.84\text{ W}$), and guarantees exact physical boundary conditions ($P = 0\text{ W}$ at $0\text{ RPM}$, strict monotonicity, and closed-form analytical invertibility).

---

## 2. Model Comparison Table

Eleven candidate functional forms were fitted via ordinary least squares (`numpy.linalg.lstsq`) or non-linear Levenberg-Marquardt optimization (`scipy.optimize.curve_fit`) against the $N = 317$ clean data points (`sweep_20261024.csv` minus the 3 known ADB OCR/screen-lag glitches analyzed in Section 5).

$$\text{Adjusted } R^2 = 1 - \frac{(1 - R^2)(N - 1)}{N - k}$$

where $N = 317$ and $k$ is the total number of fitted parameters.

| # | Model Name | Functional Form $P(\text{pot}, \text{cad})$ | $R^2$ | Adjusted $R^2$ | RMSE (W) | MAE (W) | Max Residual (W) | Params ($k$) |
|---|---|---|---:|---:|---:|---:|---:|---:|
| **8** | **Cubic Polynomial ★** | **10-term bivariate cubic in $(\text{pot}, \text{cad})$** | **0.997928** | **0.997867** | **7.84** | **5.81** | **31.80** | **10** |
| **10** | **Shifted Power Law** | $a \cdot (\text{pot} - p_0)^b \cdot \text{cad}^c + d$ | **0.990529** | **0.990408** | **16.77** | **14.01** | **49.88** | **5** |
| **3** | **Simple Power Law** | $a \cdot \text{pot}^b \cdot \text{cad}^c$ | **0.985838** | **0.985748** | **20.51** | **16.65** | **48.89** | **3** |
| **4** | **Physics ($\text{cad} \times f_2(\text{pot})$)** | $\text{cad} \cdot (a\cdot\text{pot}^2 + b\cdot\text{pot} + c) + d$ | **0.985096** | **0.984953** | **21.04** | **17.34** | **64.69** | **4** |
| 6 | Extended Cam ($\text{disp}^2$) | $\text{cad} \cdot (a\cdot\text{disp}^2 + b\cdot\text{disp} + c) + d$ | 0.983535 | 0.983377 | 22.12 | 18.07 | 75.73 | 4 |
| 11 | Exponential × Cadence | $\text{cad} \cdot (a \cdot e^{b\cdot\text{pot}} + c)$ | 0.978504 | 0.978367 | 25.27 | 17.19 | 111.85 | 3 |
| 9 | Pure Multiplicative | $(a\cdot\text{pot}^2 + b\cdot\text{pot} + c) \cdot \text{cad}$ | 0.976429 | 0.976279 | 26.46 | 19.54 | 93.54 | 3 |
| 2 | Full Quadratic Polynomial | $a\cdot\text{pot}^2 + b\cdot\text{pot}\cdot\text{cad} + c\cdot\text{cad}^2 + d\cdot\text{pot} + e\cdot\text{cad} + f$ | 0.970187 | 0.969708 | 29.76 | 23.57 | 88.74 | 6 |
| 7 | Product Polynomial | $a\cdot\text{pot}\cdot\text{cad} + b\cdot\text{pot}^2 + c\cdot\text{cad} + d\cdot\text{pot} + e$ | 0.970100 | 0.969717 | 29.80 | 23.56 | 89.64 | 5 |
| 5 | Cam Sinusoidal | $a\cdot\text{cad}\cdot(1-\cos\theta(\text{pot})) + b\cdot\text{cad} + c$ | 0.885137 | 0.884405 | 58.41 | 46.88 | 232.66 | 3 |
| 1 | Linear Baseline | $a\cdot\text{pot} + b\cdot\text{cad} + c$ | 0.732050 | 0.730343 | 89.21 | 69.88 | 373.41 | 3 |

---

## 3. Winning Models & Explicit Mathematical Equations

### 3.1 Primary Production Model — Model 8: 10-Term Bivariate Cubic Polynomial

The bivariate cubic polynomial captures both the non-linear magnet air-gap flux curve ($\text{pot}^2, \text{pot}^3$) and the cross-coupled torque-cadence terms ($\text{pot}^2 \cdot \text{cad}$ and $\text{pot} \cdot \text{cad}^2$):

$$P(\text{pot}, \text{cad}) = c_0\,\text{pot}^3 + c_1\,\text{pot}^2\,\text{cad} + c_2\,\text{pot}\,\text{cad}^2 + c_3\,\text{cad}^3 + c_4\,\text{pot}^2 + c_5\,\text{pot}\,\text{cad} + c_6\,\text{cad}^2 + c_7\,\text{pot} + c_8\,\text{cad} + c_9$$

#### Exact Fitted Coefficients (Model 8)

| Coefficient | Term | Exact Value | Scientific Notation |
|---|---|---:|---:|
| $c_0$ | $\text{pot}^3$ | `0.00000035699787` | $+3.56997873 \times 10^{-7}$ |
| $c_1$ | $\text{pot}^2 \cdot \text{cad}$ | `0.00000863303998` | $+8.63303998 \times 10^{-6}$ |
| $c_2$ | $\text{pot} \cdot \text{cad}^2$ | `-0.00000529222661` | $-5.29222661 \times 10^{-6}$ |
| $c_3$ | $\text{cad}^3$ | `-0.00010763726856` | $-1.07637269 \times 10^{-4}$ |
| $c_4$ | $\text{pot}^2$ | `-0.00096680368628` | $-9.66803686 \times 10^{-4}$ |
| $c_5$ | $\text{pot} \cdot \text{cad}$ | `-0.00587801001828` | $-5.87801002 \times 10^{-3}$ |
| $c_6$ | $\text{cad}^2$ | `0.02914636866886` | $+2.91463687 \times 10^{-2}$ |
| $c_7$ | $\text{pot}$ | `0.60301483024085` | $+6.03014830 \times 10^{-1}$ |
| $c_8$ | $\text{cad}$ | `-0.75395907093483` | $-7.53959071 \times 10^{-1}$ |
| $c_9$ | $\text{constant}$ | `-53.14074222674272` | $-5.31407422 \times 10^{+1}$ |

#### Residual Performance across Cadence Bands (Model 8)

| Cadence (RPM) | Mean Residual (W) | Std Dev (W) | Min Residual (W) | Max Residual (W) |
|---:|---:|---:|---:|---:|
| 40 | $+0.51$ | $9.93$ | $-14.12$ | $+31.80$ |
| 50 | $-1.18$ | $4.73$ | $-7.50$ | $+12.16$ |
| 60 | $+0.16$ | $4.84$ | $-10.97$ | $+7.14$ |
| 70 | $+0.37$ | $9.08$ | $-27.84$ | $+11.60$ |
| 80 | $+1.43$ | $5.34$ | $-18.68$ | $+11.10$ |
| 90 | $-2.94$ | $7.29$ | $-13.93$ | $+16.94$ |
| 100 | $+2.67$ | $8.24$ | $-9.70$ | $+25.22$ |
| 110 | $+0.18$ | $7.83$ | $-12.97$ | $+19.45$ |
| 120 | $-2.36$ | $8.22$ | $-19.27$ | $+8.79$ |
| 130 | $+1.14$ | $10.58$ | $-19.46$ | $+19.58$ |

- **Percentage Error Distribution (Model 8):** Median error = **$4.26\%$**, Mean error = **$8.56\%$**, 90th percentile = **$19.78\%$** (primarily at low power $<25\text{ W}$ where a $3\text{ W}$ error is $\sim 20\%$).

---

### 3.2 Best Compact Non-Linear Model — Model 10: Shifted Power Law

$$P(\text{pot}, \text{cad}) = a \cdot \max(\text{pot} - p_0,\, 1)^b \cdot \text{cad}^c + d$$

| Parameter | Physical Meaning | Fitted Value |
|---|---|---:|
| $a$ | Global scaling constant | $1.318257 \times 10^{-8}$ |
| $p_0$ | Zero-torque baseline potentiometer ADC offset (near Level 1 = 171 ADC) | $199.95\text{ ADC}$ |
| $b$ | Effective magnet flux / air-gap power exponent | $2.592024$ |
| $c$ | Effective cadence angular velocity exponent | $1.431340$ |
| $d$ | Baseline mechanical/drivetrain power offset | $+23.3580\text{ W}$ |

- **Accuracy:** $R^2 = 0.990529$, $\text{Adjusted } R^2 = 0.990408$, $\text{RMSE} = 16.77\text{ W}$, $\text{MAE} = 14.01\text{ W}$, $\text{Max Residual} = 49.88\text{ W}$.

---

### 3.3 Best Compact Polynomial Model — Model 4: Separable Physics ($\text{cad} \times f_2(\text{pot})$)

Factoring out cadence into a multiplicative braking-torque polynomial in $\text{pot}$:

$$P(\text{pot}, \text{cad}) = \text{cad} \times \left(a \cdot \text{pot}^2 + b \cdot \text{pot} + c\right) + d$$

| Coefficient | Term | Exact Value | Scientific Notation |
|---|---|---:|---:|
| $a$ | $\text{cad} \cdot \text{pot}^2$ | `0.000006843469` | $+6.84346868 \times 10^{-6}$ |
| $b$ | $\text{cad} \cdot \text{pot}$ | `-0.005518710550` | $-5.51871055 \times 10^{-3}$ |
| $c$ | $\text{cad}$ | `1.962544915459` | $+1.96254492 \times 10^{+0}$ |
| $d$ | $\text{constant}$ | `-49.907399181658` | $-4.99073992 \times 10^{+1}$ |

- **Accuracy:** $R^2 = 0.985096$, $\text{Adjusted } R^2 = 0.984953$, $\text{RMSE} = 21.04\text{ W}$, $\text{MAE} = 17.34\text{ W}$, $\text{Max Residual} = 64.69\text{ W}$.
- Note why **Model 4 ($k=4$, $R^2 = 0.9851$)** dramatically outperforms **Model 2 (Full Quadratic, $k=6$, $R^2 = 0.9702$)**: Model 2 lacks the cubic cross-term $\text{cad} \cdot \text{pot}^2$, whereas Model 4 places its quadratic curvature directly inside the multiplicative $\text{cad} \times f(\text{pot})$ product!

---

## 4. Physical Interpretation of the Mathematical Form

Why does the empirical power surface follow $P \propto \text{cad}^{1.43} \cdot (\text{pot} - p_0)^{2.59}$ (and why does the 10-term cubic polynomial capture $99.79\%$ of the variance)? Three physical mechanisms govern the Echelon EX-3 resistance powertrain:

1. **Eccentric Cam Linkage ($\text{pot} \to \theta_{\text{cam}} \to d_{\text{gap}}$):**
   - The resistance motor rotates an eccentric cam through $\sim 151^\circ$ across Levels 1–32, linearly tracked by the potentiometer wiper (`pot_adc` from $171$ to $1333$, $\sim 37.5\text{ ADC/level}$).
   - Pure sinusoidal cam displacement alone (`Model 5: Cam Sinusoidal`, $1 - \cos\theta$) yields only $R^2 = 0.8851$ because braking torque is **not** linear in cam displacement.
   - In Levels 1–10 ($\text{pot} \in [171, 495]$), the permanent magnet calliper is far from the aluminum flywheel rim; changes in $\text{pot}$ produce almost negligible change in magnetic flux linkages ($12\text{ W} \to 16\text{ W}$ at $40\text{ RPM}$).

2. **Air-Gap Magnetic Flux Density & Eddy-Current Braking ($\tau_{\text{brake}} \propto B_{\text{gap}}^2$):**
   - By Lorentz eddy-current braking theory, the retarding torque $\tau$ on a conducting disk of conductivity $\sigma$ and thickness $h$ passing through magnetic flux density $B_{\text{gap}}$ at angular velocity $\omega$ is:
     $$\tau_{\text{eddy}} \propto \sigma \, h \, A \, r^2 \, B_{\text{gap}}^2(d) \, \omega$$
   - Because the magnetic field $B_{\text{gap}}(d)$ of the permanent magnet pair decays rapidly with air-gap distance $d$, and $B_{\text{gap}}$ is **squared** in the eddy-current force law, the effective braking torque per RPM grows super-quadratically with potentiometer position ($\text{pot}^{2.59}$ in Model 10, or $c_1\,\text{pot}^2\,\text{cad} + c_0\,\text{pot}^3$ in Model 8).
   - Note that in Model 4, the vertex of the parabola $f(\text{pot}) = 6.843\times 10^{-6}\,\text{pot}^2 - 5.519\times 10^{-3}\,\text{pot} + 1.963$ occurs at $\text{pot} = -b/(2a) \approx 403\text{ ADC}$ (Level 7), matching the physical transition where the magnet enters close proximity to the flywheel rim.

3. **Cadence Scaling ($P = \tau \cdot \omega$ vs Eddy-Current Skin/Inductive Effects):**
   - At constant braking torque $\tau$, mechanical power scales linearly with angular velocity ($P = \tau \omega \propto \text{cad}^1$).
   - If eddy currents were purely resistive (low-frequency limit), $\tau_{\text{eddy}} \propto \omega$, which would yield $P = \tau_{\text{eddy}} \omega \propto \text{cad}^2$.
   - At realistic flywheel rim speeds ($40–130\text{ RPM}$ crank $\times$ belt step-up ratio), the self-inductance and magnetic skin effect (reaction field of the eddy currents) partially saturate the linear $\tau \propto \omega$ growth, resulting in an intermediate exponent $\text{cad}^{1.43}$ (Model 10) — or in polynomial form, a positive $\text{pot}^2 \cdot \text{cad}$ term paired with a small negative $\text{pot} \cdot \text{cad}^2$ saturation correction ($c_2 = -5.29\times 10^{-6}$ in Model 8).

---

## 5. Analysis of the 3 ADB Screen-Lag Outlier Points

During the automated 320-point sweep (`sweep_20261024.csv`), monotonicity checks within each cadence column flagged exactly **3 non-monotonic points** out of 320 ($0.94\%$):

| Row Index | Cadence (RPM) | Level | `pot_adc` | Recorded ADB Power (W) | Prev Level Power (W) | Next Level Power (W) | Neighbor Midpoint (W) | Model 8 (Cubic) Prediction (W) | Model 4 (Physics) Prediction (W) | Implied ADB Glitch Error |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 147 | 80 | 20 | 861 | **100 W** | 109 W (L19) | 140 W (L21) | **124.5 W** | **126.3 W** | 132.8 W | $-26.3\text{ W}$ ($-20.8\%$) |
| 187 | 90 | 28 | 1166 | **252 W** | 342 W (L27) | 430 W (L29) | **386.0 W** | **380.8 W** | 385.0 W | $-128.8\text{ W}$ ($-33.8\%$) |
| 203 | 100 | 12 | 569 | **57 W** | 63 W (L11) | 76 W (L13) | **69.5 W** | **66.1 W** | 53.9 W | $-9.1\text{ W}$ ($-13.8\%$) |

### Root Cause Confirmation
- In all three cases, the hardware potentiometer ADC reading (`pot_adc = 861, 1166, 569`) was **completely normal and on-target** for the commanded level, proving the physical resistance motor reached the correct position.
- However, the Echelon Fit Android app UI (scraped via ADB) briefly lagged or displayed a stale transition frame from a lower resistance level (`100W` matches `C=70/L20`, `252W` is a transient mid-seek value, and `57W` matches `C=90/L12`).
- **Model 8 (Cubic Polynomial)** predicts **$126.3\text{ W}$**, **$380.8\text{ W}$**, and **$66.1\text{ W}$** for these three points — agreeing with adjacent-level linear interpolation ($124.5\text{ W}$, $386.0\text{ W}$, $69.5\text{ W}$) to within **$1.8\text{ W}$ to $5.2\text{ W}$**.

---

## 6. ESP32-S3 Firmware Implementation (`predictPower`)

### 6.1 Lookup-Table Interpolation vs Direct Formula Evaluation

| Criterion | 2D Lookup Table (`32 × 10` Bilinear LUT) | Direct Formula (Model 8 Cubic Polynomial) | Direct Formula (Model 4 Physics) |
|---|---|---|---|
| **Flash Footprint** | `640 B` (`32×10 uint16_t`) + `64 B` pot-to-level LUT | **`40 B`** (`10 × float`) | **`16 B`** (`4 × float`) |
| **Execution Time (ESP32-S3 @ 240 MHz)** | $\sim 0.8\,\mu\text{s}$ (binary search + bilinear interp) | **$\sim 0.35\,\mu\text{s}$** (Horner FMA on hardware FPU) | **$\sim 0.12\,\mu\text{s}$** (4 FPU ops) |
| **Continuous ADC Resolution** | Quantizes or requires non-uniform X-grid search | **Native continuous `pot_adc` input** (no level rounding error) | **Native continuous `pot_adc` input** |
| **Glitch Immunity** | Requires manual patching of 3 LUT cells | **Inherently smoothed** across all 317 points | **Inherently smoothed** |
| **Accuracy ($R^2$ / RMSE / MAE)** | Grid-exact, but $\pm 8\text{ W}$ hysteresis between levels | **$R^2 = 0.9979$ / $7.84\text{ W}$ / $5.81\text{ W}$** | $R^2 = 0.9851$ / $21.04\text{ W}$ / $17.34\text{ W}$ |

**Recommendation:** Use **`Physics P3r` (4-parameter First-Principles Electromechanical Model, §8.2)** as the primary production firmware predictor in `PowerEngine` (see [`docs/power-physics-model.md`](../../power-physics-model.md)). It achieves **$R^2 = 0.999522$** ($\text{RMSE} = 3.77\text{ W}$, $\text{MAE} = 2.71\text{ W}$) with only 4 physical constants, guarantees $P(\tilde{p}, 0) = 0\text{ W}$, and outperforms **Model 8 (10-term Cubic Polynomial)** by more than $2\times$ in RMSE ($3.77\text{ W}$ vs $7.84\text{ W}$).

### 6.2 Production C++ Implementation (ESP32-S3)

```cpp
#include <cmath>
#include <stdint.h>

/**
 * @brief Primary Production Predictor: 4-Parameter First-Principles Electromechanical Model (Physics P3r).
 *        R^2 = 0.999522, Adjusted R^2 = 0.999518, RMSE = 3.77 W, MAE = 2.71 W (N=317).
 *        When driven by calibrated continuous level l in [1.0, 32.0] (p_norm = (l - 1)/31),
 *        achieves R^2 = 0.999627, RMSE = 3.33 W, MAE = 2.41 W.
 */
float predictPowerPhysicsP3r(float pNorm, float cadenceRpm) {
    if (cadenceRpm <= 0.0f) {
        return 0.0f;
    }
    if (pNorm < 0.0f) pNorm = 0.0f;
    if (pNorm > 1.0f) pNorm = 1.0f;

    constexpr float kA          = 0.0048839f; // Baseline eddy-current scale (W / RPM^2)
    constexpr float kBeta       = 1.630327f;  // Exponential air-gap magnetic flux gain
    constexpr float kCadCrit    = 128.1495f;  // Kloss inductive rollover cadence (RPM)
    constexpr float kCFric      = 0.114260f;  // Drivetrain belt/bearing Coulomb drag (W / RPM)
    constexpr float kCamSweep   = 2.635447f;  // 151 deg eccentric cam sweep (radians)

    const float u      = 1.0f - cosf(kCamSweep * pNorm);
    const float r      = cadenceRpm / kCadCrit;
    const float pEddy  = kA * expf(kBeta * u) * (cadenceRpm * cadenceRpm) / (1.0f + r * r);
    const float watts  = pEddy + kCFric * cadenceRpm;

    if (watts <= 0.0f) return 0.0f;
    if (watts >= 2000.0f) return 2000.0f;
    return watts;
}

/**
 * @brief Predict mechanical output power (Watts) directly from potentiometer ADC
 *        and cadence (RPM) using the C07 10-term bivariate cubic regression (Model 8).
 *
 * Fitted on N=317 clean points across Levels 1-32 (potAdc: 171..1333) and
 * Cadence 40..130 RPM:
 *   R^2 = 0.997928, Adjusted R^2 = 0.997867, RMSE = 7.84 W, MAE = 5.81 W
 *
 * @param potAdc     12-bit resistance wiper ADC reading (valid sweep range: 171..1333)
 * @param cadenceRpm Crank cadence in RPM (0 returns 0 W; calibrated range: 40..130 RPM)
 * @return uint16_t  Estimated power in Watts, clamped to [0, 2000]
 */
uint16_t predictPower(uint16_t potAdc, uint16_t cadenceRpm) {
    if (cadenceRpm == 0) {
        return 0;
    }

    // Model 8 (Cubic Polynomial) coefficients
    constexpr float c0 =  3.56997873e-07f; // pot^3
    constexpr float c1 =  8.63303998e-06f; // pot^2 * cad
    constexpr float c2 = -5.29222661e-06f; // pot * cad^2
    constexpr float c3 = -1.07637269e-04f; // cad^3
    constexpr float c4 = -9.66803686e-04f; // pot^2
    constexpr float c5 = -5.87801002e-03f; // pot * cad
    constexpr float c6 =  2.91463687e-02f; // cad^2
    constexpr float c7 =  6.03014830e-01f; // pot
    constexpr float c8 = -7.53959071e-01f; // cad
    constexpr float c9 = -5.31407422e+01f; // constant

    const float p = static_cast<float>(potAdc);
    const float c = static_cast<float>(cadenceRpm);
    const float p2 = p * p;
    const float c2_val = c * c;

    // Grouped evaluation to minimize FPU multiplications
    const float watts = (c0 * p + c1 * c + c4) * p2
                      + (c2 * p + c3 * c + c6) * c2_val
                      + (c5 * c + c7) * p
                      + (c8 * c + c9);

    if (watts <= 0.0f) {
        return 0;
    }
    if (watts >= 2000.0f) {
        return 2000;
    }
    return static_cast<uint16_t>(watts + 0.5f);
}

/**
 * @brief Compact 4-parameter physics model fallback (Model 4: R^2 = 0.985096, RMSE = 21.04 W).
 *        P = cadence * (a * pot^2 + b * pot + c) + d
 */
uint16_t predictPowerPhysics(uint16_t potAdc, uint16_t cadenceRpm) {
    if (cadenceRpm == 0) {
        return 0;
    }

    constexpr float a =  6.84346868e-06f;
    constexpr float b = -5.51871055e-03f;
    constexpr float c =  1.96254492e+00f;
    constexpr float d = -4.99073992e+01f;

    const float pot = static_cast<float>(potAdc);
    const float cad = static_cast<float>(cadenceRpm);

    const float torqueFactor = (a * pot + b) * pot + c;
    const float watts = cad * torqueFactor + d;

    if (watts <= 0.0f) {
        return 0;
    }
    return static_cast<uint16_t>(watts + 0.5f);
}
```

---

## 7. Deep Dive: First-Principles Electromechanical Physics & Firmware Reverse-Engineering

Following the initial 11-model empirical survey, a deeper structural and physical investigation (`tmp/reverse_engineer_physics.py`) was conducted to answer two fundamental questions:
1. Can we derive a **true first-principles physics equation** (POT $\to$ Cam Angle $\theta \to$ Air Gap $d(\theta) \to$ Magnetic Flux $B(d) \to$ Kloss Eddy-Current Torque $\tau(d, \omega) \to$ Power) that outperforms empirical polynomials with fewer parameters and tight confidence intervals?
2. Can we **reverse-engineer the exact power algorithm** inside the Echelon Fit console/app?

### 7.1 Why Naive Cam-Cosine Failed ($R^2 = 0.885$) — and How True Eddy-Current Physics Achieves $R^2 = 0.99952$ with Only 4 Parameters

In `Model 5` ($R^2 = 0.8851$), cam displacement $u(\tilde{p}) = 1 - \cos(151^\circ \cdot \tilde{p})$ (where $\tilde{p} = \frac{\text{pot} - 171}{1333 - 171} \in [0, 1]$) was assumed to scale braking torque *linearly*. In reality, two physical laws govern an eccentric-cam permanent-magnet eddy-current brake:

1. **Exponential Magnetic Flux Decay Across the Air Gap ($B^2(u) = B_0^2 e^{\beta u}$):**
   - As the eccentric cam rotates from Top Dead Center ($\theta_0 = 0^\circ$ at Level 1, $\text{pot} = 171$) through $\Delta\theta = 151^\circ$ ($2.6354\text{ rad}$ at Level 32, $\text{pot} = 1333$), the normalized linear advance of the magnet slider toward the flywheel is:
     $$u(\tilde{p}) = 1 - \cos(2.6354 \cdot \tilde{p}) \in [0,\, 1.8746]$$
   - The magnetic flux density $B(d)$ in the air gap between the permanent magnet calliper and the aluminium flywheel follows an exponential Halbach/fringing decay $B(d) \propto e^{-\kappa d}$. Since eddy-current braking force scales as $B^2(d)$, the magnetic braking factor scales as $\exp(\beta \cdot u(\tilde{p}))$.
2. **Kloss / Lorentz Eddy-Current Inductive Saturation ($\tau_{\text{eddy}}(\omega)$):**
   - An aluminium flywheel spinning through a localized magnetic field acts as a continuous induction rotor with effective eddy-current path resistance $R_{\text{eddy}}$ and self-inductance $L_{\text{eddy}}$.
   - By the classical **Kloss / Lorentz electromagnetic torque equation**, eddy-current braking torque does not grow linearly with cadence forever; above a critical skin-effect / magnetic Reynolds cadence $\text{cad}_c = \frac{R_{\text{eddy}}}{2\pi L_{\text{eddy}}}$, the induced magnetic reaction field screens the applied field:
     $$\tau_{\text{eddy}}(\tilde{p}, \text{cad}) \propto \exp\!\big(\beta \, u(\tilde{p})\big) \cdot \frac{\text{cad}}{1 + (\text{cad} / \text{cad}_c)^2}$$
   - Multiplying total torque ($\tau_{\text{eddy}} + \tau_{\text{fric}}$) by angular velocity ($\propto \text{cad}$) yields the **4-Parameter First-Principles Electromechanical Power Equation (`Physics P3r`)**:

$$P(\text{pot}, \text{cad}) = A \cdot \exp\!\Big(\beta \big(1 - \cos(2.6354 \cdot \tilde{p})\big)\Big) \cdot \frac{\text{cad}^2}{1 + (\text{cad} / \text{cad}_c)^2} + c_{\text{fric}} \cdot \text{cad}$$

#### Fitted Physical Constants & 95% Confidence Intervals ($N = 317$, $k = 4$)

| Parameter | Physical Interpretation | Fitted Value | 95% Confidence Interval | Relative SE |
|---|---|---:|---:|---:|
| $A$ | Baseline eddy-current coupling at Level 1 ($u = 0$) | `0.004884 W` | $\pm 0.000131\text{ W}$ | **$1.37\%$** |
| $\beta$ | Exponential air-gap flux concentration factor ($e^{\beta u_{\max}} = 21.25\times$) | `1.630327` | $\pm 0.013016$ | **$0.41\%$** |
| $\text{cad}_c$ | Critical eddy-current skin-effect cadence ($\omega_c = R_{\text{eddy}}/L_{\text{eddy}}$) | `128.15 RPM` | $\pm 1.22\text{ RPM}$ | **$0.48\%$** |
| $c_{\text{fric}}$ | Mechanical belt/bearing Coulomb friction torque ($\sim 1.09\text{ N}\cdot\text{m}$) | `0.114260 W/RPM` | $\pm 0.013530\text{ W/RPM}$ | **$6.02\%$** |

- **Accuracy (`Physics P3r`, $k=4$):** **$R^2 = 0.999522$** ($\text{Adjusted } R^2 = 0.999518$), **$\text{RMSE} = 3.77\text{ W}$**, **$\text{MAE} = 2.71\text{ W}$**, **$\text{Max Residual} = 18.69\text{ W}$**.
- Letting $\theta_0$ and $P_0$ float (`Physics P3`, $k=6$) yields $\theta_0 = -1.01^\circ \pm 1.80^\circ$ (confirming the cam starts within $1^\circ$ of Top Dead Center at Level 1!) and $P_0 = -1.11 \pm 2.10\text{ W}$ (confirming zero power at zero cadence), with $R^2 = 0.999531$ and $\text{RMSE} = 3.73\text{ W}$.
- Compared to the 10-parameter empirical Cubic Polynomial ($R^2 = 0.997928$, $\text{RMSE} = 7.84\text{ W}$, where 4 of the 10 terms have large colinearity error bars $|t| < 4.5$), the **4-parameter first-principles Kloss model cuts RMSE and MAE in half ($3.77\text{ W}$ vs $7.84\text{ W}$) with $60\%$ fewer parameters**.

---

### 7.2 Reverse-Engineering Echelon's Proprietary Console/App Power Algorithm

Because the Echelon bike console only transmits integer **Resistance Level $L \in \{1..32\}$** and **Cadence $\text{RPM}$** over BLE (never `pot_adc`), the Watt value displayed in the Echelon Fit app is strictly a deterministic function $P(L, \text{RPM})$.

1. **Quantifying `pot_adc` Positioning Jitter vs Integer Level $L$:**
   - Across the 10 cadence sweeps at each resistance level, the bang-bang motor controller exhibits an intra-level positioning standard deviation of **$1.97\text{ ADC counts}$** (mean span $6.2\text{ ADC counts}$, vs $37.15\text{ ADC counts/level}$).
   - Regressing the 10-term cubic polynomial on integer $L$ instead of `pot_adc` reduces RMSE from $7.84\text{ W} \to 7.07\text{ W}$ ($R^2 = 0.99793 \to 0.99832$); regressing the 6-parameter Kloss model on $L$ reduces RMSE from $3.73\text{ W} \to 3.33\text{ W}$ ($R^2 = 0.99963$). Thus, **$\sim 0.4\text{–}0.8\text{ W}$ of the RMSE floor** when predicting from `pot_adc` is physical bang-bang motor hysteresis/jitter within a single console level.

2. **Singular Value Decomposition (SVD) of the $32 \times 10$ Power Matrix $P(L, \text{RPM})$:**
   - Performing SVD on the clean $32 \times 10$ matrix reveals singular values $\sigma_1 = 4146.82$, $\sigma_2 = 28.03$, $\sigma_3 = 18.46$, $\sigma_4 = 8.51$:
     - **Rank-1 Separable Product $P(L, \text{RPM}) \approx F(L) \times G(\text{RPM})$:** Captures **$99.9925\%$** of total matrix energy ($R^2 = 0.999865$, $\text{RMSE} = 2.00\text{ W}$, $\text{MAE} = 1.62\text{ W}$; $79.2\%$ of all 317 points within $\pm 2\text{ W}$).
     - **Rank-2 Sum $P(L, \text{RPM}) \approx F_1(L) G_1(\text{RPM}) + F_2(L) G_2(\text{RPM})$:** Captures **$99.9971\%$** of total matrix energy ($R^2 = 0.999947$, $\text{RMSE} = 1.26\text{ W}$, $\text{MAE} = 0.84\text{ W}$ after integer rounding; **$94.3\%$ of all points within $\pm 2\text{ W}$**).

3. **The Smoking Gun in Effective Torque ($P / \text{RPM}$): Torque Clamping at $\text{RPM} \ge 100$!**
   - Dividing each cell of the $32 \times 10$ matrix by $\text{RPM}$ exposes the effective braking torque table $\tau(L, \text{RPM}) = P(L, \text{RPM}) / \text{RPM}$:
     - **From $40\text{ RPM}$ to $100\text{ RPM}$:** $\tau(L, \text{RPM}) = P / \text{RPM}$ increases linearly with $\text{RPM}$ (e.g., at $L=32$: $3.80 \to 4.48 \to 5.10 \to 5.57 \to 6.08 \to 6.30 \to 6.57\text{ W/RPM}$).
     - **From $100\text{ RPM}$ to $130\text{ RPM}$:** $\tau(L, \text{RPM}) = P / \text{RPM}$ **is completely flat (clamped) across all 32 levels!**
     - Specifically, at $\text{RPM} = 120$, the relation $P(L, 120) = \text{round}\!\left(P(L, 100) \times \frac{120}{100}\right)$ holds with **$\text{MAE} = 0.00\text{ W}$ and $\text{Max Error} = 0\text{ W}$ ($100.0\%$ exact match across all 32 levels!)**.
   - **How Echelon's Firmware/App Computes Power:** Echelon uses a two-regime (clamped torque-speed) model where effective torque per RPM grows with cadence up to $100\text{ RPM}$ (matching the rising side of the Kloss eddy-current curve) and is **held constant above $100\text{ RPM}$** ($\text{RPM}_{\text{eff}} = \min(\text{RPM}, 100)$), so that for $\text{RPM} \ge 100$, power scales strictly linearly with cadence:
     $$P(L, \text{RPM}) = \text{RPM} \times \Big[\tau_0(L) + k(L) \cdot \min(\text{RPM}, 100)\Big]$$

### 7.3 Updated C++ Firmware Implementation: 4-Parameter First-Principles Kloss Model (`R² = 0.99952`)

```cpp
#include <math.h>
#include <stdint.h>

/**
 * @brief First-principles 4-parameter electromechanical model (Physics P3r):
 *        POT -> 151° Eccentric Cam u(p) -> Exponential Air-Gap Flux B²(u)
 *            -> Kloss Eddy-Current Braking Torque + Drivetrain Friction -> Power (W)
 *
 * Accuracy across N=317 clean points (Levels 1..32, Cadence 40..130 RPM):
 *   R^2 = 0.999522, Adjusted R^2 = 0.999518, RMSE = 3.77 W, MAE = 2.71 W
 */
uint16_t predictPowerKlossPhysics(uint16_t potAdc, uint16_t cadenceRpm) {
    if (cadenceRpm == 0) {
        return 0;
    }

    constexpr float POT_MIN   = 171.0f;
    constexpr float POT_SPAN  = 1162.0f;   // 1333 - 171
    constexpr float CAM_RAD   = 2.635447f; // 151.0 deg in radians
    constexpr float A_EDDY    = 0.004884f; // W at u=0
    constexpr float BETA_FLUX = 1.630327f; // exponential air-gap flux factor
    constexpr float CAD_CRIT  = 128.1495f; // Kloss critical skin-effect RPM
    constexpr float C_FRIC    = 0.114260f; // mechanical drivetrain drag (W/RPM)

    float pNorm = (static_cast<float>(potAdc) - POT_MIN) / POT_SPAN;
    if (pNorm < 0.0f) pNorm = 0.0f;
    if (pNorm > 1.2f) pNorm = 1.2f;

    const float cad = static_cast<float>(cadenceRpm);
    const float uCam = 1.0f - cosf(CAM_RAD * pNorm);
    const float cadRatio = cad / CAD_CRIT;
    const float klossSpeedTerm = (cad * cad) / (1.0f + cadRatio * cadRatio);

    const float watts = A_EDDY * expf(BETA_FLUX * uCam) * klossSpeedTerm + C_FRIC * cad;
    if (watts <= 0.0f) return 0;
    if (watts >= 2000.0f) return 2000;
    return static_cast<uint16_t>(watts + 0.5f);
}
```

