#pragma once
#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <atomic>

// Observes one proven DS2LE build. Never loads, patches, or writes DS2LE.
namespace fx::lantern_le_brightness {
enum class Status : uint8_t {
    not_initialized, absent, ready, unsupported_version, invalid_image,
    unreadable, invalid_value
};
struct Sample { float multiplier=1.0f; Status status=Status::not_initialized; };
const char* status_name(Status value) noexcept;

class Adapter final {
public:
    Adapter() = default;
    ~Adapter();
    Adapter(const Adapter&) = delete;
    Adapter& operator=(const Adapter&) = delete;
    // Call from one setup/worker thread. Absent detection may be retried there
    // (e.g. once per second) for late dxgi loading. A found image is hashed only
    // once. Sampling can run concurrently; ready is published atomically.
    Status initialize() noexcept;
    // Concurrent reads are allowed after initialization. Destroy only after all
    // readers stop. A module reference keeps the validated image loaded.
    Sample sample() const noexcept;
    Status detection_status() const noexcept { return status_.load(std::memory_order_acquire); }
#ifdef DS2FX_FIXTURE
    // Owned fixture only; never compiled into production Core.
    Status initialize_owned_fixture(void* image, size_t bytes, const char* disk_hash) noexcept;
    Status initialize_loaded_path_for_fixture(const wchar_t* absolute_path) noexcept;
#endif
private:
    HMODULE module_=nullptr;
    uintptr_t value_=0;
    std::atomic<Status> status_{Status::not_initialized};
    Status initialize_loaded_path(const wchar_t* absolute_path) noexcept;
};
}
