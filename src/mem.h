#pragma once
#include "common.h"

namespace hx {

struct ModuleInfo {
    std::wstring name;
    uintptr_t base = 0;
    size_t size = 0;
};

// Read-only handle onto the target process plus the few primitives the rest of
// the program needs: typed reads, module lookup and byte-pattern scanning.
class Process {
public:
    ~Process();

    // Finds `exe` by image name and opens it with VM_READ | QUERY_INFORMATION.
    bool attach(const std::wstring& exe);
    void detach();

    bool running() const;
    uint32_t pid() const { return pid_; }
    HANDLE handle() const { return handle_; }

    // Surrounding %TEMP% style path helper for the log file.
    static std::wstring executableNameOf(uint32_t pid);

    const ModuleInfo* module(const std::wstring& name) const;
    bool refreshModules() const;

    bool read(uintptr_t address, void* buffer, size_t size) const;

    template <typename T>
    bool read(uintptr_t address, T& out) const {
        return read(address, &out, sizeof(T));
    }

    template <typename T>
    T readOr(uintptr_t address, T fallback = T{}) const {
        T value{};
        return read(address, value) ? value : fallback;
    }

    uintptr_t readPtr(uintptr_t address) const;
    Vec3 readVec3(uintptr_t address) const;

    // Scans `module` for an IDA style pattern ("48 8B 05 ? ? ? ?").
    // Returns the absolute address of the first match, or 0.
    uintptr_t scan(const std::wstring& module, const std::string& pattern, int* matchCount = nullptr);

private:
    HANDLE handle_ = nullptr;
    uint32_t pid_ = 0;
    mutable std::vector<ModuleInfo> modules_;
    mutable std::unordered_map<std::wstring, size_t> moduleIndex_;
};

// Decodes `mov/lea reg, [rip+disp32]` at `address` and returns the referenced
// absolute address. Returns 0 if the instruction is not RIP-relative.
uintptr_t decodeRipRelative(const Process& proc, uintptr_t address);

// Splits "48 8B 05 ? ? ? ?" into a byte/mask pair.
bool parsePattern(const std::string& pattern, std::vector<uint8_t>& bytes, std::vector<uint8_t>& mask);

}  // namespace hx
