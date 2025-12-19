#pragma once

namespace catter {

/**
 * Represents a character buffer.
 *
 * Define helper methods to persist character sequences. The covered
 * functionality is not more than a `memcpy` to a static char array.
 */
class Buffer {
public:
    /**
     * Takes the memory addresses of the buffer.
     *
     * @param begin of the buffer.
     * @param end of the buffer.
     */
    Buffer(char* begin, char* end) noexcept;

    ~Buffer() noexcept = default;

    /**
     * Copy the input to the buffer.
     *
     * @param input to persist.
     * @return the address of the persisted input.
     */
    const char* store(const char* input) noexcept;
    /**
     * Push the input to the buffer, will not add zero end of string.
     *
     * @param input to persist.
     * @return whether the push is successful.
     */
    bool push(const char* input) noexcept;

    Buffer(const Buffer&) noexcept = delete;
    Buffer& operator= (const Buffer&) noexcept = delete;
    Buffer(Buffer&&) noexcept = delete;
    Buffer& operator= (Buffer&&) noexcept = delete;
    Buffer() noexcept = delete;

private:
    char* top_;
    char* const end_;
};

inline Buffer::Buffer(char* const begin, char* const end) noexcept : top_(begin), end_(end) {}
}  // namespace catter
