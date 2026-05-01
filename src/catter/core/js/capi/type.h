#pragma once
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
#include <fcntl.h>
#include <cpptrace/exceptions.hpp>
#include <kota/support/type_traits.h>
#include <kota/meta/enum.h>
#include <kota/meta/struct.h>

#include "../qjs.h"
#include "util/enum.h"

namespace catter::js {

namespace detail {
template <typename T>
struct Bridge;
template <typename T>
T make_reflected_object(qjs::Object object);

template <typename T>
qjs::Object to_reflected_object(JSContext* ctx, const T& value);
}  // namespace detail

template <auto E>
    requires std::is_enum_v<std::decay_t<decltype(E)>>
struct Tag {
    bool operator== (const Tag& other) const = default;
};

template <auto... ES>
concept EnumValues =
    (std::is_enum_v<std::decay_t<decltype(ES)>> && ...) &&
    (std::same_as<std::decay_t<decltype(ES)>, std::decay_t<decltype((ES, ...))>> && ...);

template <auto... Es>
    requires EnumValues<Es...>
struct TaggedUnion;

#define TAG                                                                                        \
    template <>                                                                                    \
    struct Tag

template <auto... Es>
    requires EnumValues<Es...>
struct TaggedUnion : public std::variant<Tag<Es>...> {
    using TagType = std::common_type_t<std::decay_t<decltype(Es)>...>;
    using std::variant<Tag<Es>...>::variant;
    TaggedUnion() = default;
    TaggedUnion(const TaggedUnion&) = default;
    TaggedUnion(TaggedUnion&&) = default;
    TaggedUnion& operator= (const TaggedUnion&) = default;
    TaggedUnion& operator= (TaggedUnion&&) = default;

    static TaggedUnion make(qjs::Object object) {
        return detail::Bridge<TaggedUnion>::from_js(qjs::Value::from(object));
    }

    qjs::Object to_object(JSContext* ctx) const {
        return detail::Bridge<TaggedUnion>::to_js(ctx, *this);
    }

    std::variant<Tag<Es>...>& variant() {
        return static_cast<std::variant<Tag<Es>...>&>(*this);
    }

    const std::variant<Tag<Es>...>& variant() const {
        return static_cast<const std::variant<Tag<Es>...>&>(*this);
    }

    template <typename V>
    decltype(auto) visit(V&& visitor) const {
        return std::visit(std::forward<V>(visitor), this->variant());
    }

    template <typename V>
    decltype(auto) visit(V&& visitor) {
        return std::visit(std::forward<V>(visitor), this->variant());
    }

    TagType type() const {
        return visit([]<auto E>(const Tag<E>&) -> TagType { return E; });
    }

    template <auto E>
    decltype(auto) get_if() {
        return std::get_if<Tag<E>>(&this->variant());
    }

    template <auto E>
    decltype(auto) get_if() const {
        return std::get_if<Tag<E>>(&this->variant());
    }

    template <auto E>
    decltype(auto) get() {
        return std::get<Tag<E>>(this->variant());
    }

    template <auto E>
    decltype(auto) get() const {
        return std::get<Tag<E>>(this->variant());
    }

    bool operator== (const TaggedUnion& other) const {
        return this->visit([&]<typename T>(const T& tag) -> bool {
            return other.visit([&]<typename U>(const U& other_tag) -> bool {
                if constexpr(std::is_same_v<T, U>) {
                    return tag == other_tag;
                } else {
                    return false;
                }
            });
        });
    }
};

namespace detail {
namespace et = kota;

template <typename E>
constexpr E enum_value(std::string_view name) {
    if(auto val = et::meta::enum_value<E>(name); val.has_value()) {
        return *val;
    }
    throw cpptrace::runtime_error(std::format("Invalid enum value name: {}", name));
}

template <typename E>
constexpr std::string_view enum_name(E value) {
    return et::meta::enum_name(value, "unknown");
}

template <typename E>
std::vector<E> enum_values(const std::vector<std::string>& names) {
    std::vector<E> values;
    values.reserve(names.size());
    for(const auto& name: names) {
        values.push_back(enum_value<E>(name));
    }
    return values;
}

template <typename E>
std::vector<std::string> enum_names(const std::vector<E>& values) {
    std::vector<std::string> names;
    names.reserve(values.size());
    for(const auto& value: values) {
        names.push_back(std::string(enum_name(value)));
    }
    return names;
}

template <typename T>
struct property_name_mapper {
    constexpr static std::string_view map(std::string_view field_name) {
        return field_name;
    }
};

template <typename T>
struct Bridge {
    static T from_js(const qjs::Value& value) {
        return value.as<T>();
    }

