/// @file TCXGenerator.cpp
/// @brief Streaming TCX/TCX.gz generation with gzip compression, CRC32, and virtual speed integration.

#include "echenon/TCXGenerator.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>

#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#endif

namespace echenon {

namespace {

// =============================================================================
// Bounded 4 KB Scratch Buffer Writer
// =============================================================================

struct ScratchBufferWriter {
    uint8_t buf[TCXGenerator::SCRATCH_BUFFER_SIZE];
    char fmt[1024];
    size_t len = 0;
    const WriteChunkFn& sink;
    const DeadlineCheckFn& deadline;

    ScratchBufferWriter(const WriteChunkFn& s, const DeadlineCheckFn& d)
        : sink(s), deadline(d) {}

    bool checkDeadline() const {
        return !deadline || deadline();
    }

    bool flush() {
        if (len == 0) {
            return true;
        }
        if (!checkDeadline()) {
            return false;
        }
        if (!sink || !sink(buf, len)) {
            return false;
        }
        len = 0;
        return true;
    }

    bool append(const char* data, size_t n) {
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data);
        while (n > 0) {
            size_t space = TCXGenerator::SCRATCH_BUFFER_SIZE - len;
            if (space == 0) {
                if (!flush()) {
                    return false;
                }
                space = TCXGenerator::SCRATCH_BUFFER_SIZE;
            }
            const size_t to_copy = (n < space) ? n : space;
            std::memcpy(buf + len, ptr, to_copy);
            len += to_copy;
            ptr += to_copy;
            n -= to_copy;
            if (len == TCXGenerator::SCRATCH_BUFFER_SIZE) {
                if (!flush()) {
                    return false;
                }
            }
        }
        return true;
    }

    bool appendCStr(const char* s) {
        return s ? append(s, std::strlen(s)) : true;
    }
};

// =============================================================================
// Summary & Total Distance Resolution
// =============================================================================

RideSummary resolveEffectiveSummary(
    const RideSummary& input,
    const RideRecord* records,
    size_t record_count,
    float& out_total_distance_m) {
    RideSummary eff = input;
    out_total_distance_m = 0.0f;

    if (!records || record_count == 0) {
        return eff;
    }

    uint32_t sum_power   = 0;
    uint16_t max_power   = 0;
    uint32_t sum_cadence = 0;
    uint32_t sum_hr      = 0;
    uint8_t  max_hr      = 0;
    size_t   valid_hr_cnt = 0;

    for (size_t i = 0; i < record_count; ++i) {
        const RideRecord& r = records[i];
        const uint16_t eff_watts = (r.cadence_rpm == 0) ? 0u : r.power_watts;
        const float v_mps = TCXGenerator::computeTrackpointSpeedMps(r);
        out_total_distance_m += v_mps * 1.0f;

        sum_power += eff_watts;
        if (eff_watts > max_power) {
            max_power = eff_watts;
        }
        sum_cadence += r.cadence_rpm;

        if (r.isHrValid() && r.heart_rate_bpm > 0) {
            sum_hr += r.heart_rate_bpm;
            if (r.heart_rate_bpm > max_hr) {
                max_hr = r.heart_rate_bpm;
            }
            valid_hr_cnt++;
        }
    }

    if (eff.record_count == 0) {
        eff.record_count = static_cast<uint16_t>(record_count);
    }
    if (eff.active_duration_s == 0) {
        eff.active_duration_s = static_cast<uint16_t>(record_count);
    }
    if (eff.duration_s == 0) {
        const uint16_t span = static_cast<uint16_t>(records[record_count - 1].timestamp_offset_s + 1u);
        eff.duration_s = std::max<uint16_t>(static_cast<uint16_t>(record_count), span);
    }
    if (eff.avg_power_w == 0 && sum_power > 0) {
        eff.avg_power_w = static_cast<uint16_t>((sum_power + (record_count / 2u)) / record_count);
    }
    if (eff.max_power_w == 0 && max_power > 0) {
        eff.max_power_w = max_power;
    }
    if (eff.avg_cadence_rpm == 0 && sum_cadence > 0) {
        eff.avg_cadence_rpm = static_cast<uint8_t>((sum_cadence + (record_count / 2u)) / record_count);
    }
    if (eff.avg_hr_bpm == 0 && valid_hr_cnt > 0) {
        eff.avg_hr_bpm = static_cast<uint8_t>((sum_hr + (valid_hr_cnt / 2u)) / valid_hr_cnt);
    }
    if (eff.max_hr_bpm == 0 && max_hr > 0) {
        eff.max_hr_bpm = max_hr;
    }
    if (eff.total_work_kj == 0 && sum_power > 0) {
        eff.total_work_kj = static_cast<uint16_t>((sum_power + 500u) / 1000u);
    }

    return eff;
}

// =============================================================================
// RFC 1951 Fixed-Huffman Tables & Streaming Gzip Compressor / Decompressor
// =============================================================================

constexpr uint16_t kLenBase[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};

constexpr uint8_t kLenExtra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};

constexpr uint16_t kDistBase[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};

