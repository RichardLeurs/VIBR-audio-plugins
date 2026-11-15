// VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
#pragma once

// Finds drum kits in a folder tree and maps their files onto the TR-08's voices by file / folder name.
// Plain C++ (no JUCE) so it can be tested on its own; runs on a background thread, never on the audio thread.
#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace tr08kit
{
namespace fs = std::filesystem;

// 0..10 match tr08::Inst (BD SD LT MT HT RS CP CB CY OH CH); 11..15 are the alternates behind the panel switches
enum Slot { BD, SD, LT, MT, HT, RS, CP, CB, CY, OH, CH, LC, MC, HC, CL, MA, kNumSlots };

struct Kit
{
    std::string name;                     // path relative to the samples root, '/' separated
    fs::path dir;
    std::array<std::vector<fs::path>, kNumSlots> files;   // every matching file per slot, in natural order (kick-2 before kick-10)
    int mapped = 0;                       // how many of the 11 primary voices have a sample
};

inline bool isAudioFile (const fs::path& p)
{
    const auto name = p.filename().string();
    if (name.empty() || name[0] == '.') return false;                // .DS_Store, AppleDouble "._" files
    std::string ext = p.extension().string();
    for (auto& c : ext) c = (char) std::tolower ((unsigned char) c);
    return ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac" || ext == ".mp3";
}

// lower-case alphabetic tokens; splits on non-letters and on lower->Upper camel-case boundaries
inline std::vector<std::string> tokenize (const std::string& s)
{
    std::vector<std::string> out;
    std::string cur;
    char prev = 0;
    const auto flush = [&] { if (! cur.empty()) { out.push_back (cur); cur.clear(); } };
    for (char c : s)
    {
        if (std::isalpha ((unsigned char) c))
        {
            if (std::isupper ((unsigned char) c) && std::islower ((unsigned char) prev)) flush();
            cur += (char) std::tolower ((unsigned char) c);
        }
        else flush();
        prev = c;
    }
    flush();
    return out;
}

inline int firstNumber (const std::string& s)
{
    for (size_t i = 0; i < s.size(); ++i)
        if (std::isdigit ((unsigned char) s[i]))
            return std::atoi (s.c_str() + i);
    return -1;
}

// slot for a file, from its path relative to the kit folder; -1 if it isn't a recognisable drum sound
inline int classify (const fs::path& relative)
{
    const std::string stem = (relative.parent_path() / relative.stem()).generic_string();
    const auto tokens = tokenize (stem);
    std::string joined;
    for (auto& t : tokens) joined += t;

    const auto tok = [&] (std::initializer_list<const char*> words)
    {
        for (auto w : words) for (auto& t : tokens) if (t == w) return true;
        return false;
    };
    const auto has = [&] (std::initializer_list<const char*> subs)
    {
        for (auto s : subs) if (joined.find (s) != std::string::npos) return true;
        return false;
    };

    // low / mid / high qualifiers, also when glued to the instrument word ("lotom", "tomlo")
    std::set<std::string> q (tokens.begin(), tokens.end());
    for (auto& t : tokens)
        for (const char* base : { "tom", "conga" })
            if (const auto p = t.find (base); p != std::string::npos && t != base)
            {
                q.insert (t.substr (0, p));
                q.insert (t.substr (p + std::string (base).size()));
            }
    const auto isQ = [&] (std::initializer_list<const char*> words) { for (auto w : words) if (q.count (w)) return true; return false; };
    const bool lo = isQ ({ "lo", "low", "l", "lt", "floor", "big" }),
               mid = isQ ({ "mid", "mi", "med", "medium", "m", "mt" }),
               hi = isQ ({ "hi", "high", "h", "ht", "small" });
    const int number = firstNumber (relative.stem().string());

    // hats first, so "open hat" isn't taken for something else
    if (tok ({ "oh", "ohh", "ohat", "openhat", "opn" }) || (has ({ "open" }) && (has ({ "hat" }) || tok ({ "hh" }))))
        return OH;
    if (tok ({ "ch", "chh", "clhat", "hh", "hat", "hihat", "clsd" }) || has ({ "closed", "hihat", "clsd" }) || (has ({ "hat" }) && ! has ({ "that" })))
        return CH;

    if (! has ({ "conga", "tom" }))                      // the plugin's own two-letter codes: LT-1, MT-2, HT-3
    {
        if (tok ({ "lt" })) return LT;
        if (tok ({ "mt" })) return MT;
        if (tok ({ "ht" })) return HT;
    }
    if (has ({ "conga" }))
        return lo ? LC : (hi ? HC : MC);
    if (has ({ "tom" }))
    {
        if (lo) return LT;
        if (hi) return HT;
        if (mid) return MT;
        if (number == 1) return HT;
        if (number == 3) return LT;
        return MT;
    }
    if (tok ({ "rim", "rs" }) || has ({ "rimshot", "sidestick" })) return RS;
    if (has ({ "clap" }) || tok ({ "cp", "hc" })) return CP;
    if (has ({ "cowbell" }) || tok ({ "cb", "cow" })) return CB;
    if (has ({ "clave" })) return CL;
    if (has ({ "maraca", "shaker" })) return MA;
    if (has ({ "cymbal", "crash", "crsh", "ride" }) || tok ({ "cy", "cym", "rd", "rc", "cr" })) return CY;
    if (has ({ "snare" }) || tok ({ "sd", "sn", "snr", "sna" })) return SD;
    if (has ({ "kick", "bassdrum" }) || tok ({ "bd", "bass", "kik" })) return BD;
    return -1;
}

// natural order: digit runs compare as numbers, letters case-insensitively
inline bool naturalLess (const fs::path& pa, const fs::path& pb)
{
    const std::string a = pa.generic_string(), b = pb.generic_string();
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size())
    {
        if (std::isdigit ((unsigned char) a[i]) && std::isdigit ((unsigned char) b[j]))
        {
            size_t ei = i, ej = j;
            while (ei < a.size() && std::isdigit ((unsigned char) a[ei])) ++ei;
            while (ej < b.size() && std::isdigit ((unsigned char) b[ej])) ++ej;
            const auto na = std::stoull (a.substr (i, std::min<size_t> (ei - i, 18))),
                       nb = std::stoull (b.substr (j, std::min<size_t> (ej - j, 18)));
            if (na != nb) return na < nb;
            i = ei; j = ej;
        }
        else
        {
            const int ca = std::tolower ((unsigned char) a[i]), cb = std::tolower ((unsigned char) b[j]);
            if (ca != cb) return ca < cb;
            ++i; ++j;
        }
    }
    return a.size() - i < b.size() - j;
}