    static auto to_js(JSContext* ctx, const T& value) {
        return qjs::Value::from(ctx, value);
    }
};

template <typename T>
    requires et::meta::reflectable_class<T>
struct Bridge<T> {
    static T from_js(const qjs::Value& value) {
        return make_reflected_object<T>(value.as<qjs::Object>());
    }

    static auto to_js(JSContext* ctx, const T& value) {
        return to_reflected_object(ctx, value);
    }
};

template <typename T>
    requires std::is_enum_v<T>
struct Bridge<T> {
    static T from_js(const qjs::Value& value) {
        return enum_value<T>(value.as<std::string>());
    }

    static auto to_js(JSContext* ctx, const T& value) {
        return qjs::Value::from(ctx, std::string(enum_name(value)));
    }
};

template <typename T>
    requires std::is_enum_v<T>
struct Bridge<std::vector<T>> {
    static std::vector<T> from_js(const qjs::Value& value) {
        auto names = value.as<qjs::Array<std::string>>().as<std::vector<std::string>>();
        return enum_values<T>(names);
    }

    static auto to_js(JSContext* ctx, const std::vector<T>& vec) {
        return qjs::Array<std::string>::from(ctx, enum_names(vec));
    }
};

template <typename T>
struct Bridge<std::vector<T>> {
    static std::vector<T> from_js(const qjs::Value& value) {
        return value.as<qjs::Array<T>>().template as<std::vector<T>>();
    }

    static auto to_js(JSContext* ctx, const std::vector<T>& vec) {
        return qjs::Array<T>::from(ctx, vec);
    }
};

template <auto... Es>
    requires EnumValues<Es...>
struct Bridge<TaggedUnion<Es...>> {
    using Union = TaggedUnion<Es...>;

    static Union from_js(const qjs::Value& value) {
        auto object = value.as<qjs::Object>();

        auto tag = object["type"].as<std::string>();

        return dispatch<typename Union::TagType>(tag, [&]<auto E>(in_place_enum<E>) -> Union {
            return make_reflected_object<Tag<E>>(object);
        });
    }

    static auto to_js(JSContext* ctx, const Union& union_value) {
        return union_value.visit([&]<auto E>(const Tag<E>& tag) {
            auto object = to_reflected_object(ctx, tag);
            object.set_property("type", std::string(enum_name(E)));
            return object;
        });
    }
};

template <typename T>
T read_property(const qjs::Object& object, std::string_view property_name) {
    if constexpr(et::is_optional_v<T>) {
        using value_type = typename T::value_type;
        auto prop_val = object[std::string(property_name)];
        if(!prop_val.is_undefined()) {
            return Bridge<value_type>::from_js(prop_val);
        } else {
            return std::nullopt;
        }
    } else {
        return Bridge<T>::from_js(object[std::string(property_name)]);
    }
}

template <typename T>
void write_property(qjs::Object& object,
                    std::string_view property_name,
                    JSContext* ctx,
                    const T& value) {
    if constexpr(et::is_optional_v<T>) {
        using value_type = typename T::value_type;
        if(value.has_value()) {
            object.set_property(std::string(property_name), Bridge<value_type>::to_js(ctx, *value));
        }
    } else {
        object.set_property(std::string(property_name), Bridge<T>::to_js(ctx, value));
    }
}

template <typename T>
T make_reflected_object(qjs::Object object) {
    T value{};
    et::meta::for_each(value, [&]<typename FieldType>(FieldType field) {
        using field_type = std::remove_const_t<typename FieldType::type>;
        field.value() =
            read_property<field_type>(object, property_name_mapper<T>::map(FieldType::name()));
    });
    return value;
}

template <typename T>
qjs::Object to_reflected_object(JSContext* ctx, const T& value) {
    auto object = qjs::Object::empty_one(ctx);
    et::meta::for_each(value, [&]<typename FieldType>(FieldType field) {
        write_property(object, property_name_mapper<T>::map(FieldType::name()), ctx, field.value());
    });
    return object;
}

}  // namespace detail

enum class ActionType { skip, drop, abort, modify };

struct CatterOptions {
    static CatterOptions make(qjs::Object object) {
        return detail::make_reflected_object<CatterOptions>(std::move(object));
    }