constexpr uint8_t kDistExtra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

class StreamingGzipEncoder {
public:
    StreamingGzipEncoder(const WriteChunkFn& sink, const DeadlineCheckFn& deadline)
        : _sink(sink), _deadline(deadline) {
        std::memset(_hashHead, 0xFF, sizeof(_hashHead));
    }

    bool writeHeader() {
        // RFC 1952 10-byte gzip header: ID1=0x1f, ID2=0x8b, CM=8 (deflate), FLG=0, MTIME=0, XFL=0, OS=3 (Unix)
        static constexpr uint8_t kGzipHeader[10] = {
            0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03
        };
        return emitBytes(kGzipHeader, sizeof(kGzipHeader));
    }

    bool compressChunk(const uint8_t* data, size_t len) {
        if (len == 0) {
            return true;
        }
        _crc32 = TCXGenerator::computeCrc32(data, len, _crc32);
        _isize += static_cast<uint32_t>(len);

        if (!_blockOpen) {
            // Start non-final Fixed Huffman block: BFINAL=0, BTYPE=01 (binary 010 LSB-first = 2)
            if (!putBitsLsb(0x02u, 3)) {
                return false;
            }
            _blockOpen = true;
        }

        // Compress data using LZ77 + RFC 1951 Fixed Huffman within the chunk
        std::memset(_hashHead, 0xFF, sizeof(_hashHead));
        size_t pos = 0;
        while (pos < len) {
            size_t best_len = 0;
            size_t best_dist = 0;

            if (pos + 3 <= len) {
                const uint32_t h = hash3(data + pos);
                const int16_t prev = _hashHead[h];
                _hashHead[h] = static_cast<int16_t>(pos);

                if (prev >= 0 && pos > static_cast<size_t>(prev)) {
                    const size_t dist = pos - static_cast<size_t>(prev);
                    if (dist <= 32768u) {
                        const size_t max_len = std::min<size_t>(258u, len - pos);
                        size_t mlen = 0;
                        while (mlen < max_len && data[prev + mlen] == data[pos + mlen]) {
                            ++mlen;
                        }
                        if (mlen >= 3) {
                            best_len = mlen;
                            best_dist = dist;
                        }
                    }
                }
            }

            if (best_len >= 3) {
                if (!emitLengthDistance(static_cast<uint16_t>(best_len),
                                        static_cast<uint16_t>(best_dist))) {
                    return false;
                }
                for (size_t k = 1; k < best_len; ++k) {
                    if (pos + k + 3 <= len) {
                        _hashHead[hash3(data + pos + k)] = static_cast<int16_t>(pos + k);
                    }
                }
                pos += best_len;
            } else {
                if (!emitLiteral(data[pos])) {
                    return false;
                }
                ++pos;
            }
        }

        return true;
    }

    bool finish() {
        if (_blockOpen) {
            // Close active non-final Fixed Huffman block with EOB (256)
            if (!emitFixedSymbol(256)) {
                return false;
            }
            _blockOpen = false;
        }

        // Emit final empty Fixed Huffman block: BFINAL=1, BTYPE=01 (binary 011 LSB-first = 3) + EOB (256)
        if (!putBitsLsb(0x03u, 3)) {
            return false;
        }
        if (!emitFixedSymbol(256)) {
            return false;
        }

        // Flush remaining partial bits to byte boundary
        if (_bitCount > 0) {
            if (!emitByte(static_cast<uint8_t>(_bitBuf & 0xFFu))) {
                return false;
            }
            _bitBuf = 0;
            _bitCount = 0;
        }

        // Write RFC 1952 8-byte trailer: 4-byte little-endian CRC32 + 4-byte little-endian ISIZE
        uint8_t trailer[8];
        trailer[0] = static_cast<uint8_t>(_crc32 & 0xFFu);
        trailer[1] = static_cast<uint8_t>((_crc32 >> 8) & 0xFFu);
        trailer[2] = static_cast<uint8_t>((_crc32 >> 16) & 0xFFu);
        trailer[3] = static_cast<uint8_t>((_crc32 >> 24) & 0xFFu);
        trailer[4] = static_cast<uint8_t>(_isize & 0xFFu);
        trailer[5] = static_cast<uint8_t>((_isize >> 8) & 0xFFu);
        trailer[6] = static_cast<uint8_t>((_isize >> 16) & 0xFFu);
        trailer[7] = static_cast<uint8_t>((_isize >> 24) & 0xFFu);

        if (!emitBytes(trailer, sizeof(trailer))) {
            return false;
        }
        return flushOutBuf();
    }

private:
    static constexpr size_t HASH_SIZE = 512;

    static uint32_t hash3(const uint8_t* p) {
        uint32_t v = (static_cast<uint32_t>(p[0]) << 16) |
                     (static_cast<uint32_t>(p[1]) << 8) |
                     static_cast<uint32_t>(p[2]);
        return ((v * 2654435761u) >> 23) & (HASH_SIZE - 1);
    }

