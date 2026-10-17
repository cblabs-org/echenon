# pr-01-signal-logger

## Ruled out, failure signatures and gotchas

### [seed-0c314309] `PsychicHttpServer` lacks `beginChunked()`. Migrated to `response->sendChunk()` and `finishChunking()` for manual chunked data transfer.
`ruled-out · kind: episodic`

### [seed-7f6150b5] Attempting to pass `response` inside the `server.on` lambda caused variable shadowing warnings; variable was renamed to `res`.
`ruled-out · kind: episodic`

### [seed-bf6f3683] ArduinoJson v7 deprecation of `StaticJsonDocument` and `containsKey()` required using `JsonDocument` and `obj["key"].is<T>()`.
`ruled-out · kind: episodic`

## Decisions

### [seed-10325693] Set `pull_up_en = GPIO_PULLUP_ENABLE` on the cadence and encoder ISR pins during testing. The physical specification mandates no internal pulls (handled by series resistors), so this should be cleaned up to `GPIO_PULLUP_DISABLE` in a subsequent patch or WO3.
`decision · kind: episodic`

### [seed-1e737217] GPIO 8/9 released from ADC scan for PCNT EncoderModule
`decision · kind: episodic`

### [seed-4a2ce965] Cadence series resistors changed from 22kΩ to 4.7kΩ + 100nF (signal too weak)
`decision · kind: episodic`

### [seed-74fc1690] Encoder wire colours corrected: Black=A, Red=B, Orange=VCC
`decision · kind: episodic`

### [seed-76541932] Reduced `kRideLogMaxRows` from 72,000 to 36,000 (30 min instead of 60 min) to fit within the ESP32-S3's 8MB PSRAM constraint.
`decision · kind: episodic`

### [seed-78e9570c] 256 internal resistance levels adopted for MITM design
`decision · kind: episodic`

### [seed-97a99977] Aligned `conv_frame_size` (1000) and `max_store_buf_size` (4000) strictly to multiples of the 40-byte multi-channel reading (10 channels * 4 bytes) to prevent ESP-IDF ADC driver panic.
`decision · kind: episodic`

### [seed-e2040e76] `stepsPerDetent=4` confirmed correct for this encoder
`decision · kind: episodic`

## Open issues

### [seed-1ab47516] Console fault tolerance must be tested before committing to MITM architecture
`open-issue · kind: semantic`

### [seed-4e532e7a] Eccentric cam produces 22× displacement variation across 32 levels — rider
`open-issue · kind: semantic`

### [seed-584c1136] Console uses bang-bang motor control with 98% overshoot at mid/high levels.
`open-issue · kind: semantic`

### [seed-9b7f99d5] None at this time.
`open-issue · kind: semantic`
