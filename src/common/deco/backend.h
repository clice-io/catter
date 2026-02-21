#pragma once
#include "deco/decl.h"
#include "deco/ty.h"
#include "option/opt_specifier.h"
#include "option/opt_table.h"
#include "option/option.h"
#include "option/util.h"
#include "reflection/struct.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace deco {
namespace backend = catter::opt;
}

namespace deco::detail {
constexpr auto prefixSpan(decl::Prefix prefix) {
    if(prefix.empty()) {
        throw "Option prefix cannot be empty";
    }
    if(prefix.value & decl::Prefix::Dash) {
        if(prefix.value & decl::Prefix::DashDash) {
            if(prefix.value & decl::Prefix::Slash) {
                return backend::pfx_all;
            } else {
                return backend::pfx_dash_double;
            }
        } else {
            if(prefix.value & decl::Prefix::Slash) {
                return backend::pfx_slash_dash;
            } else {
                return backend::pfx_dash;
            }
        }
    } else {
        if(prefix.value & decl::Prefix::DashDash) {
            if(prefix.value & decl::Prefix::Slash) {
                return backend::pfx_slash_dash;
            } else {
                return backend::pfx_double;
            }
        } else {
            if(prefix.value & decl::Prefix::Slash) {
                return backend::pfx_slash_dash;
            } else {
                return backend::pfx_none;
            }
        }
    }
}

template <bool counting, std::size_t N = 0>
class StringPool {
    using PoolTy = std::conditional_t<counting, std::vector<char>, std::array<char, N>>;
    PoolTy pool_{};
    std::size_t offset_ = 0;

public:
    constexpr explicit StringPool(std::size_t reserve_bytes = 0) {
        if constexpr(counting) {
            pool_.reserve(reserve_bytes);
        } else {
            (void)reserve_bytes;
        }
    }

    constexpr std::string_view add(std::string_view str) {
        if constexpr(counting) {
            offset_ += str.size() + 1;
            return str;
        } else {
            if(offset_ + str.size() + 1 > N) {
                throw "String pool overflow";
            }
            std::copy(str.begin(), str.end(), pool_.begin() + offset_);
            pool_[offset_ + str.size()] = '\0';
            std::string_view result(pool_.data() + offset_, str.size());
            offset_ += str.size() + 1;
            return result;
        }
    }

    constexpr std::string_view add(std::string_view str1, std::string_view str2) {
        if constexpr(counting) {
            offset_ += str1.size() + str2.size() + 1;
            return str1;
        } else {
            if(offset_ + str1.size() + str2.size() + 1 > N) {
                throw "String pool overflow";
            }
            std::copy(str1.begin(), str1.end(), pool_.begin() + offset_);
            std::copy(str2.begin(), str2.end(), pool_.begin() + offset_ + str1.size());
            pool_[offset_ + str1.size() + str2.size()] = '\0';
            std::string_view result(pool_.data() + offset_, str1.size() + str2.size());
            offset_ += str1.size() + str2.size() + 1;
            return result;
        }
    }

    constexpr const char* addCStr(std::string_view str) {
        if constexpr(counting) {
            add(str);
            return "";
        } else {
            return add(str).data();
        }
    }

    constexpr std::size_t size() const {
        return offset_;
    }

    constexpr const char* data() const {
        if constexpr(counting) {
            return nullptr;
        } else {
            return pool_.data();
        }
    }

    constexpr auto storage() const {
        return pool_;
    }
};

struct BuildStats {
    std::size_t opt_count = 0;
    std::size_t strpool_bytes = 0;
    bool has_trailing_pack = false;
};

constexpr static std::size_t no_struct_index = std::numeric_limits<std::size_t>::max();

template <bool counting, std::size_t OptN = 0, std::size_t StrN = 0>
class OptBuilder {
    using InfoItem = backend::OptTable::Info;
    using PoolTy =
        std::conditional_t<counting, std::vector<InfoItem>, std::array<InfoItem, OptN + 1>>;
    using IdMapTy =
        std::conditional_t<counting, std::vector<std::size_t>, std::array<std::size_t, OptN + 1>>;

    // Keep a dummy at index 0 so item.id can be used as direct index.
    PoolTy pool_{};
    StringPool<counting, StrN> strPool_;
    IdMapTy idMap_{};
    std::size_t offset_ = 0;
    bool has_input_slot_ = false;
    bool has_trailing_pack_ = false;

    constexpr static auto makeDefaultItem(unsigned id) {
        return InfoItem::unaliased_one(backend::pfx_none,
                                       "",
                                       id,
                                       backend::Option::UnknownClass,
                                       0,
                                       "no help text",
                                       "");
    }

    constexpr auto& itemById(unsigned id) {
        if constexpr(counting) {
            return pool_[id];
        } else {
            return pool_[id];
        }
    }