    bool flushOutBuf() {
        if (_outLen == 0) {
            return true;
        }
        if (_deadline && !_deadline()) {
            return false;
        }
        if (!_sink || !_sink(_outBuf, _outLen)) {
            return false;
        }
        _outLen = 0;
        return true;
    }

    bool emitByte(uint8_t b) {
        _outBuf[_outLen++] = b;
        if (_outLen == TCXGenerator::SCRATCH_BUFFER_SIZE) {
            return flushOutBuf();
        }
        return true;
    }

    bool emitBytes(const uint8_t* p, size_t n) {
        for (size_t i = 0; i < n; ++i) {
            if (!emitByte(p[i])) {
                return false;
            }
        }
        return true;
    }

    bool putBitsLsb(uint32_t bits, uint8_t count) {
        _bitBuf |= (bits << _bitCount);
        _bitCount += count;
        while (_bitCount >= 8) {
            if (!emitByte(static_cast<uint8_t>(_bitBuf & 0xFFu))) {
                return false;
            }
            _bitBuf >>= 8;
            _bitCount -= 8;
        }
        return true;
    }

    // Huffman codes in RFC 1951 are packed MSB-first into the LSB-first bitstream
    bool putHuffmanCodeMsb(uint16_t code, uint8_t bits) {
        uint32_t rev = 0;
        for (uint8_t i = 0; i < bits; ++i) {
            rev = (rev << 1) | ((code >> i) & 1u);
        }
        return putBitsLsb(rev, bits);
    }

    bool emitFixedSymbol(uint16_t sym) {
        if (sym <= 143) {
            return putHuffmanCodeMsb(static_cast<uint16_t>(0x30u + sym), 8);
        } else if (sym <= 255) {
            return putHuffmanCodeMsb(static_cast<uint16_t>(0x190u + (sym - 144u)), 9);
        } else if (sym <= 279) {
            return putHuffmanCodeMsb(static_cast<uint16_t>(sym - 256u), 7);
        } else {
            return putHuffmanCodeMsb(static_cast<uint16_t>(0xC0u + (sym - 280u)), 8);
        }
    }

    bool emitLiteral(uint8_t lit) {
        return emitFixedSymbol(lit);
    }

    bool emitLengthDistance(uint16_t length, uint16_t dist) {
        uint8_t len_idx = 0;
        for (uint8_t i = 0; i < 29; ++i) {
            const uint16_t base = kLenBase[i];
            const uint16_t max_v = static_cast<uint16_t>(base + (1u << kLenExtra[i]) - 1u);
            if (length >= base && length <= max_v) {
                len_idx = i;
                break;
            }
        }
        if (!emitFixedSymbol(static_cast<uint16_t>(257u + len_idx))) {
            return false;
        }
        if (kLenExtra[len_idx] > 0) {
            if (!putBitsLsb(length - kLenBase[len_idx], kLenExtra[len_idx])) {
                return false;
            }
        }

        uint8_t dist_idx = 0;
        for (uint8_t i = 0; i < 30; ++i) {
            const uint16_t base = kDistBase[i];
            const uint16_t max_v = static_cast<uint16_t>(base + (1u << kDistExtra[i]) - 1u);
            if (dist >= base && dist <= max_v) {
                dist_idx = i;
                break;
            }
        }
        if (!putHuffmanCodeMsb(dist_idx, 5)) {
            return false;
        }
        if (kDistExtra[dist_idx] > 0) {
            if (!putBitsLsb(dist - kDistBase[dist_idx], kDistExtra[dist_idx])) {
                return false;
            }
        }
        return true;
    }

    const WriteChunkFn& _sink;
    const DeadlineCheckFn& _deadline;
    uint8_t _outBuf[TCXGenerator::SCRATCH_BUFFER_SIZE];
    size_t _outLen = 0;
    uint32_t _bitBuf = 0;
    uint8_t _bitCount = 0;
    uint32_t _crc32 = 0;
    uint32_t _isize = 0;
    bool _blockOpen = false;
    int16_t _hashHead[HASH_SIZE];
};

struct BitReader {
    const uint8_t* data;
    size_t size;
    size_t byte_pos = 0;
    uint32_t bit_buf = 0;
    uint8_t bit_count = 0;

    BitReader(const uint8_t* d, size_t s) : data(d), size(s) {}

    bool ensureBits(uint8_t n) {
        while (bit_count < n) {
            if (byte_pos >= size) {
                return false;
            }
            bit_buf |= (static_cast<uint32_t>(data[byte_pos++]) << bit_count);
            bit_count += 8;
        }
        return true;
    }

    bool readBitsLsb(uint8_t n, uint32_t& out) {
        if (n == 0) {
            out = 0;
            return true;
        }
        if (!ensureBits(n)) {
            return false;
        }
        out = bit_buf & ((1u << n) - 1u);
        bit_buf >>= n;
        bit_count -= n;
        return true;
    }

    bool readBitsMsb(uint8_t n, uint32_t& out) {
        out = 0;
        for (uint8_t i = 0; i < n; ++i) {
            uint32_t bit = 0;
            if (!readBitsLsb(1, bit)) {
                return false;
            }
            out = (out << 1) | bit;
        }
        return true;
    }

