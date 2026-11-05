#include "mem.h"

#include <tlhelp32.h>

namespace hx {
namespace {

constexpr size_t kChunk = 4u * 1024u * 1024u;

bool readChunked(HANDLE handle, uintptr_t address, uint8_t* out, size_t size) {
    size_t done = 0;
    bool ok = true;
    while (done < size) {
        const size_t step = std::min(kChunk, size - done);
        SIZE_T got = 0;
        if (!::ReadProcessMemory(handle, reinterpret_cast<LPCVOID>(address + done), out + done, step,
                                 &got)) {
            ok = false;
            std::memset(out + done, 0, step);
        }
        done += step;
    }
    return ok;
}

}  // namespace

Process::~Process() { detach(); }

void Process::detach() {
    if (handle_) {
        ::CloseHandle(handle_);
        handle_ = nullptr;
    }
    pid_ = 0;
    modules_.clear();
    moduleIndex_.clear();
}

bool Process::attach(const std::wstring& exe) {
    detach();

    HANDLE snap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    bool found = false;
    uint32_t pid = 0;

    if (::Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, exe.c_str()) == 0) {
                pid = pe.th32ProcessID;
                found = true;
                break;
            }
        } while (::Process32NextW(snap, &pe));
    }
    ::CloseHandle(snap);

    if (!found || pid == 0) return false;

    HANDLE h = ::OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!h) return false;

    handle_ = h;
    pid_ = pid;
    return refreshModules();
}

bool Process::running() const {
    if (!handle_) return false;
    DWORD code = 0;
    if (!::GetExitCodeProcess(handle_, &code)) return false;
    return code == STILL_ACTIVE;
}

bool Process::refreshModules() const {
    std::vector<ModuleInfo> modules;
    std::unordered_map<std::wstring, size_t> index;

    HANDLE snap = ::CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid_);
    if (snap == INVALID_HANDLE_VALUE) return false;

    MODULEENTRY32W me{};
    me.dwSize = sizeof(me);
    if (::Module32FirstW(snap, &me)) {
        do {
            ModuleInfo info;
            info.name = me.szModule;
            info.base = reinterpret_cast<uintptr_t>(me.modBaseAddr);
            info.size = me.modBaseSize;
            index[info.name] = modules.size();
            modules.push_back(std::move(info));
        } while (::Module32NextW(snap, &me));
    }
    ::CloseHandle(snap);

    modules_ = std::move(modules);
    moduleIndex_ = std::move(index);
    return !modules_.empty();
}

const ModuleInfo* Process::module(const std::wstring& name) const {
    if (moduleIndex_.empty() && !refreshModules()) return nullptr;

    auto it = moduleIndex_.find(name);
    if (it != moduleIndex_.end()) return &modules_[it->second];

    // Case-insensitive fallback, module names are not guaranteed in case.
    for (const auto& m : modules_) {
        if (_wcsicmp(m.name.c_str(), name.c_str()) == 0) return &m;
    }
    return nullptr;
}

bool Process::read(uintptr_t address, void* buffer, size_t size) const {
    if (!handle_ || address == 0 || size == 0) return false;
    SIZE_T got = 0;
    if (!::ReadProcessMemory(handle_, reinterpret_cast<LPCVOID>(address), buffer, size, &got)) {
        return false;
    }
    return got == size;
}

uintptr_t Process::readPtr(uintptr_t address) const {
    uintptr_t value = 0;
    if (!read(address, value)) return 0;
    return value;
}

Vec3 Process::readVec3(uintptr_t address) const {
    Vec3 v{};
    read(address, v);
    return v;
}

bool parsePattern(const std::string& pattern, std::vector<uint8_t>& bytes, std::vector<uint8_t>& mask) {
    bytes.clear();
    mask.clear();

    size_t i = 0;
    while (i < pattern.size()) {
        while (i < pattern.size() && std::isspace(static_cast<unsigned char>(pattern[i]))) ++i;
        if (i >= pattern.size()) break;

        if (pattern[i] == '?') {
            while (i < pattern.size() && pattern[i] == '?') ++i;
            bytes.push_back(0);
            mask.push_back(0);
            continue;
        }

        char* end = nullptr;
        const long value = std::strtol(pattern.c_str() + i, &end, 16);
        if (end == pattern.c_str() + i) return false;
        bytes.push_back(static_cast<uint8_t>(value & 0xFF));
        mask.push_back(0xFF);
        i = static_cast<size_t>(end - pattern.c_str());
    }

    return !bytes.empty();
}

uintptr_t decodeRipRelative(const Process& proc, uintptr_t address) {
    uint8_t code[16]{};
    if (!proc.read(address, code, sizeof(code))) return 0;

    size_t i = 0;
    // Legacy prefixes.
    while (i < 4) {
        const uint8_t b = code[i];
        if (b == 0x66 || b == 0x67 || b == 0x2E || b == 0x3E || b == 0x26 || b == 0x36 || b == 0x64 ||
            b == 0x65 || b == 0xF0 || b == 0xF2 || b == 0xF3) {
            ++i;
            continue;
        }
        break;
    }

    // REX prefix.
    if ((code[i] & 0xF0) == 0x40) ++i;

    const uint8_t opcode = code[i];
    if (opcode != 0x8B && opcode != 0x8D && opcode != 0x89 && opcode != 0x3B) return 0;
    ++i;

    const uint8_t modrm = code[i];
    if ((modrm & 0xC7) != 0x05) return 0;  // mod=00, rm=101 -> RIP relative
    ++i;

    int32_t disp = 0;
    std::memcpy(&disp, code + i, sizeof(disp));
    const size_t length = i + 4;
    return address + length + static_cast<uintptr_t>(static_cast<int64_t>(disp));
}

uintptr_t Process::scan(const std::wstring& moduleName, const std::string& pattern, int* matchCount) {
    if (matchCount) *matchCount = 0;

    const ModuleInfo* mod = module(moduleName);
    if (!mod || mod->size == 0) return 0;

    std::vector<uint8_t> bytes;
    std::vector<uint8_t> mask;
    if (!parsePattern(pattern, bytes, mask)) return 0;

    std::vector<uint8_t> image(mod->size, 0);
    readChunked(handle_, mod->base, image.data(), image.size());

    const size_t m = bytes.size();
    if (image.size() < m) return 0;

    uintptr_t first = 0;
    int hits = 0;

    const uint8_t* data = image.data();
    const size_t limit = image.size() - m;
    size_t offset = 0;

    while (offset <= limit) {
        const void* hit = std::memchr(data + offset, bytes[0], limit - offset + 1);
        if (!hit) break;

        const size_t pos = static_cast<size_t>(static_cast<const uint8_t*>(hit) - data);
        bool matched = true;
        for (size_t k = 1; k < m; ++k) {
            if (mask[k] && data[pos + k] != bytes[k]) {
                matched = false;
                break;
            }
        }

        if (matched) {
            if (hits == 0) first = mod->base + pos;
            ++hits;
            if (!matchCount) break;
            offset = pos + 1;
        } else {
            offset = pos + 1;
        }
    }

    if (matchCount) *matchCount = hits;
    return first;
}

std::wstring Process::executableNameOf(uint32_t pid) {
    HANDLE snap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return L"";
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    std::wstring name;
    if (::Process32FirstW(snap, &pe)) {
        do {
            if (pe.th32ProcessID == pid) {
                name = pe.szExeFile;
                break;
            }
        } while (::Process32NextW(snap, &pe));
    }
    ::CloseHandle(snap);
    return name;
}

}  // namespace hx
