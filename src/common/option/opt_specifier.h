#pragma once

namespace catter::opt {

/// OptSpecifier - Wrapper class for abstracting references to option IDs.
class OptSpecifier {
    unsigned _id = 0;

public:
    OptSpecifier() = default;
    explicit OptSpecifier(bool) = delete;

    /*implicit*/ OptSpecifier(unsigned id) : _id(id) {}

    bool is_valid() const {
        return this->_id != 0;
    }

    unsigned id() const {
        return this->_id;
    }

    bool operator== (OptSpecifier opt) const {
        return this->_id == opt.id();
    }

    bool operator!= (OptSpecifier opt) const {
        return !(*this == opt);
    }
};

}  // namespace catter::opt
