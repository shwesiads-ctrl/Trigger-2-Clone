#pragma once
#include <juce_core/juce_core.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

// Converts Steven Slate Trigger 2 ".tci" sample files (V1, V2 and Instrument Editor containers, mono and stereo)
// to 24-bit WAV files, named by velocity layer and round robin.
//
// This is a C++ port of the decoders in https://github.com/AidanScroggins/tci2wav (MIT License,
// Copyright (c) 2026 Aidan Scroggins), whose WRITEUP.md documents the format. The format was reverse
// engineered by that project. Known limits, inherited from it: for stereo waves the first ~2 ms of the attack
// is not decodable yet, so those frames are left silent (graded B in MAP.txt); a mono wave that cannot be
// solved is skipped.
//
// Only convert libraries you own.
namespace TCI
{
using Bytes = std::vector<uint8_t>;
using Vec = std::vector<double>;

constexpr int kBlock = 201;
constexpr int kMaxK = 24;
constexpr double kSampleRate = 44100.0;

// ------------------------------------------------------------------ bit access (MSB first)
struct Bits
{
    const uint8_t* u;
    int64_t bytes;

    int64_t bitLen() const { return bytes * 8; }
    uint32_t at (int64_t j) const { return (j >= 0 && j < bytes) ? u[j] : 0u; }

    int byteAt (int64_t pos) const
    {
        const int64_t j = pos >> 3;
        const int s = (int) (pos & 7);
        uint32_t v = at (j) << s;
        if (s)
            v |= at (j + 1) >> (8 - s);
        return (int) (v & 0xFF);
    }

    // k-bit sign-magnitude field, k <= 24. k = 1 always decodes to 0.
    double sm (int64_t pos, int k) const
    {
        const int64_t j = pos >> 3;
        const int s = (int) (pos & 7);
        const uint32_t word = (at (j) << 24) | (at (j + 1) << 16) | (at (j + 2) << 8) | at (j + 3);
        const uint32_t code = (word >> (32 - s - k)) & ((1u << k) - 1u);
        const uint32_t mag = code & ((1u << (k - 1)) - 1u);
        return ((code & (1u << (k - 1))) && mag) ? -(double) mag : (double) mag;
    }

    Bits sub (int64_t byteOffset) const { return { u + byteOffset, bytes - byteOffset }; }
};

inline double sm24 (int b0, int b1, int b2)
{
    const int v = (b0 << 16) | (b1 << 8) | b2;
    return (v & 0x800000) ? -(double) (v & 0x7FFFFF) : (double) v;
}

// ------------------------------------------------------------------ containers
struct Wave
{
    int64_t comp = 0, frames = 0;
    juce::String stereo = "?";
    Bytes blob;
    Vec v1;
    bool hasV1 = false;
};

// Chain of [k:8][201 x k-bit] blocks that must use exactly `comp` bits and give exactly `count` samples.
inline bool v1Stream (const Bits& b, int64_t bitPos, int64_t comp, int64_t count, Vec& out)
{
    out.assign ((size_t) count, 0.0);
    const int64_t end = bitPos + comp;
    int64_t pos = bitPos, n = 0;
    while (n < count)
    {
        if (pos + 8 > end)
            return false;
        const int k = b.byteAt (pos);
        if (k < 1 || k > kMaxK)
            return false;
        pos += 8;
        const int64_t take = std::min<int64_t> (kBlock, count - n);
        if (pos + take * k > end)
            return false;
        for (int64_t i = 0; i < take; ++i, ++n)
            out[(size_t) n] = b.sm (pos + i * k, k);
        pos += take * k;
    }
    return pos == end;
}

inline bool inflateZlib (const uint8_t* data, size_t size, Bytes& out)
{
    auto tryFormat = [&] (const uint8_t* d, size_t n, juce::GZIPDecompressorInputStream::Format f)
    {
        juce::MemoryOutputStream mo;
        juce::GZIPDecompressorInputStream in (new juce::MemoryInputStream (d, n, false), true, f);
        mo.writeFromInputStream (in, -1);
        const auto block = mo.getMemoryBlock();
        out.assign ((const uint8_t*) block.getData(), (const uint8_t*) block.getData() + block.getSize());
        return ! out.empty();
    };
    if (tryFormat (data, size, juce::GZIPDecompressorInputStream::zlibFormat))
        return true;
    if (size > 6 && data[0] == 0x78)
        return tryFormat (data + 2, size - 6, juce::GZIPDecompressorInputStream::deflateFormat);
    return false;
}

inline uint32_t rd32 (const uint8_t* p) { return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24); }
inline uint32_t rd32be (const uint8_t* p) { return (uint32_t) p[3] | ((uint32_t) p[2] << 8) | ((uint32_t) p[1] << 16) | ((uint32_t) p[0] << 24); }

