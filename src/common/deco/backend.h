#pragma once
#include "deco/decl.h"
#include "deco/ty.h"

#include "reflection/struct.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace deco::detail {
struct ParsedNamedOption {
    std::span<const std::string_view> prefixes = backend::pfx_none;
    std::string_view prefix;
    std::string_view name;
};

constexpr auto parseNamedOption(std::string_view full_name) {
    if(full_name.starts_with("--")) {
        if(full_name.size() <= 2) {
            throw "Option name cannot be only '--'";
        }
        return ParsedNamedOption{backend::pfx_double, "--", full_name.substr(2)};
    }
    if(full_name.starts_with("-")) {
        if(full_name.size() <= 1) {
            throw "Option name cannot be only '-'";
        }
        return ParsedNamedOption{backend::pfx_dash, "-", full_name.substr(1)};
    }
    if(full_name.starts_with("/")) {
        if(full_name.size() <= 1) {
            throw "Option name cannot be only '/'";
        }
        return ParsedNamedOption{backend::pfx_slash_dash, "/", full_name.substr(1)};
    }
    throw "Option name must start with '-', '--', or '/'";
}

template <bool counting, std::size_t N = 0>
class MemPool {
    using PoolTy = std::conditional_t<counting, std::vector<char>, std::array<char, N>>;
    PoolTy pool_{};
    std::size_t offset_ = 0;

public:
    constexpr explicit MemPool(std::size_t reserve_bytes = 0) {
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

    constexpr std::string_view addReplace(std::string_view str, char old_char, char new_char) {
        if constexpr(counting) {
            offset_ += str.size() + 1;
            return str;
        } else {
            if(offset_ + str.size() + 1 > N) {
                throw "String pool overflow";
            }
            for(std::size_t i = 0; i < str.size(); ++i) {
                pool_[offset_ + i] = (str[i] == old_char) ? new_char : str[i];
            }
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

template <typename RootTy, bool counting, std::size_t OptN = 0, std::size_t StrN = 0>
class OptBuilder {
public:
    using AccessorFn = void* (*)(void*);

private:
    using InfoItem = backend::OptTable::Info;
    using PoolTy =
        std::conditional_t<counting, std::vector<InfoItem>, std::array<InfoItem, OptN + 1>>;
    using IdMapTy =
        std::conditional_t<counting, std::vector<AccessorFn>, std::array<AccessorFn, OptN + 1>>;

    // Keep a dummy at index 0 so item.id can be used as direct index.
    PoolTy pool_{};
    MemPool<counting, StrN> strPool_;
    IdMapTy idMap_{};

    std::size_t offset_ = 0;
    bool has_input_slot_ = false;
    bool has_trailing_pack_ = false;
    bool has_exclusive_category_ = false;
    unsigned exclusive_category_ = 0;

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

    template <typename ObjTy, std::size_t I>
    constexpr static auto& fieldByPath(ObjTy& object) {
        return refl::field_of<I>(object);
    }

    template <typename ObjTy, std::size_t I, std::size_t J, std::size_t... Rest>
    constexpr static auto& fieldByPath(ObjTy& object) {
        auto& nested = refl::field_of<I>(object);
        return fieldByPath<std::remove_cvref_t<decltype(nested)>, J, Rest...>(nested);
    }

    template <std::size_t... Path>
    static void* fieldAccessor(void* object) {
        if(object == nullptr) {
            return nullptr;
        }
        auto& field = fieldByPath<RootTy, Path...>(*static_cast<RootTy*>(object));
        return static_cast<void*>(&field);
    }

    constexpr auto& newItem(AccessorFn mapped_accessor = nullptr) {
        if constexpr(counting) {
            const auto item_id = static_cast<unsigned>(pool_.size());
            pool_.push_back(makeDefaultItem(item_id));
            auto& item = pool_.back();
            item.id = item_id;
            idMap_.push_back(mapped_accessor);
            return item;
        } else {
            if(offset_ + 1 >= pool_.size()) {
                throw "Option pool overflow";
            }
            ++offset_;
            const auto item_id = static_cast<unsigned>(offset_);
            pool_[item_id] = makeDefaultItem(item_id);
            pool_[item_id].id = item_id;
            idMap_[item_id] = mapped_accessor;
            return pool_[item_id];
        }
    }

    constexpr void registerExclusiveCategory(const decl::CommonOptionFields& fields) {
        if(!fields.exclusive) {
            return;
        }
        if(fields.category == 0) {
            throw "Exclusive option must set a non-zero category";
        }
        if(has_exclusive_category_ && exclusive_category_ != fields.category) {
            throw "Only one exclusive category is allowed";
        }
        has_exclusive_category_ = true;
        exclusive_category_ = fields.category;
    }

    constexpr static void configPush(std::vector<decl::ConfigFields>& config_stack,
                                     const decl::ConfigFields& cfg) {
        config_stack.push_back(cfg);
    }

    constexpr static void configPopNearestStart(std::vector<decl::ConfigFields>& config_stack) {
        if(config_stack.empty()) {
            throw "Unmatched config end field";
        }
        int i = config_stack.size() - 1;
        while(i >= 0 && config_stack[i--].type != decl::ConfigFields::Type::Start) {}
        if(config_stack[i + 1].type != decl::ConfigFields::Type::Start) {
            throw "Unmatched config end field";
        }
        config_stack.resize(i + 1);
    }

    constexpr static void configConsumeNext(std::vector<decl::ConfigFields>& config_stack) {
        config_stack.erase(std::remove_if(config_stack.begin(),
                                          config_stack.end(),
                                          [](const decl::ConfigFields& cfg) {
                                              return cfg.type == decl::ConfigFields::Type::Next;
                                          }),
                           config_stack.end());
    }

    constexpr static void onConfigField(std::vector<decl::ConfigFields>& config_stack,
                                        const decl::ConfigFields& cfg) {
        switch(cfg.type) {
            case decl::ConfigFields::Type::Start: configPush(config_stack, cfg); break;
            case decl::ConfigFields::Type::End: configPopNearestStart(config_stack); break;
            case decl::ConfigFields::Type::Next: configPush(config_stack, cfg); break;
        }
    }

    template <typename OptTy>
    constexpr static void applyCurrentConfig(OptTy& opt,
                                             std::vector<decl::ConfigFields>& config_stack) {
        for(const auto& cfg: config_stack) {
            opt.required = cfg.required;
            opt.exclusive = cfg.exclusive;
            opt.category = cfg.category;
            if(!cfg.help.empty()) {
                opt.help = cfg.help;
            }
        }
        configConsumeNext(config_stack);
    }

    template <typename CurrentTy>
    constexpr static void applyConfigPass(CurrentTy& object,
                                          std::vector<decl::ConfigFields>& config_stack) {
        refl::for_each(object, [&](auto field) {
            using FieldTy = std::remove_cvref_t<typename decltype(field)::type>;
            auto& value = field.value();
            if constexpr(std::is_base_of_v<decl::ConfigFields, FieldTy>) {
                onConfigField(config_stack, static_cast<const decl::ConfigFields&>(value));
            } else if constexpr(ty::is_decoed<FieldTy>) {
                applyCurrentConfig(static_cast<decl::CommonOptionFields&>(value), config_stack);
            } else if constexpr(refl::reflectable_class<FieldTy>) {
                applyConfigPass(value, config_stack);
            } else {
                static_assert(catter::meta::dep_true<FieldTy>,
                              "Only deco fields or nested option structs are supported.");
            }
        });
    }

    constexpr auto& setCommonOptions(InfoItem& item, const decl::CommonOptionFields& fields) {
        if(!fields.help.empty()) {
            item.help_text = strPool_.addCStr(fields.help);
        }
        if(!fields.meta_var.empty()) {
            item.meta_var = strPool_.addCStr(fields.meta_var);
        }
        return item;
    }

    constexpr auto& setNamedOptions(unsigned item_id,
                                    AccessorFn mapped_accessor,
                                    std::string_view field_name,
                                    const decl::NamedOptionFields& fields) {
        auto& item = itemById(item_id);

        auto setPrefixedName = [this](InfoItem& target, std::string_view full_name) {
            auto parsed = parseNamedOption(full_name);
            target._prefixes = parsed.prefixes;
            target._prefixed_name = strPool_.add(parsed.prefix, parsed.name);
        };

        if(fields.names.empty()) {
            auto normalized_name = strPool_.addReplace(field_name, '_', '-');
            if(normalized_name.size() == 1) {
                item._prefixes = backend::pfx_dash;
                item._prefixed_name = strPool_.add("-", normalized_name);
            } else {
                item._prefixes = backend::pfx_double;
                item._prefixed_name = strPool_.add("--", normalized_name);
            }
            setCommonOptions(item, fields);
            return item;
        }

        setPrefixedName(item, fields.names.front());

        const auto item_snapshot = item;
        for(std::size_t i = 1; i < fields.names.size(); ++i) {
            auto& alias = newItem(mapped_accessor);
            auto alias_id = alias.id;
            alias = item_snapshot;
            alias.id = alias_id;
            setPrefixedName(alias, fields.names[i]);
            setCommonOptions(alias, fields);
        }

        setCommonOptions(itemById(item_id), fields);
        return itemById(item_id);
    }

    template <typename OptTy>
    constexpr void addInputLikeOption(const OptTy& opt,
                                      AccessorFn mapped_accessor,
                                      bool trailing_pack) {
        if(has_input_slot_) {
            throw "Only one of DecoInput/DecoPack can be declared";
        }
        registerExclusiveCategory(opt);
        has_input_slot_ = true;
        has_trailing_pack_ = trailing_pack;
        auto& item = newItem(mapped_accessor);
        item = InfoItem::input(item.id);
        if constexpr(std::is_base_of_v<decl::CommonOptionFields, OptTy>) {
            setCommonOptions(item, opt);
        }
    }

    constexpr void addFlagOption(const decl::FlagOption& opt,
                                 AccessorFn mapped_accessor,
                                 std::string_view field_name) {
        registerExclusiveCategory(opt);
        auto& item = newItem(mapped_accessor);
        item.kind = backend::Option::FlagClass;
        item.param = 0;
        setNamedOptions(item.id, mapped_accessor, field_name, opt);
    }

    template <typename ResTy>
    constexpr void addKVOption(const decl::KVOption<ResTy>& opt,
                               AccessorFn mapped_accessor,
                               std::string_view field_name) {
        registerExclusiveCategory(opt);
        auto& item = newItem(mapped_accessor);
        item.kind = (opt.style == decl::KVStyle::Joined) ? backend::Option::JoinedClass
                                                         : backend::Option::SeparateClass;
        item.param = 1;
        setNamedOptions(item.id, mapped_accessor, field_name, opt);
    }

    template <typename ResTy>
    constexpr void addCommaOption(const decl::CommaJoinedOption<ResTy>& opt,
                                  AccessorFn mapped_accessor,
                                  std::string_view field_name) {
        registerExclusiveCategory(opt);
        auto& item = newItem(mapped_accessor);
        item.kind = backend::Option::CommaJoinedClass;
        item.param = 1;
        setNamedOptions(item.id, mapped_accessor, field_name, opt);
    }

    template <typename ResTy>
    constexpr void addMultiOption(const decl::MultiOption<ResTy>& opt,
                                  AccessorFn mapped_accessor,
                                  std::string_view field_name) {
        if(opt.arg_num == 0) {
            throw "DecoMulti arg_num must be greater than 0";
        }
        if(opt.arg_num > std::numeric_limits<unsigned char>::max()) {
            throw "DecoMulti arg_num exceeds backend param capacity";
        }
        registerExclusiveCategory(opt);
        auto& item = newItem(mapped_accessor);
        item.kind = backend::Option::MultiArgClass;
        item.param = static_cast<unsigned char>(opt.arg_num);
        setNamedOptions(item.id, mapped_accessor, field_name, opt);
    }

    template <bool input_pass, typename CurrentTy, std::size_t... Path>
    constexpr void buildPass(CurrentTy& object) {
        refl::for_each(object, [this](auto field) {
            using FieldTy = std::remove_cvref_t<typename decltype(field)::type>;
            auto& value = field.value();
            constexpr auto idx = decltype(field)::index();
            constexpr auto name = decltype(field)::name();

            if constexpr(std::is_base_of_v<decl::ConfigFields, FieldTy>) {
                return;
            } else if constexpr(ty::is_decoed<FieldTy>) {
                if constexpr(input_pass) {
                    if constexpr(FieldTy::decoTy == decl::DecoType::Input) {
                        addInputLikeOption(value, &fieldAccessor<Path..., idx>, false);
                    } else if constexpr(FieldTy::decoTy == decl::DecoType::TrailingInput) {
                        addInputLikeOption(value, &fieldAccessor<Path..., idx>, true);
                    }
                } else {
                    if constexpr(FieldTy::decoTy == decl::DecoType::Flag) {
                        addFlagOption(value, &fieldAccessor<Path..., idx>, name);
                    } else if constexpr(FieldTy::decoTy == decl::DecoType::KV) {
                        addKVOption(value, &fieldAccessor<Path..., idx>, name);
                    } else if constexpr(FieldTy::decoTy == decl::DecoType::CommaJoined) {
                        addCommaOption(value, &fieldAccessor<Path..., idx>, name);
                    } else if constexpr(FieldTy::decoTy == decl::DecoType::Multi) {
                        addMultiOption(value, &fieldAccessor<Path..., idx>, name);
                    }
                }
            } else if constexpr(refl::reflectable_class<FieldTy>) {
                buildPass<input_pass, FieldTy, Path..., idx>(value);
            } else {
                static_assert(catter::meta::dep_true<FieldTy>,
                              "Only deco fields or nested option structs are supported.");
            }
        });
    }

    template <typename DecoTy>
    constexpr static bool isFieldPresent(const DecoTy& field) {
        if constexpr(DecoTy::decoTy == decl::DecoType::Flag) {
            return field.value;
        } else {
            return field.value.has_value();
        }
    }

    template <typename CurrentTy, typename Callback>
    constexpr bool visitDecoFields(const CurrentTy& object, const Callback& callback) const {
        return refl::for_each(object, [this, &callback](auto field) {
            using FieldTy = std::remove_cvref_t<typename decltype(field)::type>;
            if constexpr(std::is_base_of_v<decl::ConfigFields, FieldTy>) {
                return true;
            } else if constexpr(ty::is_decoed<FieldTy>) {
                return bool(
                    callback(static_cast<const FieldTy&>(field.value()), decltype(field)::name()));
            } else if constexpr(refl::reflectable_class<FieldTy>) {
                return visitDecoFields(field.value(), callback);
            } else {
                static_assert(catter::meta::dep_true<FieldTy>,
                              "Only deco fields or nested option structs are supported.");
                return true;
            }
        });
    }

public:
    constexpr explicit OptBuilder(std::size_t reserve_bytes = 0) : strPool_(reserve_bytes) {
        if constexpr(counting) {
            pool_.reserve(16);
            idMap_.reserve(16);
        } else {
            idMap_.fill(nullptr);
        }

        // Dummy item: keeps id and index aligned (id 0 => index 0).
        if constexpr(counting) {
            pool_.push_back(makeDefaultItem(0));
            idMap_.push_back(nullptr);
        } else {
            pool_[0] = makeDefaultItem(0);
            idMap_[0] = nullptr;
        }

        auto& unknown = newItem(nullptr);
        unknown = InfoItem::unknown(unknown.id);
    }

    constexpr explicit OptBuilder(std::in_place_t, std::size_t reserve_bytes = 0) :
        OptBuilder(reserve_bytes) {
        build();
    }

    constexpr void build() {
        static_assert(refl::reflectable_class<RootTy>,
                      "OptBuilder root type must be a reflectable struct");
        auto object = RootTy{};
        std::vector<decl::ConfigFields> config_stack;
        applyConfigPass(object, config_stack);
        buildPass<true>(object);
        buildPass<false>(object);
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
            return std::span<const AccessorFn>(idMap_.data(), idMap_.size());
        } else {
            return std::span<const AccessorFn>(idMap_.data(), offset_ + 1);
        }
    }

    constexpr void* field_ptr_of(backend::OptSpecifier opt, RootTy& object) const {
        if(!opt.is_valid()) {
            return nullptr;
        }
        const auto id = opt.id();
        if(id >= id_map().size()) {
            return nullptr;
        }
        auto accessor = idMap_[id];
        if(accessor == nullptr) {
            return nullptr;
        }
        return accessor(static_cast<void*>(&object));
    }

    std::optional<std::string> validate(const RootTy& object) const {
        auto configured = object;
        std::vector<decl::ConfigFields> config_stack;
        applyConfigPass(configured, config_stack);

        struct CategoryState {
            unsigned category = 0;
            unsigned total = 0;
            unsigned present = 0;
            bool exclusive = false;
        };

        std::vector<CategoryState> category_states;
        std::optional<std::string> err = std::nullopt;

        auto findOrAddCategory = [&](unsigned category) -> CategoryState& {
            auto it = std::find_if(
                category_states.begin(),
                category_states.end(),
                [category](const CategoryState& item) { return item.category == category; });
            if(it == category_states.end()) {
                category_states.push_back(CategoryState{.category = category});
                return category_states.back();
            }
            return *it;
        };

        visitDecoFields(configured, [&](const auto& field, std::string_view field_name) {
            const bool present = isFieldPresent(field);
            if(field.required && !present) {
                err = std::format("required option is missing: {}", field_name);
                return false;
            }
            if(field.category != 0) {
                auto& category_state = findOrAddCategory(field.category);
                category_state.total += 1;
                if(present) {
                    category_state.present += 1;
                }
                category_state.exclusive = category_state.exclusive || field.exclusive;
            }
            return true;
        });
        if(err.has_value()) {
            return err;
        }

        unsigned present_categories = 0;
        unsigned present_exclusive_categories = 0;
        unsigned exclusive_category = 0;
        for(const auto& category_state: category_states) {
            if(category_state.present != 0 && category_state.present != category_state.total) {
                return std::string("category ") + std::to_string(category_state.category) +
                       " must be all set or all unset";
            }
            if(category_state.present > 0) {
                present_categories += 1;
                if(category_state.exclusive) {
                    present_exclusive_categories += 1;
                    exclusive_category = category_state.category;
                }
            }
        }

        if(present_exclusive_categories > 1) {
            return "multiple exclusive categories are present";
        }
        if(present_exclusive_categories == 1 && present_categories > 1) {
            return std::string("exclusive category ") + std::to_string(exclusive_category) +
                   " cannot be combined with other categories";
        }
        return std::nullopt;
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
    OptBuilder<OptDeco, true> counter;
    counter.build();
    return counter.finish();
}

template <typename OptDeco>
consteval const auto& build_storage() {
    constexpr auto stats = build_stats<OptDeco>();
    constexpr static OptBuilder<OptDeco, false, stats.opt_count, stats.strpool_bytes> builder(
        std::in_place,
        stats.strpool_bytes);
    return builder;
}
}  // namespace deco::detail