    constexpr auto& newItem(std::size_t mapped_struct_idx = no_struct_index) {
        if constexpr(counting) {
            const auto item_id = static_cast<unsigned>(pool_.size());
            pool_.push_back(makeDefaultItem(item_id));
            auto& item = pool_.back();
            item.id = item_id;
            idMap_.push_back(mapped_struct_idx);
            return item;
        } else {
            if(offset_ + 1 >= pool_.size()) {
                throw "Option pool overflow";
            }
            ++offset_;
            const auto item_id = static_cast<unsigned>(offset_);
            pool_[item_id] = makeDefaultItem(item_id);
            pool_[item_id].id = item_id;
            idMap_[item_id] = mapped_struct_idx;
            return pool_[item_id];
        }
    }

    constexpr auto& setCommonOptions(InfoItem& item, const decl::CommonOptionFields& fields) {
        if(!fields.help.empty()) {
            item.help_text = strPool_.addCStr(fields.help);
        }
        return item;
    }

    constexpr auto& setNamedOptions(unsigned item_id,
                                    std::size_t mapped_struct_idx,
                                    const decl::NamedOptionFields& fields) {
        auto& item = itemById(item_id);
        item._prefixes = prefixSpan(fields.prefix);
        item._prefixed_name = strPool_.add(item._prefixes[0], fields.name);

        const auto item_snapshot = item;
        for(const auto& alias: fields.alias) {
            auto& aliased = newItem(mapped_struct_idx);
            auto aliased_id = aliased.id;
            aliased = item_snapshot;
            aliased.id = aliased_id;
            aliased._prefixed_name = strPool_.add(aliased._prefixes[0], alias);
            setCommonOptions(aliased, fields);
        }

        setCommonOptions(itemById(item_id), fields);
        return itemById(item_id);
    }

    template <typename ResTy>
    constexpr void addInputOption(const decl::InputOption<ResTy>& opt, std::size_t struct_idx) {
        if(has_input_slot_) {
            throw "Only one of DecoInput/DecoPack can be declared";
        }
        has_input_slot_ = true;
        auto& item = newItem(struct_idx);
        item = InfoItem::input(item.id);
        setCommonOptions(item, opt);
    }

    template <typename ResTy>
    constexpr void addPackOption(const decl::PackOption<ResTy>& opt, std::size_t struct_idx) {
        if(has_input_slot_) {
            throw "Only one of DecoInput/DecoPack can be declared";
        }
        has_input_slot_ = true;
        has_trailing_pack_ = true;
        auto& item = newItem(struct_idx);
        item = InfoItem::input(item.id);
        setCommonOptions(item, opt);
    }

    constexpr void addFlagOption(const decl::FlagOption& opt, std::size_t struct_idx) {
        auto& item = newItem(struct_idx);
        item.kind = backend::Option::FlagClass;
        item.param = 0;
        setNamedOptions(item.id, struct_idx, opt);
    }

    template <typename ResTy>
    constexpr void addKVOption(const decl::KVOption<ResTy>& opt, std::size_t struct_idx) {
        auto& item = newItem(struct_idx);
        item.kind = (opt.style == decl::KVStyle::Joined) ? backend::Option::JoinedClass
                                                         : backend::Option::SeparateClass;
        item.param = 1;
        setNamedOptions(item.id, struct_idx, opt);
    }

    template <typename ResTy>
    constexpr void addCommaOption(const decl::CommaJoinedOption<ResTy>& opt,
                                  std::size_t struct_idx) {
        auto& item = newItem(struct_idx);
        item.kind = backend::Option::CommaJoinedClass;
        item.param = 1;
        setNamedOptions(item.id, struct_idx, opt);
    }

    template <typename ResTy>
    constexpr void addMultiOption(const decl::MultiOption<ResTy>& opt, std::size_t struct_idx) {
        if(opt.arg_num == 0) {
            throw "DecoMulti arg_num must be greater than 0";
        }
        if(opt.arg_num > std::numeric_limits<unsigned char>::max()) {
            throw "DecoMulti arg_num exceeds backend param capacity";
        }
        auto& item = newItem(struct_idx);
        item.kind = backend::Option::MultiArgClass;
        item.param = static_cast<unsigned char>(opt.arg_num);
        setNamedOptions(item.id, struct_idx, opt);
    }

public:
    constexpr explicit OptBuilder(std::size_t reserve_bytes = 0) : strPool_(reserve_bytes) {
        if constexpr(counting) {
            pool_.reserve(16);
            idMap_.reserve(16);
        } else {
            idMap_.fill(no_struct_index);
        }

        // Dummy item: keeps id and index aligned (id 0 => index 0).
        if constexpr(counting) {
            pool_.push_back(makeDefaultItem(0));
            idMap_.push_back(no_struct_index);
        } else {
            pool_[0] = makeDefaultItem(0);
            idMap_[0] = no_struct_index;
        }

        auto& unknown = newItem(no_struct_index);
        unknown = InfoItem::unknown(unknown.id);
    }