// Trigger 2 (V2): header, audio blob, then a zlib footer that holds the XML with per-wave sizes.
inline bool parseV2 (const Bytes& f, std::vector<Wave>& waves)
{
    if (f.size() < 100)
        return false;
    const uint64_t audioLen = rd32 (f.data() + 64);
    if (76 + audioLen + 8 >= f.size())
        return false;
    Bytes dec;
    if (! inflateZlib (f.data() + 76 + audioLen + 8, f.size() - (size_t) (76 + audioLen + 8), dec))
        return false;
    if (dec.size() < 8 || dec[0] != 'V' || dec[1] != 'C' || dec[2] != '2' || dec[3] != '!')
        return false;

    std::string xml ((const char*) dec.data() + 8, dec.size() - 8);
    const auto tag = xml.find ("<trigger_instrument");
    if (tag == std::string::npos)
        return false;
    const auto tagEnd = xml.find ('>', tag);
    if (tagEnd == std::string::npos)
        return false;

    juce::StringPairArray attrs;
    {
        size_t i = tag + 19;
        while (i < tagEnd)
        {
            while (i < tagEnd && std::isspace ((unsigned char) xml[i])) ++i;
            const size_t ks = i;
            while (i < tagEnd && xml[i] != '=' && ! std::isspace ((unsigned char) xml[i])) ++i;
            if (i >= tagEnd || xml[i] != '=') { ++i; continue; }
            const std::string key = xml.substr (ks, i - ks);
            ++i;
            if (i >= tagEnd || xml[i] != '"') continue;
            const size_t vs = ++i;
            while (i < tagEnd && xml[i] != '"') ++i;
            attrs.set (key.c_str(), juce::String::fromUTF8 (xml.substr (vs, i - vs).c_str()));
            ++i;
        }
    }
    const int n = attrs["data_count"].getIntValue();
    if (n <= 0)
        return false;

    int64_t off = 0;
    const uint8_t* blob = f.data() + 76;
    for (int i = 0; i < n; ++i)
    {
        const auto cs = attrs["wd" + juce::String (i) + "comp1"], fs = attrs["wd" + juce::String (i) + "frames"];
        if (cs.isEmpty() || fs.isEmpty())
            return false;
        Wave w;
        w.comp = cs.getLargeIntValue();
        w.frames = fs.getLargeIntValue();
        w.stereo = attrs.getValue ("wd" + juce::String (i) + "stereo", "?");
        const int64_t nb = (w.comp + 7) / 8;
        if (off + nb > (int64_t) audioLen)
            return false;
        w.blob.assign (blob + off, blob + off + nb);
        off += nb;
        waves.push_back (std::move (w));
    }
    return ! waves.empty();
}

// Oracle-shaped single wave: [01][comp u32][frames u32] + V1 stream.
inline bool parseV1 (const Bytes& f, std::vector<Wave>& waves)
{
    if (f.size() < 10 || f[0] != 1)
        return false;
    const Bits b { f.data(), (int64_t) f.size() };
    for (int le = 0; le < 2; ++le)
    {
        const int64_t comp = le ? rd32 (f.data() + 1) : rd32be (f.data() + 1);
        const int64_t fr = le ? rd32 (f.data() + 5) : rd32be (f.data() + 5);
        if (! (comp > 0 && comp <= 8 * ((int64_t) f.size() - 9)) || ! (fr > 1 && fr < 10000000))
            continue;
        Wave w;
        if (v1Stream (b, 72, comp, fr - 1, w.v1))
        {
            w.comp = comp; w.frames = fr; w.stereo = "0-x"; w.hasV1 = true;
            waves.push_back (std::move (w));
            return true;
        }
    }
    return false;
}

