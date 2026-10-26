#pragma once

/// @file TCXGenerator.h
/// @brief Streaming 1 Hz Training Center Database v2 (.tcx and .tcx.gz) generator
///        with bounded 4 KB scratch buffer, first-principles virtual speed/distance
///        integration, Xert MPA coasting continuity, and deadline checking
///        (SPEC.md §7.5, §9.4–§9.5).

#include "echenon/PowerEngine.h"
#include "echenon/RideStore.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace echenon {

/// Callback invoked to emit a chunk of output bytes (uncompressed XML or .tcx.gz).
/// Return true to continue streaming, or false to abort generation immediately.
using WriteChunkFn = std::function<bool(const uint8_t* data, size_t len)>;

/// Callback invoked periodically to check whether the caller's execution budget
/// (e.g. DataManager 15 s DeadlineToken) still has time remaining.
/// Return true while time remains, or false when the deadline has expired.
/// An empty/null DeadlineCheckFn is treated as never expired.
using DeadlineCheckFn = std::function<bool()>;

class TCXGenerator {
public:
    /// Maximum size of the formatting and compression scratch buffer (4 KB).
    static constexpr size_t SCRATCH_BUFFER_SIZE = 4096;

    /// Number of 1 Hz records between periodic deadline checks.
    static constexpr uint16_t DEADLINE_CHECK_INTERVAL_RECORDS = 32;

    /// Format a Unix epoch timestamp (seconds since 1970-01-01T00:00:00Z) as
    /// ISO-8601 UTC ("YYYY-MM-DDTHH:MM:SSZ").
    static void formatIso8601Utc(uint32_t epoch_s, char* out_buf, size_t out_len);
    static std::string formatIso8601Utc(uint32_t epoch_s);

    /// Compute virtual flat-road speed (m/s) for a 1 Hz RideRecord using
    /// PowerEngine::computeVirtualSpeedKph(P) / 3.6f (0.245 v^3 + 3.53 v = P).
    /// Enforces 0.00 m/s during active coasting (cadence_rpm == 0).
    static float computeTrackpointSpeedMps(const RideRecord& record);

    /// Compute standard IEEE 802.3 CRC32 (RFC 1952 gzip trailer checksum).
    static uint32_t computeCrc32(const uint8_t* data, size_t len, uint32_t initial_crc = 0);

    /// Stream uncompressed Training Center Database v2 XML (.tcx) through a
    /// bounded 4 KB scratch buffer into write_chunk.
    static bool generateTcx(
        const RideSummary& summary,
        const RideRecord* records,
        size_t record_count,
        const WriteChunkFn& write_chunk,
        const DeadlineCheckFn& check_deadline = nullptr);

    static bool generateTcx(
        const RideSummary& summary,
        const std::vector<RideRecord>& records,
        const WriteChunkFn& write_chunk,
        const DeadlineCheckFn& check_deadline = nullptr) {
        return generateTcx(
            summary,
            records.empty() ? nullptr : records.data(),
            records.size(),
            write_chunk,
            check_deadline);
    }

    /// Load a ride from RideStore and stream uncompressed .tcx XML into write_chunk.
    static bool generateTcxFromStore(
        const RideStore& store,
        uint32_t session_id,
        const WriteChunkFn& write_chunk,
        const DeadlineCheckFn& check_deadline = nullptr);

    /// Stream RFC 1952 gzip-compressed Training Center Database v2 (.tcx.gz)
    /// (10-byte gzip header + RFC 1951 raw DEFLATE stream + CRC32 + ISIZE trailer)
    /// through bounded 4 KB buffers into write_chunk.
    static bool generateTcxGz(
        const RideSummary& summary,
        const RideRecord* records,
        size_t record_count,
        const WriteChunkFn& write_chunk,
        const DeadlineCheckFn& check_deadline = nullptr);

    static bool generateTcxGz(
        const RideSummary& summary,
        const std::vector<RideRecord>& records,
        const WriteChunkFn& write_chunk,
        const DeadlineCheckFn& check_deadline = nullptr) {
        return generateTcxGz(
            summary,
            records.empty() ? nullptr : records.data(),
            records.size(),
            write_chunk,
            check_deadline);
    }

    /// Load a ride from RideStore and stream gzip-compressed .tcx.gz into write_chunk.
    static bool generateTcxGzFromStore(
        const RideStore& store,
        uint32_t session_id,
        const WriteChunkFn& write_chunk,
        const DeadlineCheckFn& check_deadline = nullptr);

    /// Stream a ride's .tcx.gz directly into an IRideFileSystem file.
    static bool generateTcxGzToFile(
        const RideSummary& summary,
        const RideRecord* records,
        size_t record_count,
        IRideFileSystem& fs,
        const std::string& out_path,
        const DeadlineCheckFn& check_deadline = nullptr);

    /// Load a stored (or active) session from RideStore and stream its .tcx.gz
    /// to out_path (defaults to RideStore::getRideTcxGzPath(session_id) when empty).
    static bool generateTcxGzToFile(
        RideStore& store,
        uint32_t session_id,
        const std::string& out_path = "",
        const DeadlineCheckFn& check_deadline = nullptr);

    /// Decompress an RFC 1952 .tcx.gz stream and verify its IEEE 802.3 CRC32 and
    /// ISIZE trailer (supports RFC 1951 stored and fixed-Huffman blocks).
    static bool decompressGzip(
        const uint8_t* gz_data,
        size_t gz_len,
        std::vector<uint8_t>& out_uncompressed);
};

} // namespace echenon