    void alignToByte() {
        bit_buf = 0;
        bit_count = 0;
    }

    bool readFixedSymbol(uint16_t& sym) {
        uint32_t code7 = 0;
        if (!readBitsMsb(7, code7)) {
            return false;
        }
        // 7-bit codes: 0000000..0010111 -> 256..279
        if (code7 <= 0x17u) {
            sym = static_cast<uint16_t>(256u + code7);
            return true;
        }

        uint32_t bit8 = 0;
        if (!readBitsLsb(1, bit8)) {
            return false;
        }
        const uint32_t code8 = (code7 << 1) | bit8;
        // 8-bit codes: 00110000..10111111 (0x30..0xBF) -> 0..143
        if (code8 >= 0x30u && code8 <= 0xBFu) {
            sym = static_cast<uint16_t>(code8 - 0x30u);
            return true;
        }
        // 8-bit codes: 11000000..11000111 (0xC0..0xC7) -> 280..287
        if (code8 >= 0xC0u && code8 <= 0xC7u) {
            sym = static_cast<uint16_t>(280u + (code8 - 0xC0u));
            return true;
        }

        uint32_t bit9 = 0;
        if (!readBitsLsb(1, bit9)) {
            return false;
        }
        const uint32_t code9 = (code8 << 1) | bit9;
        // 9-bit codes: 110010000..111111111 (0x190..0x1FF) -> 144..255
        if (code9 >= 0x190u && code9 <= 0x1FFu) {
            sym = static_cast<uint16_t>(144u + (code9 - 0x190u));
            return true;
        }
        return false;
    }
};

} // namespace

// =============================================================================
// Public Methods
// =============================================================================

void TCXGenerator::formatIso8601Utc(uint32_t epoch_s, char* out_buf, size_t out_len) {
    if (!out_buf || out_len == 0) {
        return;
    }

    const uint32_t days   = epoch_s / 86400u;
    const uint32_t rem    = epoch_s % 86400u;
    const unsigned hour   = rem / 3600u;
    const unsigned minute = (rem % 3600u) / 60u;
    const unsigned second = rem % 60u;

    // Howard Hinnant civil-from-days algorithm (UTC proleptic Gregorian)
    const int64_t  z   = static_cast<int64_t>(days) + 719468;
    const int64_t  era = (z >= 0 ? z : z - 146096) / 146097;
    const uint32_t doe = static_cast<uint32_t>(z - era * 146097);
    const uint32_t yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
    int64_t        y   = static_cast<int64_t>(yoe) + era * 400;
    const uint32_t doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
    const uint32_t mp  = (5u * doy + 2u) / 153u;
    const unsigned d   = static_cast<unsigned>(doy - (153u * mp + 2u) / 5u + 1u);
    const unsigned m   = static_cast<unsigned>(mp < 10u ? mp + 3u : mp - 9u);
    y += (m <= 2u) ? 1 : 0;

    std::snprintf(out_buf, out_len, "%04d-%02u-%02uT%02u:%02u:%02uZ",
                  static_cast<int>(y), m, d, hour, minute, second);
}

std::string TCXGenerator::formatIso8601Utc(uint32_t epoch_s) {
    char buf[32];
    formatIso8601Utc(epoch_s, buf, sizeof(buf));
    return std::string(buf);
}

float TCXGenerator::computeTrackpointSpeedMps(const RideRecord& record) {
    if (record.cadence_rpm == 0 || record.power_watts == 0) {
        return 0.0f;
    }
    const float v_kph = PowerEngine::computeVirtualSpeedKph(static_cast<float>(record.power_watts));
    if (!std::isfinite(v_kph) || v_kph <= 0.0f) {
        return 0.0f;
    }
    return v_kph / 3.6f;
}

uint32_t TCXGenerator::computeCrc32(const uint8_t* data, size_t len, uint32_t initial_crc) {
    uint32_t crc = ~initial_crc;
    if (data) {
        for (size_t i = 0; i < len; ++i) {
            crc ^= data[i];
            for (int bit = 0; bit < 8; ++bit) {
                crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
            }
        }
    }
    return ~crc;
}

