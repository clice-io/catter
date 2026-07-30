#pragma once

/**
 * @brief Generic conversion bridge between C++ values and QuickJS values.
 *
 * This module centralizes the policy used by the C API data model to cross the C++/JavaScript
 * boundary. `Bridge<T>` provides the conversion entry points and delegates ordinary values to the
 * qjs wrappers. Specialized bridges add support for reflected structs, string-backed enums,
 * vectors, and tagged unions.
 *
 * Reflected structs are converted field by field. Optional fields accept `undefined` when reading
 * and are omitted when empty during writing. A reflected type may define a nested `name_mapper`
 * with a static `map` function to translate C++ field names to JavaScript property names.
 * `TaggedUnion` serializes each alternative as an object with a string `type` discriminator and
 * uses the reflected `Tag` specialization as its payload schema.
 *
 * Domain types should keep only their schema and local customization in their own headers, then
 * use `make_reflected_object` and `to_reflected_object` for object conversion.
 */

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

template <typename T>
struct Bridge;

template <typename T>
T make_reflected_object(qjs::Object object);

template <typename T>
qjs::Object to_reflected_object(JSContext* ctx, const T& value);

namespace detail {

template <typename E>
constexpr E enum_value(std::string_view name) {
    if(auto val = kota::meta::enum_value<E>(name); val.has_value()) {
        return *val;
    }
    throw cpptrace::runtime_error(std::format("Invalid enum value name: {}", name));
}

template <typename E>
constexpr std::string_view enum_name(E value) {
    return kota::meta::enum_name(value, "unknown");
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
T read_property(const qjs::Object& object, std::string_view property_name) {
    if constexpr(kota::is_optional_v<T>) {
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
    if constexpr(kota::is_optional_v<T>) {
        using value_type = typename T::value_type;
        if(value.has_value()) {
            object.set_property(std::string(property_name), Bridge<value_type>::to_js(ctx, *value));
        }
    } else {
        object.set_property(std::string(property_name), Bridge<T>::to_js(ctx, value));
    }
}
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
        return Bridge<TaggedUnion>::from_js(qjs::Value::from(object));
    }

    qjs::Object to_object(JSContext* ctx) const {
        return Bridge<TaggedUnion>::to_js(ctx, *this);
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
    requires kota::meta::reflectable_class<T>
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
        return detail::enum_value<T>(value.as<std::string>());
    }

    static auto to_js(JSContext* ctx, const T& value) {
        return qjs::Value::from(ctx, std::string(detail::enum_name(value)));
    }
};

template <typename T>
    requires std::is_enum_v<T>
struct Bridge<std::vector<T>> {
    static std::vector<T> from_js(const qjs::Value& value) {
        auto names = value.as<qjs::Array<std::string>>().as<std::vector<std::string>>();
        return detail::enum_values<T>(names);
    }

    static auto to_js(JSContext* ctx, const std::vector<T>& vec) {
        return qjs::Array<std::string>::from(ctx, detail::enum_names(vec));
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
            object.set_property("type", std::string(detail::enum_name(E)));
            return object;
        });
    }
};

template <typename T>
T make_reflected_object(qjs::Object object) {
    T value{};
    kota::meta::for_each(value, [&]<typename FieldType>(FieldType field) {
        using field_type = std::remove_const_t<typename FieldType::type>;

        if constexpr(requires { typename T::name_mapper; }) {
            field.value() =
                detail::read_property<field_type>(object, T::name_mapper::map(FieldType::name()));
        } else {
            field.value() = detail::read_property<field_type>(object, FieldType::name());
        }
    });
    return value;
}

template <typename T>
qjs::Object to_reflected_object(JSContext* ctx, const T& value) {
    auto object = qjs::Object::empty_one(ctx);
    kota::meta::for_each(value, [&]<typename FieldType>(FieldType field) {
        if constexpr(requires { typename T::name_mapper; }) {
            detail::write_property(object,
                                   T::name_mapper::map(FieldType::name()),
                                   ctx,
                                   field.value());
        } else {
            detail::write_property(object, FieldType::name(), ctx, field.value());
        }
    });
    return object;
}

}  // namespace catter::js