inline void collectAudio (const fs::path& dir, int depth, std::vector<fs::path>& out)
{
    std::error_code ec;
    for (fs::directory_iterator it (dir, ec), end; ! ec && it != end; it.increment (ec))
    {
        const auto& p = it->path();
        if (p.filename().string().rfind ('.', 0) == 0) continue;
        if (it->is_directory (ec)) { if (depth < 4) collectAudio (p, depth + 1, out); }
        else if (isAudioFile (p)) out.push_back (p);
    }
}

inline Kit mapFiles (const fs::path& dir)
{
    Kit kit;
    kit.dir = dir;
    std::vector<fs::path> files;
    collectAudio (dir, 0, files);
    std::sort (files.begin(), files.end(), naturalLess);
    for (auto& f : files)
    {
        const int slot = classify (fs::relative (f, dir));
        if (slot >= 0)
            kit.files[(size_t) slot].push_back (f);
    }
    for (int i = 0; i <= CH; ++i) if (! kit.files[(size_t) i].empty()) ++kit.mapped;
    return kit;
}

constexpr int kMinMapped = 4;     // a folder needs at least this many recognisable voices to count as a kit

// A folder is a container when two or more of its sub-folders are kits on their own; otherwise it is a kit if it qualifies.
inline void scan (const fs::path& root, const fs::path& dir, int depth, std::vector<Kit>& kits)
{
    std::error_code ec;
    std::vector<fs::path> subdirs;
    for (fs::directory_iterator it (dir, ec), end; ! ec && it != end; it.increment (ec))
        if (it->is_directory (ec) && it->path().filename().string().rfind ('.', 0) != 0)
            subdirs.push_back (it->path());
    std::sort (subdirs.begin(), subdirs.end());

    int qualifying = 0;
    for (auto& s : subdirs)
        if (mapFiles (s).mapped >= kMinMapped) ++qualifying;

    if (qualifying >= 2 && depth < 5)
    {
        for (auto& s : subdirs) scan (root, s, depth + 1, kits);
        return;
    }

    auto kit = mapFiles (dir);
    if (kit.mapped >= kMinMapped)
    {
        kit.name = dir == root ? dir.filename().generic_string() : fs::relative (dir, root).generic_string();
        kits.push_back (std::move (kit));
    }
    else if (depth < 5)
        for (auto& s : subdirs) scan (root, s, depth + 1, kits);
}

inline std::vector<Kit> scanKits (const fs::path& root)
{
    std::vector<Kit> kits;
    scan (root, root, 0, kits);
    return kits;
}
} // namespace tr08kit
