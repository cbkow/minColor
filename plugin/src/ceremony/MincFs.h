/* Cross-platform filesystem helpers (C++17 std::filesystem) so the ceremony code builds under
   MSVC as well as clang — replaces the POSIX mkdir / opendir-readdir / unlink / localtime_r that
   don't compile on Windows. std::filesystem is available on both (mac deployment target 13.0). */
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <ctime>
#include <system_error>
#include <filesystem>

namespace mfs {
namespace fs = std::filesystem;

inline void mkdirs(const std::string &p) { std::error_code ec; fs::create_directories(p, ec); }
inline bool removeFile(const std::string &p) { std::error_code ec; return fs::remove(p, ec); }
inline bool exists(const std::string &p) { std::error_code ec; return fs::exists(p, ec); }

/* recursive copy, skipping files that already exist (copyTree port) */
inline void copyTree(const std::string &from, const std::string &to) {
    std::error_code ec;
    fs::copy(from, to, fs::copy_options::recursive | fs::copy_options::skip_existing, ec);
}

struct DirEnt { std::string name; std::uintmax_t size; };
inline std::vector<DirEnt> listFiles(const std::string &dir) {
    std::vector<DirEnt> out; std::error_code ec;
    for (auto &e : fs::directory_iterator(dir, ec)) {
        std::error_code fe; if (!e.is_regular_file(fe)) continue;
        std::error_code se; out.push_back({ e.path().filename().string(), fs::file_size(e.path(), se) });
    }
    return out;
}

inline void localTime(const std::time_t *t, std::tm *out) {
#ifdef _WIN32
    localtime_s(out, t);
#else
    localtime_r(t, out);
#endif
}
} // namespace mfs
