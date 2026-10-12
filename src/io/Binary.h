#pragma once

// B2.1 — Binary helpers for the .opxs scene file format.
// Layering rule: no raylib, no ImGui, no windows.h here.
// All multi-byte values are little-endian IEEE-754.

#include <bit>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>

namespace openphysx {

// ---------------------------------------------------------------------------
// Writer — append primitives into an in-memory buffer.
// Produces a flat byte sequence ready to write to disk.
// ---------------------------------------------------------------------------
class Writer
{
public:
    std::vector<uint8_t> buf;

    void u8(uint8_t v)
    {
        buf.push_back(v);
    }

    void u32(uint32_t v)
    {
        for (int i = 0; i < 4; ++i)
            buf.push_back(static_cast<uint8_t>(v >> (8 * i)));
    }

    void u64(uint64_t v)
    {
        for (int i = 0; i < 8; ++i)
            buf.push_back(static_cast<uint8_t>(v >> (8 * i)));
    }

    void f32(float f)
    {
        u32(std::bit_cast<uint32_t>(f));
    }

    // Length-prefixed UTF-8 string: u32 byte-count then raw bytes.
    void str(const std::string& s)
    {
        u32(static_cast<uint32_t>(s.size()));
        buf.insert(buf.end(), s.begin(), s.end());
    }

    // Write a 4-byte tag (e.g. "OPXS", "OBJS", "MESH").
    void tag(const char t[4])
    {
        for (int i = 0; i < 4; ++i)
            buf.push_back(static_cast<uint8_t>(t[i]));
    }

    // Start a chunk: write the 4-byte tag and a zero u64 length placeholder.
    // Returns the byte offset of the length field; pass to end_chunk().
    std::size_t begin_chunk(const char t[4])
    {
        tag(t);
        const std::size_t slot = buf.size();
        u64(0); // placeholder
        return slot;
    }

    // Patch the length field at `slot` with the actual payload size.
    void end_chunk(std::size_t slot)
    {
        const uint64_t len = static_cast<uint64_t>(buf.size() - (slot + 8));
        for (int i = 0; i < 8; ++i)
            buf[slot + i] = static_cast<uint8_t>(len >> (8 * i));
    }
};

// ---------------------------------------------------------------------------
// Reader — bounds-checked read from a contiguous byte buffer.
// Returns false on overrun; never throws UB, never allocates more than
// the remaining bytes.
// ---------------------------------------------------------------------------
class Reader
{
public:
    Reader() = default;
    Reader(const uint8_t* data, std::size_t size) : p_(data), end_(data + size) {}

    std::size_t remaining() const { return static_cast<std::size_t>(end_ - p_); }
    bool empty() const { return p_ >= end_; }

    bool u8(uint8_t& v)
    {
        if (remaining() < 1) return false;
        v = *p_++;
        return true;
    }

    bool u32(uint32_t& v)
    {
        if (remaining() < 4) return false;
        v = 0;
        for (int i = 0; i < 4; ++i)
            v |= static_cast<uint32_t>(p_[i]) << (8 * i);
        p_ += 4;
        return true;
    }

    bool u64(uint64_t& v)
    {
        if (remaining() < 8) return false;
        v = 0;
        for (int i = 0; i < 8; ++i)
            v |= static_cast<uint64_t>(p_[i]) << (8 * i);
        p_ += 8;
        return true;
    }

    bool f32(float& f)
    {
        uint32_t b;
        if (!u32(b)) return false;
        f = std::bit_cast<float>(b);
        return true;
    }

    // Length-prefixed UTF-8 string. Rejects length > remaining bytes.
    bool str(std::string& s)
    {
        uint32_t n;
        if (!u32(n)) return false;
        if (n > remaining()) return false;
        s.assign(reinterpret_cast<const char*>(p_), n);
        p_ += n;
        return true;
    }

    // Carve out a sub-reader for the next `n` bytes without consuming them
    // via the sub-reader advancing this reader's position.
    bool sub(uint64_t n, Reader& out)
    {
        if (n > remaining()) return false;
        out = Reader(p_, static_cast<std::size_t>(n));
        p_ += n;
        return true;
    }

    // Skip exactly `n` bytes.
    bool skip(uint64_t n)
    {
        if (n > remaining()) return false;
        p_ += n;
        return true;
    }

    // Read a 4-byte tag into buf[4] (no null terminator needed).
    bool tag(char buf[4])
    {
        if (remaining() < 4) return false;
        for (int i = 0; i < 4; ++i)
            buf[i] = static_cast<char>(*p_++);
        return true;
    }

private:
    const uint8_t* p_   = nullptr;
    const uint8_t* end_ = nullptr;
};

} // namespace openphysx
