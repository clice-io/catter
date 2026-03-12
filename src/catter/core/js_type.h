#pragma once
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <eventide/reflection/enum.h>
#include <eventide/reflection/struct.h>

#include "qjs.h"

namespace catter::js {

namespace detail {
namespace et = eventide;

template <typename E>
E enum_value(std::string_view name) {
    if(auto val = et::refl::enum_value<E>(name); val.has_value()) {
        return *val;
    }
    throw std::runtime_error(std::format("Invalid enum value name: {}", name));
}

template <typename E>
std::string_view enum_name(E value) {
    return et::refl::enum_name(value, "unknown");
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
struct is_optional : std::false_type {};

template <typename T>
struct is_optional<std::optional<T>> : std::true_type {
    using value_type = T;
};

template <typename T>
constexpr inline bool is_optional_v = is_optional<T>::value;

template <typename T>
struct is_vector : std::false_type {};

template <typename T, typename Alloc>
struct is_vector<std::vector<T, Alloc>> : std::true_type {
    using value_type = T;
};

template <typename T>
constexpr inline bool is_vector_v = is_vector<T>::value;

template <typename T>
concept ReflectableObject = et::refl::reflectable_class<T>;

template <typename T>
struct property_name_mapper {
    constexpr static std::string_view map(std::string_view field_name) {
        return field_name;
    }
};

template <typename T>
T make_reflected_object(qjs::Object object);

template <typename T>
qjs::Object to_reflected_object(JSContext* ctx, const T& value);

template <typename T>
auto to_property_value(JSContext* ctx, const T& value) {
    if constexpr(std::is_enum_v<T>) {
        return qjs::Value::from(ctx, std::string(enum_name(value)));
    } else if constexpr(is_vector_v<T> && std::is_enum_v<typename is_vector<T>::value_type>) {
        return qjs::Array<std::string>::from(ctx, enum_names(value));
    } else if constexpr(is_vector_v<T> &&
                        std::same_as<typename is_vector<T>::value_type, std::string>) {
        return qjs::Array<std::string>::from(ctx, value);
    } else if constexpr(ReflectableObject<T>) {
        return to_reflected_object(ctx, value);
    } else {
        return qjs::Value::from(ctx, value);
    }
}

template <typename T>
T from_property_value(const qjs::Value& value) {
    if constexpr(std::is_enum_v<T>) {
        return enum_value<T>(value.as<std::string>());
    } else if constexpr(is_vector_v<T> && std::is_enum_v<typename is_vector<T>::value_type>) {
        auto names = value.as<qjs::Array<std::string>>().as<std::vector<std::string>>();
        return enum_values<typename is_vector<T>::value_type>(names);
    } else if constexpr(is_vector_v<T> &&
                        std::same_as<typename is_vector<T>::value_type, std::string>) {
        return value.as<qjs::Array<std::string>>().as<std::vector<std::string>>();
    } else if constexpr(ReflectableObject<T>) {
        return make_reflected_object<T>(value.as<qjs::Object>());
    } else {
        return value.as<T>();
    }
}

template <typename T>
T read_property(const qjs::Object& object, std::string_view property_name) {
    if constexpr(is_optional_v<T>) {
        using value_type = typename is_optional<T>::value_type;
        auto optional_value = object.get_optional_property(std::string(property_name));
        if(!optional_value.has_value() || optional_value->is_nothing()) {
            return std::nullopt;
        }
        return from_property_value<value_type>(*optional_value);
    } else {
        return from_property_value<T>(object[std::string(property_name)]);
    }
}

template <typename T>
void write_property(qjs::Object& object,
                    std::string_view property_name,
                    JSContext* ctx,
                    const T& value) {
    if constexpr(is_optional_v<T>) {
        if(value.has_value()) {
            object.set_property(std::string(property_name), to_property_value(ctx, value.value()));
        }
    } else {
        object.set_property(std::string(property_name), to_property_value(ctx, value));
    }
}

template <typename T>
T make_reflected_object(qjs::Object object) {
    T value{};
    et::refl::for_each(value, [&]<typename FieldType>(FieldType field) {
        using field_type = std::remove_const_t<typename FieldType::type>;
        field.value() =
            read_property<field_type>(object, property_name_mapper<T>::map(FieldType::name()));
    });
    return value;
}

template <typename T>
qjs::Object to_reflected_object(JSContext* ctx, const T& value) {
    auto object = qjs::Object::empty_one(ctx);
    et::refl::for_each(value, [&]<typename FieldType>(FieldType field) {
        write_property(object, property_name_mapper<T>::map(FieldType::name()), ctx, field.value());
    });
    return object;
}

}  // namespace detail

enum class Action { skip, drop, abort, modify };

enum class Event { finish, output };

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
    std::vector<Action> supportActions;
    std::vector<Event> supportEvents;
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
    CatterRuntime runtime;
    CatterOptions options;
    bool isScriptSupported;
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

struct ActionResult {
    static ActionResult make(qjs::Object object) {
        return detail::make_reflected_object<ActionResult>(std::move(object));
    }

    qjs::Object to_object(JSContext* ctx) const {
        return detail::to_reflected_object(ctx, *this);
    }

    bool operator== (const ActionResult&) const = default;

public:
    std::optional<CommandData> data;
    Action type;
};

struct ExecutionEvent {
    static ExecutionEvent make(qjs::Object object) {
        return detail::make_reflected_object<ExecutionEvent>(std::move(object));
    }

    qjs::Object to_object(JSContext* ctx) const {
        return detail::to_reflected_object(ctx, *this);
    }

    bool operator== (const ExecutionEvent&) const = default;

public:
    std::optional<std::string> stdOut;
    std::optional<std::string> stdErr;
    int64_t code;
    Event type;
};

template <>
struct detail::property_name_mapper<ExecutionEvent> {
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

}  // namespace catter::js
