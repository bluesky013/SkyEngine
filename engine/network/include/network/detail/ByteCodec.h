//
// Created on 2026/09/25.
//
// Shared little-endian byte reader/writer used across the network module. Header-only.
//

#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace sky::net {

    class ByteWriter {
    public:
        explicit ByteWriter(std::vector<uint8_t> &out) : buffer(out) {}

        void U8(uint8_t value) { buffer.push_back(value); }

        void U16(uint16_t value)
        {
            buffer.push_back(static_cast<uint8_t>(value & 0xFFu));
            buffer.push_back(static_cast<uint8_t>((value >> 8u) & 0xFFu));
        }

        void U32(uint32_t value)
        {
            for (uint32_t i = 0; i < 4; ++i) {
                buffer.push_back(static_cast<uint8_t>((value >> (i * 8u)) & 0xFFu));
            }
        }

        void U64(uint64_t value)
        {
            for (uint32_t i = 0; i < 8; ++i) {
                buffer.push_back(static_cast<uint8_t>((value >> (i * 8u)) & 0xFFu));
            }
        }

        void Bytes(std::span<const uint8_t> data) { buffer.insert(buffer.end(), data.begin(), data.end()); }

    private:
        std::vector<uint8_t> &buffer;
    };

    class ByteReader {
    public:
        explicit ByteReader(std::span<const uint8_t> in) : data(in) {}

        bool U8(uint8_t &value)
        {
            if (offset + 1 > data.size()) { overflow = true; return false; }
            value = data[offset++];
            return true;
        }

        bool U16(uint16_t &value)
        {
            if (offset + 2 > data.size()) { overflow = true; return false; }
            value = static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset + 1]) << 8u);
            offset += 2;
            return true;
        }

        bool U32(uint32_t &value)
        {
            if (offset + 4 > data.size()) { overflow = true; return false; }
            value = 0;
            for (uint32_t i = 0; i < 4; ++i) {
                value |= static_cast<uint32_t>(data[offset + i]) << (i * 8u);
            }
            offset += 4;
            return true;
        }

        bool U64(uint64_t &value)
        {
            if (offset + 8 > data.size()) { overflow = true; return false; }
            value = 0;
            for (uint32_t i = 0; i < 8; ++i) {
                value |= static_cast<uint64_t>(data[offset + i]) << (i * 8u);
            }
            offset += 8;
            return true;
        }

        bool Bytes(uint32_t count, std::span<const uint8_t> &out)
        {
            if (offset + count > data.size()) { overflow = true; return false; }
            out = data.subspan(offset, count);
            offset += count;
            return true;
        }

        bool Ok() const { return !overflow; }

        // Remaining unread bytes.
        std::span<const uint8_t> Rest() const { return data.subspan(offset); }

    private:
        std::span<const uint8_t> data;
        uint32_t                 offset   = 0;
        bool                     overflow = false;
    };

} // namespace sky::net
