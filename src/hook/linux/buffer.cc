#include "./buffer.h"
#include "./array.h"
#include <utility>

namespace catter {

const char* Buffer::store(const char* const input) noexcept {
    if(input == nullptr)
        return nullptr;

    const auto input_end = catter::array::end(input) + 1;  // include the zero element
    auto top = catter::array::copy(input, input_end, top_, end_);
    if(top != nullptr)
        std::swap(top_, top);
    return top;
}

}  // namespace catter