// Trigger Instrument Editor files ("COMPRESSED INSTRUMENT").
inline bool parseEditor (const Bytes& f, std::vector<Wave>& waves)
{
    if (f.size() < 128 || std::memcmp (f.data(), "TRIGGER ", 8) != 0)
        return false;
    std::string tag;
    for (int i = 8; i < 64 && f[(size_t) i]; ++i)
        tag += (char) f[(size_t) i];
    if (tag.rfind ("COMPRESSED", 0) != 0)
        return false;
    const Bits b { f.data(), (int64_t) f.size() };
    int64_t pos = 128;
    for (int w = 0; w < 256; ++w)
    {
        if (pos + 9 > (int64_t) f.size() || f[(size_t) pos] != 1)
            break;
        const int64_t comp = rd32 (f.data() + pos + 1), fr = rd32 (f.data() + pos + 5);
        if (! (comp > 0 && fr > 1 && fr < 10000000))
            break;
        const int64_t base = pos + 9, blen = (comp + 7) / 8;
        if (base + blen > (int64_t) f.size())
            break;
        Wave wv;
        if (! v1Stream (b, base * 8, comp, fr - 1, wv.v1))
            break;
        wv.comp = comp; wv.frames = fr; wv.stereo = "0-ed"; wv.hasV1 = true;
        waves.push_back (std::move (wv));
        pos = base + blen;
        if (pos + 8 > (int64_t) f.size() || f[(size_t) pos] != 5)
            break;
        pos += 8;
    }
    return ! waves.empty();
}

// ------------------------------------------------------------------ mono solver
struct Decoded { Vec samples; int64_t endpos; };

inline Decoded decodeBlocks (const Bits& b, int64_t pos)
{
    Decoded d;
    const int64_t bitlen = b.bitLen();
    while (pos + 8 <= bitlen)
    {
        const int k = b.byteAt (pos);
        if (! (k >= 1 && k <= kMaxK))
            break;
        pos += 8;
        int64_t n = kBlock;
        if (pos + n * k > bitlen)
            n = (bitlen - pos) / k;
        if (n <= 0)
            break;
        for (int64_t i = 0; i < n; ++i, pos += k)
            d.samples.push_back (b.sm (pos, k));
    }
    d.endpos = pos;
    return d;
}

struct Spec
{
    enum Kind { None, Raw, Blk, Blocks, Tail8 } kind = None;
    int64_t S = 0, C = 0, T = 0, T0 = 8;
    int k = 0;
    std::vector<std::pair<int, int>> blks;
};

struct Solved { Spec spec; char grade = 'X'; juce::String note; };

struct TailInfo { int64_t cnt, endpos; };

inline TailInfo tailCount (const Bits& b, int64_t T)
{
    const int64_t bitlen = b.bitLen();
    int64_t pos = T, cnt = 0;
    while (pos + 8 <= bitlen)
    {
        const int k = b.byteAt (pos);
        if (! (k >= 1 && k <= kMaxK))
            break;
        pos += 8;
        int64_t n = kBlock;
        if (pos + n * k > bitlen)
            n = (bitlen - pos) / k;
        if (n <= 0)
            break;
        pos += n * k;
        cnt += n;
    }
    return { cnt, pos };
}

inline double smoothScore (const Vec& seg, int k)
{
    if (seg.size() < 8)
        return 9.0;
    double s = 0;
    for (size_t i = 1; i < seg.size(); ++i)
        s += std::abs (seg[i] - seg[i - 1]);
    return (s / (double) (seg.size() - 1)) / std::pow (2.0, k - 1);
}

inline Vec decodeSpan (const Bits& b, int64_t pos, int k, int64_t C)
{
    Vec out ((size_t) C);
    for (int64_t i = 0; i < C; ++i, pos += k)
        out[(size_t) i] = b.sm (pos, k);
    return out;
}