bool TCXGenerator::generateTcx(
    const RideSummary& summary,
    const RideRecord* records,
    size_t record_count,
    const WriteChunkFn& write_chunk,
    const DeadlineCheckFn& check_deadline) {
    if (!write_chunk) {
        return false;
    }
    if (record_count > 0 && !records) {
        return false;
    }
    if (check_deadline && !check_deadline()) {
        return false;
    }

    float total_distance_m = 0.0f;
    const RideSummary eff = resolveEffectiveSummary(
        summary, records, record_count, total_distance_m);

#if defined(ESP_PLATFORM)
    void* mem = heap_caps_malloc(sizeof(ScratchBufferWriter), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!mem) return false;
    auto writer = std::unique_ptr<ScratchBufferWriter, void(*)(ScratchBufferWriter*)>(
        new(mem) ScratchBufferWriter(write_chunk, check_deadline),
        [](ScratchBufferWriter* p) { p->~ScratchBufferWriter(); free(p); }
    );
#else
    auto writer = std::unique_ptr<ScratchBufferWriter>(
        new(std::nothrow) ScratchBufferWriter(write_chunk, check_deadline)
    );
    if (!writer) return false;
#endif

    char start_iso[32];
    formatIso8601Utc(eff.start_epoch_s, start_iso, sizeof(start_iso));

    // Format XML header + <Lap> summary metadata directly into PSRAM writer->fmt
    int n = std::snprintf(
        writer->fmt,
        sizeof(writer->fmt),
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<TrainingCenterDatabase"
        " xmlns=\"http://www.garmin.com/xmlschemas/TrainingCenterDatabase/v2\""
        " xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
        " xmlns:ns3=\"http://www.garmin.com/xmlschemas/ActivityExtension/v2\">\n"
        "  <Activities>\n"
        "    <Activity Sport=\"Biking\">\n"
        "      <Id>%s</Id>\n"
        "      <Lap StartTime=\"%s\">\n"
        "        <TotalTimeSeconds>%u.00</TotalTimeSeconds>\n"
        "        <DistanceMeters>%.2f</DistanceMeters>\n"
        "        <Calories>%u</Calories>\n",
        start_iso,
        start_iso,
        static_cast<unsigned>(eff.duration_s),
        static_cast<double>(total_distance_m),
        static_cast<unsigned>(eff.total_work_kj));
    if (n <= 0 || static_cast<size_t>(n) >= sizeof(writer->fmt) ||
        !writer->append(writer->fmt, static_cast<size_t>(n))) {
        return false;
    }

    if (eff.avg_hr_bpm > 0 && eff.max_hr_bpm > 0) {
        int hr_n = std::snprintf(
            writer->fmt,
            sizeof(writer->fmt),
            "        <AverageHeartRateBpm><Value>%u</Value></AverageHeartRateBpm>\n"
            "        <MaximumHeartRateBpm><Value>%u</Value></MaximumHeartRateBpm>\n",
            static_cast<unsigned>(eff.avg_hr_bpm),
            static_cast<unsigned>(eff.max_hr_bpm));
        if (hr_n <= 0 || static_cast<size_t>(hr_n) >= sizeof(writer->fmt) ||
            !writer->append(writer->fmt, static_cast<size_t>(hr_n))) {
            return false;
        }
    }

    int mid_n = std::snprintf(
        writer->fmt,
        sizeof(writer->fmt),
        "        <Intensity>Active</Intensity>\n"
        "        <Cadence>%u</Cadence>\n"
        "        <TriggerMethod>Manual</TriggerMethod>\n"
        "        <Track>\n",
        static_cast<unsigned>(eff.avg_cadence_rpm));
    if (mid_n <= 0 || static_cast<size_t>(mid_n) >= sizeof(writer->fmt) ||
        !writer->append(writer->fmt, static_cast<size_t>(mid_n))) {
        return false;
    }

    // Stream 1 Hz <Trackpoint> elements
    float cumulative_distance_m = 0.0f;
    char tp_iso[32];

    for (size_t i = 0; i < record_count; ++i) {
        if ((i % DEADLINE_CHECK_INTERVAL_RECORDS) == 0 &&
            check_deadline && !check_deadline()) {
            return false;
        }

        const RideRecord& rec = records[i];
        const uint32_t tp_epoch_s = eff.start_epoch_s + static_cast<uint32_t>(rec.timestamp_offset_s);
        formatIso8601Utc(tp_epoch_s, tp_iso, sizeof(tp_iso));

        // Coasting continuity rule (SPEC.md §9.4): when cadence == 0, emit 0 RPM, 0.00 m/s, 0 W
        const uint8_t  eff_cadence = rec.cadence_rpm;
        const uint16_t eff_watts   = (eff_cadence == 0) ? 0u : rec.power_watts;
        const float    v_mps       = computeTrackpointSpeedMps(rec);
        cumulative_distance_m += v_mps * 1.0f;

        const bool emit_hr = rec.isHrValid() && (rec.heart_rate_bpm > 0);
        int tp_n = 0;
        if (emit_hr) {
            tp_n = std::snprintf(
                writer->fmt,
                sizeof(writer->fmt),
                "          <Trackpoint>\n"
                "            <Time>%s</Time>\n"
                "            <DistanceMeters>%.2f</DistanceMeters>\n"
                "            <HeartRateBpm><Value>%u</Value></HeartRateBpm>\n"
                "            <Cadence>%u</Cadence>\n"
                "            <Extensions>\n"
                "              <ns3:TPX>\n"
                "                <ns3:Speed>%.2f</ns3:Speed>\n"
                "                <ns3:Watts>%u</ns3:Watts>\n"
                "              </ns3:TPX>\n"
                "            </Extensions>\n"
                "          </Trackpoint>\n",
                tp_iso,
                static_cast<double>(cumulative_distance_m),
                static_cast<unsigned>(rec.heart_rate_bpm),
                static_cast<unsigned>(eff_cadence),
                static_cast<double>(v_mps),
                static_cast<unsigned>(eff_watts));
        } else {
            tp_n = std::snprintf(
                writer->fmt,
                sizeof(writer->fmt),
                "          <Trackpoint>\n"
                "            <Time>%s</Time>\n"
                "            <DistanceMeters>%.2f</DistanceMeters>\n"
                "            <Cadence>%u</Cadence>\n"
                "            <Extensions>\n"
                "              <ns3:TPX>\n"
                "                <ns3:Speed>%.2f</ns3:Speed>\n"
                "                <ns3:Watts>%u</ns3:Watts>\n"
                "              </ns3:TPX>\n"
                "            </Extensions>\n"
                "          </Trackpoint>\n",
                tp_iso,
                static_cast<double>(cumulative_distance_m),
                static_cast<unsigned>(eff_cadence),
                static_cast<double>(v_mps),
                static_cast<unsigned>(eff_watts));
        }

        if (tp_n <= 0 || static_cast<size_t>(tp_n) >= sizeof(writer->fmt) ||
            !writer->append(writer->fmt, static_cast<size_t>(tp_n))) {
            return false;
        }
    }

    // Format </Track>, <Extensions><ns3:LX>, <Creator>, and <Author> footer
    int foot_n = std::snprintf(
        writer->fmt,
        sizeof(writer->fmt),
        "        </Track>\n"
        "        <Extensions>\n"
        "          <ns3:LX>\n"
        "            <ns3:AvgWatts>%u</ns3:AvgWatts>\n"
        "            <ns3:MaxWatts>%u</ns3:MaxWatts>\n"
        "          </ns3:LX>\n"
        "        </Extensions>\n"
        "      </Lap>\n"
        "      <Creator xsi:type=\"Device_t\">\n"
        "        <Name>Echenon EX-3</Name>\n"
        "        <UnitId>0</UnitId>\n"
        "        <ProductID>0</ProductID>\n"
        "        <Version>\n"
        "          <VersionMajor>1</VersionMajor>\n"
        "          <VersionMinor>0</VersionMinor>\n"
        "          <BuildMajor>0</BuildMajor>\n"
        "          <BuildMinor>0</BuildMinor>\n"
        "        </Version>\n"
        "      </Creator>\n"
        "    </Activity>\n"
        "  </Activities>\n"
        "  <Author xsi:type=\"Application_t\">\n"
        "    <Name>Echenon Firmware</Name>\n"
        "    <Build>\n"
        "      <Version>\n"
        "        <VersionMajor>1</VersionMajor>\n"
        "        <VersionMinor>0</VersionMinor>\n"
        "        <BuildMajor>0</BuildMajor>\n"
        "        <BuildMinor>0</BuildMinor>\n"
        "      </Version>\n"
        "    </Build>\n"
        "    <LangID>en</LangID>\n"
        "    <PartNumber>000-00000-00</PartNumber>\n"
        "  </Author>\n"
        "</TrainingCenterDatabase>\n",
        static_cast<unsigned>(eff.avg_power_w),
        static_cast<unsigned>(eff.max_power_w));
    if (foot_n <= 0 || static_cast<size_t>(foot_n) >= sizeof(writer->fmt) ||
        !writer->append(writer->fmt, static_cast<size_t>(foot_n))) {
        return false;
    }

    return writer->flush();
}

