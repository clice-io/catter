#pragma once

#include <iterator>
#include "array.h"
#include "linux-mac/config.h"
#include <string_view>

namespace catter {

template <char SeparatorCh>
class Splitter {
public:
    class Iterator;
    friend class Iterator;

public:
    explicit Splitter(const char* path) noexcept : begin_(path), end_(catter::array::end(path)) {}

    Splitter() = delete;
    Splitter(const Splitter&) = delete;
    Splitter& operator= (const Splitter&) = delete;
    Splitter(Splitter&&) = delete;
    Splitter& operator= (Splitter&&) = delete;

    [[nodiscard]] Iterator begin() const {
        if(begin_ == end_)
            return {*this, nullptr, nullptr};
        const auto candidate = next_path_separator(begin_, end_);
        return {*this, begin_, candidate};
    };

    [[nodiscard]] Iterator end() const {
        return {*this, nullptr, nullptr};
    };

private:
    const char* next_path_separator(const char* const current, const char* const end) const {
        auto it = current;
        while((it != end) && (*it != SeparatorCh)) {
            ++it;
        }
        return it;
    }

    [[nodiscard]] std::pair<const char*, const char*> next(const char* current) const {
        if(current == end_)
            return std::make_pair(nullptr, nullptr);

        const auto begin = std::next(current);
        if(begin == end_)
            return std::make_pair(nullptr, nullptr);

        const auto candidate = next_path_separator(begin, end_);
        return std::make_pair(begin, candidate);
    };

private:
    const char* const begin_;
    const char* const end_;

public:
    class Iterator {
    public:
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::input_iterator_tag;
        using value_type = std::string_view;
        using pointer = const value_type*;
        using reference = const value_type&;

    public:
        Iterator(const Splitter& paths, const char* begin, const char* end) noexcept :
            paths_(paths), begin_(begin), end_(end) {};

        value_type operator* () const {
            return std::string_view(begin_, (end_ - begin_));
        };

        Iterator operator++ (int) {
            Iterator result(*this);
            this->operator++ ();
            return result;
        };

        Iterator& operator++ () {
            const auto& [begin, end] = paths_.next(end_);
            begin_ = begin;
            end_ = end;
            return *this;
        };

        bool operator== (const Iterator& other) const {
            return &paths_ == &other.paths_ && begin_ == other.begin_ && end_ == other.end_;
        };

        bool operator!= (const Iterator& other) const {
            return !(this->operator== (other));
        };

    private:
        const Splitter& paths_;
        const char* begin_;
        const char* end_;
    };
};

using Paths = Splitter<config::OS_PATH_SEPARATOR>;
using SearchPaths = Splitter<config::OS_PATH_SEPARATOR>;

}  // namespace catter