inline int kmap (int byte)
{
    switch (byte)
    {
        case 145: case 149: case 29: case 62: case 75: return 23;
        case 142: case 129: case 191: case 20: return 20;
        case 22: case 140: return 21;
        case 24: return 24;
        default: return -1;
    }
}

inline bool tailEndOk (int64_t endp, int64_t L) { return endp >= L - 64; }

inline Solved solveWave (const Bytes& blob, int64_t fr)
{
    const Bits w { blob.data(), (int64_t) blob.size() };
    const int64_t bitlen = w.bitLen();

    // raw 200-sample head
    if (blob.size() >= 600)
    {
        double rmad = 0, prev = sm24 (blob[0], blob[1], blob[2]);
        for (int i = 1; i < 200; ++i)
        {
            const double v = sm24 (blob[(size_t) (3 * i)], blob[(size_t) (3 * i + 1)], blob[(size_t) (3 * i + 2)]);
            rmad += std::abs (v - prev);
            prev = v;
        }
        rmad = (rmad / 199.0) / std::pow (2.0, 23);
        if (rmad < 0.25)
        {
            const auto t = decodeBlocks (w.sub (600), 0);
            const int64_t over = 200 + (int64_t) t.samples.size() - fr;
            if (over >= -2 && tailEndOk (4800 + t.endpos, bitlen))
            {
                Solved s; s.spec.kind = Spec::Raw; s.grade = 'A';
                s.note = "raw200 over=" + juce::String (over);
                return s;
            }
        }
    }

    // run of identity blocks from bit 0
    {
        struct B { int k; int64_t p; };
        std::vector<B> blks;
        int64_t p = 0;
        while (p + 8 <= bitlen && blks.size() < 12)
        {
            const int k = w.byteAt (p);
            if (! (k >= 1 && k <= kMaxK) || p + 8 + 201 * k > bitlen)
                break;
            blks.push_back ({ k, p });
            p += 8 + 201 * k;
        }
        for (int n = (int) std::min<size_t> (blks.size(), 12); n >= 2; --n)
        {
            const int64_t Tp = blks[(size_t) n - 1].p + 8 + 201 * blks[(size_t) n - 1].k;
            const auto t = tailCount (w, Tp);
            const int64_t over = (int64_t) n * 201 + t.cnt - fr;
            if (over >= -2 && over <= 44 && tailEndOk (t.endpos, bitlen))
            {
                Solved s; s.spec.kind = Spec::Blocks; s.spec.T = Tp; s.grade = 'A';
                for (int i = 0; i < n; ++i)
                    s.spec.blks.push_back ({ blks[(size_t) i].k, 201 });
                s.note = juce::String (n) + "x identity blocks over=" + juce::String (over);
                return s;
            }
        }
    }

    // single attack block + V1 tail
    struct Cand { bool bonus; int64_t S, absOver, C; int k; int64_t T; int byte; double sc; };
    struct K1 { int64_t S, absOver, C, T; int byte; double sc; };
    std::vector<Cand> cands;
    std::vector<K1> k1c;

    // run scan: bit offsets that start long 201-sample chains, longest first
    std::vector<std::pair<double, int64_t>> runs;
    {
        const int64_t end = std::min<int64_t> (60000, bitlen - 8);
        for (int64_t pos = 0; pos < end; ++pos)
        {
            const int k0 = w.byteAt (pos);
            if (! (k0 >= 1 && k0 <= kMaxK))
                continue;
            double run = 0;
            int64_t p = pos;
            while (p + 8 <= bitlen)
            {
                const int k = w.byteAt (p);
                if (! (k >= 1 && k <= kMaxK))
                    break;
                if (p + 8 + 201 * k > bitlen) { run += 0.5; break; }
                p += 8 + 201 * k;
                run += 1;
                if (run > 600)
                    break;
            }
            if (run >= 10)
                runs.push_back ({ run, pos });
        }
        std::stable_sort (runs.begin(), runs.end(), [] (auto& a, auto& b) { return a.first > b.first; });
    }

    for (const auto& r : runs)
    {
        const int64_t T = r.second;
        const auto t = tailCount (w, T);
        if (! tailEndOk (t.endpos, bitlen))
            continue;
        const int64_t lo = std::max<int64_t> (8, fr - t.cnt - 8);
        for (int64_t C = lo; C <= fr - t.cnt + 44; ++C)
            for (int k = 1; k <= 24; ++k)
            {
                const int64_t S = T - 8 - C * k;
                if (S < 0 || S > 6000)
                    continue;
                const double sc = smoothScore (decodeSpan (w, S + 8, k, C), k);
                if (sc >= 0.15)
                    continue;
                const int b = w.byteAt (S);
                const int64_t ao = std::abs (C + t.cnt - fr);
                if (k == 1)
                    k1c.push_back ({ S, ao, C, T, b, sc });
                else
                    cands.push_back ({ b == k || kmap (b) == k, S, ao, C, k, T, b, sc });
            }
    }

    Solved best; bool haveBest = false;
    if (! cands.empty())
    {
        std::stable_sort (cands.begin(), cands.end(), [] (const Cand& a, const Cand& b)
        {
            if (a.bonus != b.bonus) return a.bonus;
            if (a.S != b.S) return a.S < b.S;
            return a.absOver < b.absOver;
        });
        const auto& c = cands[0];
        const int64_t over = c.C + tailCount (w, c.T).cnt - fr;
        best.spec.kind = Spec::Blk; best.spec.S = c.S; best.spec.k = c.k; best.spec.C = c.C; best.spec.T = c.T;
        best.grade = (over == 0 && c.sc < 0.08) ? 'A' : (std::abs (over) <= 2 ? 'B' : 'C');
        best.note = "C=" + juce::String (c.C) + " k=" + juce::String (c.k) + " S=" + juce::String (c.S) + " T=" + juce::String (c.T)
                    + " over=" + juce::String (over);
        haveBest = true;
    }
    else if (! k1c.empty())
    {
        std::stable_sort (k1c.begin(), k1c.end(), [] (const K1& a, const K1& b)
        {
            if (a.S != b.S) return a.S < b.S;
            return a.absOver < b.absOver;
        });
        const auto& c = k1c[0];
        best.spec.kind = Spec::Blk; best.spec.S = c.T - 8 - c.C; best.spec.k = 1; best.spec.C = c.C; best.spec.T = c.T;
        best.grade = 'C';
        best.note = "k=1 (silence?)";
        haveBest = true;
    }

    // tail from leading zero bytes
    Solved tailn; bool haveTail = false;
    {
        int Z = 0;
        while (Z < 4 && (size_t) Z < blob.size() && blob[(size_t) Z] == 0)
            ++Z;
        if (Z)
        {
            const int64_t T0 = 8 * Z;
            const auto t = tailCount (w, T0);
            const int64_t over = t.cnt - fr;
            if (over >= -8 && over <= 44 && tailEndOk (t.endpos, bitlen))
            {
                const int kk = w.byteAt (T0);
                if (kk >= 1 && kk <= 24 && smoothScore (decodeSpan (w, T0, kk, 201), kk) < 0.15)
                {
                    tailn.spec.kind = Spec::Tail8; tailn.spec.T0 = T0; tailn.grade = 'B';
                    tailn.note = "tail-from-" + juce::String (T0);
                    haveTail = true;
                }
            }
        }
    }
    if (haveBest && best.spec.kind == Spec::Blk && best.spec.k == 1 && haveTail) return tailn;
    if (haveBest) return best;
    if (haveTail) return tailn;
    Solved none; none.note = "unsolved";
    return none;
}