bool TCXGenerator::generateTcxFromStore(
    const RideStore& store,
    uint32_t session_id,
    const WriteChunkFn& write_chunk,
    const DeadlineCheckFn& check_deadline) {
    if (store.isSessionActive() && store.getActiveSessionId() == session_id) {
        return generateTcx(
            store.getActiveSummary(),
            store.getRecords(),
            store.getRecordCount(),
            write_chunk,
            check_deadline);
    }

    RideSummary summary{};
    std::vector<RideRecord> records;
    if (!store.loadStoredRide(session_id, summary, records)) {
        return false;
    }
    return generateTcx(summary, records, write_chunk, check_deadline);
}

bool TCXGenerator::generateTcxGz(
    const RideSummary& summary,
    const RideRecord* records,
    size_t record_count,
    const WriteChunkFn& write_chunk,
    const DeadlineCheckFn& check_deadline) {
    if (!write_chunk) {
        return false;
    }
    if (record_count > 0 && !records) {
        return false;
    }
    if (check_deadline && !check_deadline()) {
        return false;
    }

    // Heap-allocate the encoder (~5 KB) to free main-task stack headroom.
    // On ESP32, place in PSRAM via heap_caps_malloc; on native, use regular new.
#ifdef ESP_PLATFORM
    void* mem = heap_caps_malloc(sizeof(StreamingGzipEncoder), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!mem) return false;
    auto gz = std::unique_ptr<StreamingGzipEncoder, void(*)(StreamingGzipEncoder*)>(
        new(mem) StreamingGzipEncoder(write_chunk, check_deadline),
        [](StreamingGzipEncoder* p) { p->~StreamingGzipEncoder(); free(p); }
    );
#else
    auto gz = std::unique_ptr<StreamingGzipEncoder>(new(std::nothrow) StreamingGzipEncoder(write_chunk, check_deadline));
    if (!gz) return false;
#endif
    if (!gz->writeHeader()) {
        return false;
    }

    const WriteChunkFn xml_to_gz = [&gz](const uint8_t* chunk, size_t len) -> bool {
        return gz->compressChunk(chunk, len);
    };

    if (!generateTcx(summary, records, record_count, xml_to_gz, check_deadline)) {
        return false;
    }

    return gz->finish();
}

