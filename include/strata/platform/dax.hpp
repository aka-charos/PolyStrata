// include/strata/platform/dax.hpp - whether a file is mapped straight from persistent memory (DAX).
//
// On a volume that maps persistent memory directly (Intel Optane persistent memory in App Direct mode, formatted
// with DAX: NTFS with `format /dax`, ext4 or xfs mounted with `-o dax`), the system documents a file's mapping as
// the persistent memory's own pages, with no copy in its file cache.  What strata-poly does for a file on a drive
// - holding the RAM that its mapped experts take within a budget, reading them through the file cache ahead of the
// page faults, dropping their pages between requests - is then not wanted (Options::dax, docs/PMEM.md).
//
// This asks the system and nothing more: it measures nothing.  Windows says it of the volume
// (FILE_DAX_VOLUME), and maps a compressed, encrypted or sparse file there through the file cache as on a drive,
// so such a file is not one.  Linux says it of the file (STATX_ATTR_DAX, from 5.8); an older kernel does not say,
// and the answer is no.
//
// STRATA_DAX=1 answers yes for every file and STRATA_DAX=0 answers no, whatever the system says: the first runs
// the engine's DAX path on a PC without persistent memory or on a kernel that does not say, the second turns it
// off.
#pragma once

#include <cstdlib>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <fcntl.h>
#include <sys/stat.h>
#endif

namespace strata::platform {

/// What STRATA_DAX says: 1 for "every file is", 0 for "no file is", -1 when it is not set (ask the system).
inline int dax_override() {
    const char* v = std::getenv("STRATA_DAX");
    if (v == nullptr || *v == '\0') return -1;
    return std::atoi(v) != 0 ? 1 : 0;
}

/// Whether the system says `path` is mapped straight from persistent memory.  False when the file cannot be
/// opened or the system does not say.
inline bool system_says_dax(const std::string& path) {
#if defined(_WIN32)
    // FILE_DAX_VOLUME, from winnt.h of the Windows 10 SDK 10.0.14393 and later
    constexpr DWORD kFileDaxVolume = 0x20000000;
    // no access asked for: the volume's flags and the file's attributes need none
    const HANDLE h = CreateFileA(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD flags = 0;
    BY_HANDLE_FILE_INFORMATION info{};
    const bool ok = GetVolumeInformationByHandleW(h, nullptr, 0, nullptr, nullptr, &flags, nullptr, 0) != 0 &&
                    GetFileInformationByHandle(h, &info) != 0;
    CloseHandle(h);
    // a compressed, encrypted or sparse file on a DAX volume is mapped through the file cache
    constexpr DWORD kCached = FILE_ATTRIBUTE_COMPRESSED | FILE_ATTRIBUTE_ENCRYPTED | FILE_ATTRIBUTE_SPARSE_FILE;
    return ok && (flags & kFileDaxVolume) != 0 && (info.dwFileAttributes & kCached) == 0;
#elif defined(__linux__) && defined(STATX_ATTR_DAX)
    // the file's own state: a volume mounted with dax=inode has files of both kinds
    struct statx sx {};
    if (statx(AT_FDCWD, path.c_str(), 0, STATX_BASIC_STATS, &sx) != 0) return false;
    return (sx.stx_attributes_mask & STATX_ATTR_DAX) != 0 && (sx.stx_attributes & STATX_ATTR_DAX) != 0;
#else
    (void) path;
    return false;
#endif
}

/// Whether `path` is taken as mapped straight from persistent memory: what the system says, or what STRATA_DAX
/// says in its place.
inline bool file_is_dax(const std::string& path) {
    const int forced = dax_override();
    return forced >= 0 ? forced == 1 : system_says_dax(path);
}

}  // namespace strata::platform