inline Vec decodeWave (const Bytes& blob, const Spec& sp)
{
    const Bits w { blob.data(), (int64_t) blob.size() };
    Vec out;
    switch (sp.kind)
    {
        case Spec::Raw:
        {
            for (int i = 0; i < 200; ++i)
                out.push_back (sm24 (blob[(size_t) (3 * i)], blob[(size_t) (3 * i + 1)], blob[(size_t) (3 * i + 2)]));
            auto t = decodeBlocks (w.sub (600), 0);
            out.insert (out.end(), t.samples.begin(), t.samples.end());
            break;
        }
        case Spec::Tail8:
            out = decodeBlocks (w, sp.T0).samples;
            break;
        case Spec::Blk:
        {
            for (int64_t i = 0; i < sp.C; ++i)
                out.push_back (w.sm (sp.S + 8 + i * sp.k, sp.k));
            auto t = decodeBlocks (w, sp.T);
            out.insert (out.end(), t.samples.begin(), t.samples.end());
            break;
        }
        case Spec::Blocks:
        {
            int64_t p = 0;
            for (auto& bk : sp.blks)
            {
                p += 8;
                for (int i = 0; i < bk.second; ++i, p += bk.first)
                    out.push_back (w.sm (p, bk.first));
            }
            auto t = decodeBlocks (w, sp.T);
            out.insert (out.end(), t.samples.begin(), t.samples.end());
            break;
        }
        default: break;
    }
    return out;
}