bool TCXGenerator::generateTcxGzFromStore(
    const RideStore& store,
    uint32_t session_id,
    const WriteChunkFn& write_chunk,
    const DeadlineCheckFn& check_deadline) {
    if (store.isSessionActive() && store.getActiveSessionId() == session_id) {
        return generateTcxGz(
            store.getActiveSummary(),
            store.getRecords(),
            store.getRecordCount(),
            write_chunk,
            check_deadline);
    }

    RideSummary summary{};
    std::vector<RideRecord> records;
    if (!store.loadStoredRide(session_id, summary, records)) {
        return false;
    }
    return generateTcxGz(summary, records, write_chunk, check_deadline);
}

bool TCXGenerator::generateTcxGzToFile(
    const RideSummary& summary,
    const RideRecord* records,
    size_t record_count,
    IRideFileSystem& fs,
    const std::string& out_path,
    const DeadlineCheckFn& check_deadline) {
    if (out_path.empty()) {
        return false;
    }

    fs.mkdir(RideStore::RIDES_DIR);
    if (fs.exists(out_path.c_str())) {
        fs.remove(out_path.c_str());
    }

    bool first_chunk = true;
    const WriteChunkFn file_sink = [&fs, &out_path, &first_chunk](const uint8_t* data, size_t len) -> bool {
        if (first_chunk) {
            first_chunk = false;
            return fs.writeFile(out_path.c_str(), data, len);
        }
        return fs.appendFile(out_path.c_str(), data, len);
    };

    if (!generateTcxGz(summary, records, record_count, file_sink, check_deadline)) {
        if (fs.exists(out_path.c_str())) {
            fs.remove(out_path.c_str());
        }
        return false;
    }
    return true;
}

bool TCXGenerator::generateTcxGzToFile(
    RideStore& store,
    uint32_t session_id,
    const std::string& out_path,
    const DeadlineCheckFn& check_deadline) {
    const std::string target_path = out_path.empty()
        ? RideStore::getRideTcxGzPath(session_id)
        : out_path;

    if (store.isSessionActive() && store.getActiveSessionId() == session_id) {
        return generateTcxGzToFile(
            store.getActiveSummary(),
            store.getRecords(),
            store.getRecordCount(),
            store.fileSystem(),
            target_path,
            check_deadline);
    }

    RideSummary summary{};
    std::vector<RideRecord> records;
    if (!store.loadStoredRide(session_id, summary, records)) {
        return false;
    }

    return generateTcxGzToFile(
        summary,
        records.empty() ? nullptr : records.data(),
        records.size(),
        store.fileSystem(),
        target_path,
        check_deadline);
}

bool TCXGenerator::compressGzip(
    const uint8_t* data,
    size_t len,
    std::vector<uint8_t>& out_compressed,
    const DeadlineCheckFn& check_deadline) {
    out_compressed.clear();
    if (!data && len > 0) {
        return false;
    }

    const WriteChunkFn chunk_sink = [&out_compressed](const uint8_t* chunk, size_t clen) -> bool {
        if (chunk && clen > 0) {
            out_compressed.insert(out_compressed.end(), chunk, chunk + clen);
        }
        return true;
    };

#ifdef ESP_PLATFORM
    void* mem = heap_caps_malloc(sizeof(StreamingGzipEncoder), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!mem) return false;
    auto gz = std::unique_ptr<StreamingGzipEncoder, void(*)(StreamingGzipEncoder*)>(
        new(mem) StreamingGzipEncoder(chunk_sink, check_deadline),
        [](StreamingGzipEncoder* p) { p->~StreamingGzipEncoder(); free(p); }
    );
#else
    auto gz = std::unique_ptr<StreamingGzipEncoder>(new(std::nothrow) StreamingGzipEncoder(chunk_sink, check_deadline));
    if (!gz) return false;
#endif

    if (!gz->writeHeader()) {
        return false;
    }
    if (len > 0 && !gz->compressChunk(data, len)) {
        return false;
    }
    return gz->finish();
}