    qjs::Object to_object(JSContext* ctx) const {
        return detail::to_reflected_object(ctx, *this);
    }

    bool operator== (const CatterOptions&) const = default;

public:
    bool log;
};

struct CatterRuntime {
    enum class Type { inject, eslogger, env };

    static CatterRuntime make(qjs::Object object) {
        return detail::make_reflected_object<CatterRuntime>(std::move(object));
    }

    qjs::Object to_object(JSContext* ctx) const {
        return detail::to_reflected_object(ctx, *this);
    }

    bool operator== (const CatterRuntime&) const = default;

public:
    std::vector<ActionType> supportActions;
    Type type;
    bool supportParentId;
};

struct CatterConfig {
    static CatterConfig make(qjs::Object object) {
        return detail::make_reflected_object<CatterConfig>(std::move(object));
    }

    qjs::Object to_object(JSContext* ctx) const {
        return detail::to_reflected_object(ctx, *this);
    }

    bool operator== (const CatterConfig&) const = default;

public:
    std::string scriptPath;
    std::vector<std::string> scriptArgs;
    std::vector<std::string> buildSystemCommand;
    std::string buildSystemCommandCwd;
    CatterRuntime runtime;
    CatterOptions options;
    bool execute;
};

struct CommandData {
    static CommandData make(qjs::Object object) {
        return detail::make_reflected_object<CommandData>(std::move(object));
    }

    qjs::Object to_object(JSContext* ctx) const {
        return detail::to_reflected_object(ctx, *this);
    }

    bool operator== (const CommandData&) const = default;

public:
    std::string cwd;
    std::string exe;
    std::vector<std::string> argv;
    std::vector<std::string> env;
    CatterRuntime runtime;
    std::optional<int64_t> parent;
};

struct ProcessResult {
    static ProcessResult make(qjs::Object object) {
        return detail::make_reflected_object<ProcessResult>(std::move(object));
    }

    qjs::Object to_object(JSContext* ctx) const {
        return detail::to_reflected_object(ctx, *this);
    }

    bool operator== (const ProcessResult&) const = default;

public:
    int64_t code;
    std::string stdOut;
    std::string stdErr;
};

struct CatterErr {
    static CatterErr make(qjs::Object object) {
        return detail::make_reflected_object<CatterErr>(std::move(object));
    }

    qjs::Object to_object(JSContext* ctx) const {
        return detail::to_reflected_object(ctx, *this);
    }

    bool operator== (const CatterErr&) const = default;

public:
    std::string msg;
};

struct OptionItem {
    static OptionItem make(qjs::Object object) {
        return detail::make_reflected_object<OptionItem>(std::move(object));
    }

    qjs::Object to_object(JSContext* ctx) const {
        return detail::to_reflected_object(ctx, *this);
    }

    bool operator== (const OptionItem&) const = default;

public:
    std::vector<std::string> values;
    std::string key;
    uint32_t id;
    std::optional<uint32_t> unalias;
    uint32_t index;
};

struct OptionInfo {
    static OptionInfo make(qjs::Object object) {
        return detail::make_reflected_object<OptionInfo>(std::move(object));
    }

    qjs::Object to_object(JSContext* ctx) const {
        return detail::to_reflected_object(ctx, *this);
    }

    bool operator== (const OptionInfo&) const = default;

public:
    uint32_t id;
    std::string prefixedKey;
    uint32_t kind;
    uint32_t group;
    uint32_t alias;
    std::vector<std::string> aliasArgs;
    uint32_t flags;
    uint32_t visibility;
    uint32_t param;
    std::string help;
    std::string meta_var;
};

using Action =
    TaggedUnion<ActionType::skip, ActionType::drop, ActionType::abort, ActionType::modify>;

TAG<ActionType::modify> {
    CommandData data;
    bool operator== (const Tag& other) const = default;
};

template <>
struct detail::property_name_mapper<ProcessResult> {
    constexpr static std::string_view map(std::string_view field_name) {
        if(field_name == "stdOut") {
            return "stdout";
        }
        if(field_name == "stdErr") {
            return "stderr";
        }
        return field_name;
    }
};

#undef TAG
}  // namespace catter::js