inline Vec voiceRule (const Vec& v, int64_t frames)
{
    Vec out ((size_t) frames, 0.0);
    std::copy (v.begin(), v.begin() + (std::ptrdiff_t) std::min<int64_t> ((int64_t) v.size(), frames), out.begin());
    return out;
}

// ------------------------------------------------------------------ stereo
struct StereoResult { Vec L, R; int64_t headFrames = 0, tailStart = 0, tailSamples = 0; };

inline bool decodeStereo (const Bytes& blob, int64_t comp, int64_t frames, StereoResult& out)
{
    if (! (comp > 0 && frames > 0))
        return false;
    const Bits b { blob.data(), (int64_t) blob.size() };
    const int64_t total = 2 * frames;
    const int64_t scan = std::min<int64_t> (65536, comp);

    auto walk = [&] (int64_t start, int64_t& count, int64_t& endp)
    {
        int64_t pos = start;
        count = 0;
        auto nearEnd = [&] { endp = pos; return pos >= comp - 64; };
        while (pos + 8 <= comp)
        {
            const int k = b.byteAt (pos);
            if (k < 1 || k > kMaxK)
                return nearEnd();
            pos += 8;
            int64_t n = kBlock;
            if (pos + n * k > comp)
                n = (comp - pos) / k;
            if (n <= 0)
                return nearEnd();
            pos += n * k;
            count += n;
            if (count > total)
                return false;
        }
        return nearEnd();
    };

    int64_t bestHead = -1, bestT = 0, bestCount = 0, bestEnd = 0;
    for (int64_t T = 0; T + 8 <= std::min<int64_t> (scan, b.bitLen()); ++T)
    {
        const int k = b.byteAt (T);
        if (k < 1 || k > kMaxK)
            continue;
        int64_t count, endp;
        if (! walk (T, count, endp))
            continue;
        const int64_t head = total - count;
        if (head < 0 || head > 4096)
            continue;
        if (bestHead < 0 || head < bestHead) { bestHead = head; bestT = T; bestCount = count; bestEnd = endp; }
    }
    if (bestHead < 0)
        return false;

    Vec joint ((size_t) bestCount, 0.0);
    {
        int64_t pos = bestT, n = 0;
        while (pos + 8 <= bestEnd && n < bestCount)
        {
            const int k = b.byteAt (pos);
            if (k < 1 || k > kMaxK)
                break;
            pos += 8;
            const int64_t m = std::min<int64_t> ({ (int64_t) kBlock, bestCount - n, (bestEnd - pos) / k });
            for (int64_t i = 0; i < m; ++i, ++n)
                joint[(size_t) n] = b.sm (pos + i * k, k);
            pos += m * k;
        }
    }
    out.L.assign ((size_t) frames, 0.0);
    out.R.assign ((size_t) frames, 0.0);
    for (int64_t j = 0; j < frames; ++j)
    {
        const int64_t i = 2 * j - bestHead;
        if (i >= 0) out.L[(size_t) j] = joint[(size_t) i];
        if (i + 1 >= 0) out.R[(size_t) j] = joint[(size_t) (i + 1)];
    }
    out.headFrames = (bestHead + 1) / 2;
    out.tailStart = bestT;
    out.tailSamples = bestCount;
    return true;
}