bool TCXGenerator::decompressGzip(
    const uint8_t* gz_data,
    size_t gz_len,
    std::vector<uint8_t>& out_uncompressed) {
    out_uncompressed.clear();
    // Minimum RFC 1952 gzip size: 10-byte header + at least 2 bytes deflate + 8-byte trailer
    if (!gz_data || gz_len < 18) {
        return false;
    }

    // Validate RFC 1952 header magic & compression method (8 = deflate)
    if (gz_data[0] != 0x1f || gz_data[1] != 0x8b || gz_data[2] != 0x08) {
        return false;
    }

    // Pre-reserve output vector if uncompressed size in trailer looks reasonable
    const uint8_t* trailer = gz_data + (gz_len - 8);
    const uint32_t expected_isize =
        static_cast<uint32_t>(trailer[4]) |
        (static_cast<uint32_t>(trailer[5]) << 8) |
        (static_cast<uint32_t>(trailer[6]) << 16) |
        (static_cast<uint32_t>(trailer[7]) << 24);
    if (expected_isize > 0 && expected_isize <= 10u * 1024u * 1024u) {
        out_uncompressed.reserve(expected_isize);
    }

    const uint8_t flg = gz_data[3];
    size_t pos = 10;

    // Skip optional RFC 1952 header fields if present
    if (flg & 0x04u) { // FEXTRA
        if (pos + 2 > gz_len - 8) return false;
        const size_t xlen = static_cast<size_t>(gz_data[pos]) |
                            (static_cast<size_t>(gz_data[pos + 1]) << 8);
        pos += 2 + xlen;
    }
    if (flg & 0x08u) { // FNAME
        while (pos < gz_len - 8 && gz_data[pos] != 0) ++pos;
        ++pos;
    }
    if (flg & 0x10u) { // FCOMMENT
        while (pos < gz_len - 8 && gz_data[pos] != 0) ++pos;
        ++pos;
    }
    if (flg & 0x02u) { // FHCRC
        pos += 2;
    }
    if (pos > gz_len - 8) {
        return false;
    }

    const size_t deflate_len = (gz_len - 8) - pos;
    BitReader reader(gz_data + pos, deflate_len);

    bool bfinal = false;
    while (!bfinal) {
        uint32_t final_bit = 0;
        uint32_t btype = 0;
        if (!reader.readBitsLsb(1, final_bit) || !reader.readBitsLsb(2, btype)) {
            return false;
        }
        bfinal = (final_bit != 0);

        if (btype == 0) {
            // Stored block (BTYPE=00)
            reader.alignToByte();
            uint32_t block_len = 0;
            uint32_t block_nlen = 0;
            if (!reader.readBitsLsb(16, block_len) || !reader.readBitsLsb(16, block_nlen)) {
                return false;
            }
            if ((block_len ^ 0xFFFFu) != block_nlen) {
                return false;
            }
            for (uint32_t i = 0; i < block_len; ++i) {
                uint32_t b = 0;
                if (!reader.readBitsLsb(8, b)) {
                    return false;
                }
                out_uncompressed.push_back(static_cast<uint8_t>(b));
            }
        } else if (btype == 1) {
            // Fixed Huffman block (BTYPE=01)
            while (true) {
                uint16_t sym = 0;
                if (!reader.readFixedSymbol(sym)) {
                    return false;
                }
                if (sym < 256) {
                    out_uncompressed.push_back(static_cast<uint8_t>(sym));
                } else if (sym == 256) {
                    break; // End of block
                } else if (sym <= 285) {
                    const uint8_t len_idx = static_cast<uint8_t>(sym - 257u);
                    uint32_t extra_len = 0;
                    if (!reader.readBitsLsb(kLenExtra[len_idx], extra_len)) {
                        return false;
                    }
                    const size_t match_len = static_cast<size_t>(kLenBase[len_idx]) + extra_len;

                    uint32_t dist_code = 0;
                    if (!reader.readBitsMsb(5, dist_code) || dist_code >= 30) {
                        return false;
                    }
                    uint32_t extra_dist = 0;
                    if (!reader.readBitsLsb(kDistExtra[dist_code], extra_dist)) {
                        return false;
                    }
                    const size_t match_dist = static_cast<size_t>(kDistBase[dist_code]) + extra_dist;
                    if (match_dist == 0 || match_dist > out_uncompressed.size()) {
                        return false;
                    }

                    for (size_t k = 0; k < match_len; ++k) {
                        const uint8_t b = out_uncompressed[out_uncompressed.size() - match_dist];
                        out_uncompressed.push_back(b);
                    }
                } else {
                    return false;
                }
            }
        } else {
            return false;
        }
    }

    // Verify RFC 1952 trailer: 4-byte little-endian CRC32 and 4-byte little-endian ISIZE
    const uint32_t expected_crc =
        static_cast<uint32_t>(trailer[0]) |
        (static_cast<uint32_t>(trailer[1]) << 8) |
        (static_cast<uint32_t>(trailer[2]) << 16) |
        (static_cast<uint32_t>(trailer[3]) << 24);

    const uint32_t actual_crc = computeCrc32(
        out_uncompressed.empty() ? nullptr : out_uncompressed.data(),
        out_uncompressed.size());
    const uint32_t actual_isize = static_cast<uint32_t>(out_uncompressed.size() & 0xFFFFFFFFu);

    return (actual_crc == expected_crc) && (actual_isize == expected_isize);
}

} // namespace echenon