    template <typename OptDeco>
    constexpr explicit OptBuilder(std::in_place_type_t<OptDeco>, std::size_t reserve_bytes = 0) :
        OptBuilder(reserve_bytes) {
        build<OptDeco>();
    }

    template <typename OptDeco>
    constexpr void build() {
        // Pass 1: add input-like options first, so OptTable can discover input id
        // before first searchable option.
        refl::for_each(OptDeco{}, [this](auto field) {
            using FieldTy = typename decltype(field)::type;
            using DecoTy = std::remove_cvref_t<FieldTy>;
            static_assert(ty::is_decoed<DecoTy>, "Only deco options are supported in OptDeco");
            auto& value = field.value();
            constexpr auto idx = decltype(field)::index();
            if constexpr(DecoTy::decoTy == decl::DecoType::Input) {
                addInputOption(
                    static_cast<const decl::InputOption<typename DecoTy::result_type>&>(value),
                    idx);
            } else if constexpr(DecoTy::decoTy == decl::DecoType::TrailingInput) {
                addPackOption(
                    static_cast<const decl::PackOption<typename DecoTy::result_type>&>(value),
                    idx);
            }
        });

        // Pass 2: add all searchable options in user order.
        refl::for_each(OptDeco{}, [this](auto field) {
            using FieldTy = typename decltype(field)::type;
            using DecoTy = std::remove_cvref_t<FieldTy>;
            auto& value = field.value();
            constexpr auto idx = decltype(field)::index();
            if constexpr(DecoTy::decoTy == decl::DecoType::Flag) {
                addFlagOption(static_cast<const decl::FlagOption&>(value), idx);
            } else if constexpr(DecoTy::decoTy == decl::DecoType::KV) {
                addKVOption(static_cast<const decl::KVOption<typename DecoTy::result_type>&>(value),
                            idx);
            } else if constexpr(DecoTy::decoTy == decl::DecoType::CommaJoined) {
                addCommaOption(
                    static_cast<const decl::CommaJoinedOption<typename DecoTy::result_type>&>(
                        value),
                    idx);
            } else if constexpr(DecoTy::decoTy == decl::DecoType::Multi) {
                addMultiOption(
                    static_cast<const decl::MultiOption<typename DecoTy::result_type>&>(value),
                    idx);
            }
        });
    }

    constexpr std::size_t opt_size() const {
        if constexpr(counting) {
            return pool_.size() - 1;
        } else {
            return offset_;
        }
    }

    constexpr std::size_t strpool_size() const {
        return strPool_.size();
    }

    constexpr auto option_infos() const {
        if constexpr(counting) {
            return std::span<const InfoItem>(pool_.data() + 1, pool_.size() - 1);
        } else {
            return std::span<const InfoItem>(pool_.data() + 1, offset_);
        }
    }

    constexpr auto id_map() const {
        if constexpr(counting) {
            return std::span<const std::size_t>(idMap_.data(), idMap_.size());
        } else {
            return std::span<const std::size_t>(idMap_.data(), offset_ + 1);
        }
    }

    constexpr std::size_t struct_index_of(backend::OptSpecifier opt) const {
        if(!opt.is_valid()) {
            return no_struct_index;
        }
        const auto id = opt.id();
        if(id >= id_map().size()) {
            return no_struct_index;
        }
        return idMap_[id];
    }

    auto make_opt_table() const& {
        return backend::OptTable(option_infos())
            .set_tablegen_mode(false)
            .set_dash_dash_parsing(true)
            .set_dash_dash_as_single_pack(has_trailing_pack_);
    }

    auto make_opt_table() const&& = delete;

    constexpr BuildStats finish() const {
        static_assert(counting, "finish() is only for counting builders");
        return BuildStats{
            .opt_count = pool_.size() - 1,
            .strpool_bytes = strPool_.size(),
            .has_trailing_pack = has_trailing_pack_,
        };
    }
};

template <typename OptDeco>
consteval auto build_stats() {
    OptBuilder<true> counter;
    counter.template build<OptDeco>();
    return counter.finish();
}

template <typename OptDeco>
consteval const auto& build_storage() {
    constexpr auto stats = build_stats<OptDeco>();
    constexpr static OptBuilder<false, stats.opt_count, stats.strpool_bytes> builder(
        std::in_place_type<OptDeco>,
        stats.strpool_bytes);
    return builder;
}
}  // namespace deco::detail