// ------------------------------------------------------------------ export
inline double attackPeak (const Vec& v)
{
    double m = 0;
    for (size_t i = 0; i < std::min<size_t> (v.size(), 5000); ++i)
        m = std::max (m, std::abs (v[i]));
    return m;
}

inline bool writeWav (const juce::File& file, const std::vector<const Vec*>& ch)
{
    const int nCh = (int) ch.size();
    const size_t n = ch[0]->size();
    juce::MemoryOutputStream m;
    auto w32 = [&] (uint32_t v) { m.writeInt ((int) v); };
    auto w16 = [&] (uint16_t v) { m.writeShort ((short) v); };
    m.write ("RIFF", 4); w32 (36 + (uint32_t) (n * (size_t) nCh * 3));
    m.write ("WAVEfmt ", 8); w32 (16); w16 (1); w16 ((uint16_t) nCh);
    w32 (44100); w32 (44100 * (uint32_t) nCh * 3); w16 ((uint16_t) (nCh * 3)); w16 (24);
    m.write ("data", 4); w32 ((uint32_t) (n * (size_t) nCh * 3));
    for (size_t i = 0; i < n; ++i)
        for (int c = 0; c < nCh; ++c)
        {
            const int v = (int) std::max (-8388608.0, std::min (8388607.0, std::round ((*ch[(size_t) c])[i]))) & 0xFFFFFF;
            m.writeByte ((char) (v & 255)); m.writeByte ((char) ((v >> 8) & 255)); m.writeByte ((char) ((v >> 16) & 255));
        }
    return file.replaceWithData (m.getData(), m.getDataSize());
}

inline juce::String cleanName (const juce::String& s)
{
    juce::String out;
    for (auto c : s)
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
            out += c;
    return out;
}

struct Result
{
    bool ok = false;
    juce::String error, summary;
    int written = 0, skipped = 0, stereo = 0;
};

// progress (0..1, text) returns false to cancel
using Progress = std::function<bool (double, const juce::String&)>;

