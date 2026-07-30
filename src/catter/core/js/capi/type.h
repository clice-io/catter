#pragma once
#include "bridge.h"

namespace catter::js {

enum class ActionType { skip, drop, abort, modify };

struct CatterOptions {
    enum class StdioMode { inherit, capture };

    static CatterOptions make(qjs::Object object) {
        return make_reflected_object<CatterOptions>(std::move(object));
    }

    qjs::Object to_object(JSContext* ctx) const {
        return to_reflected_object(ctx, *this);
    }

    bool operator== (const CatterOptions&) const = default;

public:
    bool log;
    StdioMode stdioMode;
};

struct CatterRuntime {
    enum class Type { inject, eslogger, env };

    static CatterRuntime make(qjs::Object object) {
        return make_reflected_object<CatterRuntime>(std::move(object));
    }

    qjs::Object to_object(JSContext* ctx) const {
        return to_reflected_object(ctx, *this);
    }

    bool operator== (const CatterRuntime&) const = default;

public:
    std::vector<ActionType> supportActions;
    Type type;
    bool supportParentId;
};

struct CatterConfig {
    static CatterConfig make(qjs::Object object) {
        return make_reflected_object<CatterConfig>(std::move(object));
    }

    qjs::Object to_object(JSContext* ctx) const {
        return to_reflected_object(ctx, *this);
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
        return make_reflected_object<CommandData>(std::move(object));
    }

    qjs::Object to_object(JSContext* ctx) const {
        return to_reflected_object(ctx, *this);
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
    struct name_mapper {
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

    static ProcessResult make(qjs::Object object) {
        return make_reflected_object<ProcessResult>(std::move(object));
    }

    qjs::Object to_object(JSContext* ctx) const {
        return to_reflected_object(ctx, *this);
    }

    bool operator== (const ProcessResult&) const = default;

public:
    int64_t code;
    std::string stdOut;
    std::string stdErr;
};

struct CatterErr {
    static CatterErr make(qjs::Object object) {
        return make_reflected_object<CatterErr>(std::move(object));
    }

    qjs::Object to_object(JSContext* ctx) const {
        return to_reflected_object(ctx, *this);
    }

    bool operator== (const CatterErr&) const = default;

public:
    std::string msg;
    std::optional<int64_t> parent;
};

struct OptionItem {
    static OptionItem make(qjs::Object object) {
        return make_reflected_object<OptionItem>(std::move(object));
    }

    qjs::Object to_object(JSContext* ctx) const {
        return to_reflected_object(ctx, *this);
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
        return make_reflected_object<OptionInfo>(std::move(object));
    }

    qjs::Object to_object(JSContext* ctx) const {
        return to_reflected_object(ctx, *this);
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

}  // namespace catter::js