inline Result convertFile (const juce::File& tci, const juce::File& outDir, const Progress& progress)
{
    Result res;
    Bytes data;
    {
        juce::FileInputStream in (tci);
        if (! in.openedOk())
        {
            res.error = "Cannot open " + tci.getFileName();
            return res;
        }
        const auto size = in.getTotalLength();
        if (size < 128 || size > (juce::int64) 1900 * 1024 * 1024)
        {
            res.error = "Unsupported file size";
            return res;
        }
        data.resize ((size_t) size);
        in.read (data.data(), (int) std::min<juce::int64> (size, 0x7fffffff));
    }

    if (progress && ! progress (0.02, "Reading " + tci.getFileName()))
        return res;

    std::vector<Wave> waves;
    if (! parseV2 (data, waves) && ! parseV1 (data, waves) && ! parseEditor (data, waves))
    {
        res.error = "Not a readable TCI file";
        return res;
    }
    Bytes().swap (data);

    // name: <FAMILY>_<MIC>_V##_RR#.wav from the file name
    const auto base = tci.getFileNameWithoutExtension();
    juce::StringArray toks;
    toks.addTokens (base, " \t", "");
    toks.removeEmptyStrings();
    juce::String mic = toks.size() > 1 ? cleanName (toks[toks.size() - 1].toUpperCase()) : juce::String();
    juce::String family = toks.size() > 1 ? juce::String() : cleanName (base);
    if (toks.size() > 1)
    {
        for (int i = 0; i < toks.size() - 1; ++i)
            family += cleanName (toks[i]);
    }
    if (family.isEmpty()) family = "Sample";
    if (mic.isEmpty()) mic = "MIC";

    struct Item { double pk = 0; int idx = 0; bool skip = false, st = false; Vec a, b; char grade = 'X'; juce::String note; };
    std::vector<Item> items;
    const int n = (int) waves.size();
    for (int i = 0; i < n; ++i)
    {
        if (progress && ! progress (0.05 + 0.8 * (double) i / (double) n, "Decoding sample " + juce::String (i + 1) + " of " + juce::String (n)))
        {
            res.error = "Cancelled";
            return res;
        }
        auto& w = waves[(size_t) i];
        Item it; it.idx = i;
        try
        {
            if (w.stereo.startsWith ("1"))
            {
                StereoResult sr;
                if (decodeStereo (w.blob, w.comp, w.frames, sr))
                {
                    it.st = true; it.a = std::move (sr.L); it.b = std::move (sr.R); it.grade = 'B';
                    it.pk = std::max (attackPeak (it.a), attackPeak (it.b));
                    it.note = "stereo, " + juce::String (sr.headFrames) + " head frames silent";
                }
                else { it.skip = true; it.note = "stereo: no tail found"; }
            }
            else if (w.hasV1)
            {
                it.a = voiceRule (w.v1, w.frames); it.grade = 'A'; it.pk = attackPeak (it.a); it.note = "V1 single";
            }
            else
            {
                const auto s = solveWave (w.blob, w.frames);
                if (s.spec.kind == Spec::None) { it.skip = true; it.note = s.note; }
                else
                {
                    it.a = voiceRule (decodeWave (w.blob, s.spec), w.frames);
                    it.grade = s.grade; it.pk = attackPeak (it.a); it.note = s.note;
                }
            }
        }
        catch (...) { it.skip = true; it.note = "error"; }
        Bytes().swap (w.blob);
        items.push_back (std::move (it));
    }

    // velocity layers by attack peak, round robins within layers whose peaks are within 12%
    std::vector<size_t> ranked;
    for (size_t i = 0; i < items.size(); ++i)
        if (! items[i].skip) ranked.push_back (i);
    std::stable_sort (ranked.begin(), ranked.end(), [&] (size_t a, size_t b) { return items[a].pk < items[b].pk; });
    std::vector<std::vector<size_t>> groups, cur;
    {
        std::vector<size_t> g;
        double mx = 0;
        for (auto i : ranked)
        {
            if (! g.empty() && items[i].pk / std::max (mx, 1e-9) >= 1.12) { groups.push_back (g); g.clear(); mx = 0; }
            g.push_back (i);
            mx = std::max (mx, items[i].pk);
        }
        if (! g.empty()) groups.push_back (g);
    }

    outDir.createDirectory();
    juce::String map;
    int total = 0;
    for (auto& g : groups) total += (int) g.size();
    int done = 0;
    for (size_t vi = 0; vi < groups.size(); ++vi)
    {
        auto g = groups[vi];
        std::stable_sort (g.begin(), g.end(), [&] (size_t a, size_t b) { return items[a].pk < items[b].pk; });
        for (size_t ri = 0; ri < g.size(); ++ri)
        {
            auto& it = items[g[ri]];
            const auto name = family + "_" + mic + "_V" + juce::String (vi + 1).paddedLeft ('0', 2) + "_RR" + juce::String (ri + 1) + ".wav";
            if (progress && ! progress (0.85 + 0.15 * (double) done / (double) std::max (1, total), "Writing " + name))
            {
                res.error = "Cancelled";
                return res;
            }
            std::vector<const Vec*> chans { &it.a };
            if (it.st) chans.push_back (&it.b);
            if (writeWav (outDir.getChildFile (name), chans))
            {
                ++res.written;
                if (it.st) ++res.stereo;
                map << name << " <- wave" << juce::String (it.idx).paddedLeft ('0', 2) << " [" << juce::String::charToString (it.grade) << "] " << it.note << "\n";
            }
            ++done;
        }
    }
    for (auto& it : items)
        if (it.skip)
        {
            ++res.skipped;
            map << "-- wave" << juce::String (it.idx).paddedLeft ('0', 2) << " skipped: " << it.note << "\n";
        }
    outDir.getChildFile ("MAP.txt").replaceWithText (map);

    res.ok = res.written > 0;
    if (! res.ok)
        res.error = "No samples could be decoded";
    res.summary = juce::String (res.written) + " WAV" + (res.written == 1 ? "" : "s") + (res.skipped ? ", " + juce::String (res.skipped) + " skipped" : juce::String());
    if (progress)
        progress (1.0, res.summary);
    return res;
}
} // namespace TCI
