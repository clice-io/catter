#include "qjs.h"

#include <atomic>
#include <cassert>
#include <cstdint>
#include <utility>
#include <quickjs.h>

namespace catter::qjs {

namespace {

void* next_runtime_token() noexcept {
    static std::atomic_uintptr_t next{1};
    return reinterpret_cast<void*>(next.fetch_add(1, std::memory_order_relaxed));
}

}  // namespace

Exception::Exception(const std::string& details) : cpptrace::runtime_error(std::string(details)) {}

Exception::Exception(std::string&& details) : cpptrace::runtime_error(std::move(details)) {}

TypeException::TypeException(const std::string& details) :
    Exception(std::format("TypeError: {}", details)) {}

JSException::JSException(const Error& error) : Exception(error.format()) {}

JSException JSException::dump(JSContext* ctx) {
    auto exception = Value{ctx, JS_GetException(ctx)};
    if(exception.is_error()) {
        return JSException(Error(ctx, exception.value()));
    } else {
        return JSException(
            Error::internal_error(ctx, "Non-error exception: {}", json::stringify(exception)));
    }
}

Value::Value(const Value& other) noexcept :
    ctx(other.ctx), val(other.ctx ? JS_DupValue(other.ctx, other.val) : JS_UNINITIALIZED) {}

Value::Value(Value&& other) noexcept :
    ctx(std::exchange(other.ctx, nullptr)), val(std::exchange(other.val, JS_UNINITIALIZED)) {}

Value& Value::operator= (const Value& other) noexcept {
    if(this != &other) {
        if(this->ctx) {
            JS_FreeValue(this->ctx, this->val);
        }
        this->ctx = other.ctx;
        this->val = other.ctx ? JS_DupValue(other.ctx, other.val) : JS_UNINITIALIZED;
    }
    return *this;
}

Value& Value::operator= (Value&& other) noexcept {
    if(this != &other) {
        if(this->ctx) {
            JS_FreeValue(this->ctx, this->val);
        }
        ctx = std::exchange(other.ctx, nullptr);
        val = std::exchange(other.val, JS_UNINITIALIZED);
    }
    return *this;
}

Value::~Value() noexcept {
    if(this->ctx) {
        JS_FreeValue(ctx, val);
    }
}

Value::Value(JSContext* ctx, const JSValue& val) noexcept : ctx(ctx), val(JS_DupValue(ctx, val)) {}

Value::Value(JSContext* ctx, JSValue&& val) noexcept : ctx(ctx), val(std::move(val)) {}

Value Value::undefined(JSContext* ctx) noexcept {
    return Value{ctx, JS_UNDEFINED};
}

Value Value::null(JSContext* ctx) noexcept {
    return Value{ctx, JS_NULL};
}

bool Value::is_object() const noexcept {
    return JS_IsObject(this->val);
}

bool Value::is_error() const noexcept {
    return JS_IsError(this->val);
}

bool Value::is_promise() const noexcept {
    return JS_IsPromise(this->val);
}

bool Value::is_function() const noexcept {
    return JS_IsFunction(this->ctx, this->val);
}

bool Value::is_exception() const noexcept {
    return JS_IsException(this->val);
}

bool Value::is_undefined() const noexcept {
    return JS_IsUndefined(this->val);
}

bool Value::is_null() const noexcept {
    return JS_IsNull(this->val);
}

bool Value::is_nothing() const noexcept {
    return this->is_null() || this->is_undefined();
}

bool Value::is_valid() const noexcept {
    return this->ctx != nullptr;
}

Value::operator bool() const noexcept {
    return this->is_valid();
}

const JSValue& Value::value() const noexcept {
    return this->val;
}

JSValue Value::release() noexcept {
    JSValue temp = this->val;
    this->val = JS_UNINITIALIZED;
    this->ctx = nullptr;
    return temp;
}

JSContext* Value::context() const noexcept {
    return this->ctx;
}

Atom::Atom(JSContext* ctx, const JSAtom& atom) noexcept : ctx(ctx), atom(JS_DupAtom(ctx, atom)) {}

Atom::Atom(JSContext* ctx, JSAtom&& atom) noexcept : ctx(ctx), atom(std::move(atom)) {}

Atom::Atom(const Atom& other) noexcept :
    ctx(other.ctx), atom(other.ctx ? JS_DupAtom(other.ctx, other.atom) : JS_ATOM_NULL) {}

Atom::Atom(Atom&& other) noexcept :
    ctx(std::exchange(other.ctx, nullptr)), atom(std::exchange(other.atom, JS_ATOM_NULL)) {}

Atom& Atom::operator= (const Atom& other) noexcept {
    if(this != &other) {
        if(this->ctx) {
            JS_FreeAtom(this->ctx, this->atom);
        }
        this->ctx = other.ctx;
        this->atom = other.ctx ? JS_DupAtom(other.ctx, other.atom) : JS_ATOM_NULL;
    }
    return *this;
}

Atom& Atom::operator= (Atom&& other) noexcept {
    if(this != &other) {
        if(this->ctx) {
            JS_FreeAtom(this->ctx, this->atom);
        }
        ctx = std::exchange(other.ctx, nullptr);
        atom = std::exchange(other.atom, JS_ATOM_NULL);
    }
    return *this;
}

Atom::~Atom() noexcept {
    if(this->ctx) {
        JS_FreeAtom(this->ctx, this->atom);
    }
}

JSAtom Atom::value() const noexcept {
    return this->atom;
}

std::string Atom::to_string() const noexcept {
    const char* str = JS_AtomToCString(this->ctx, this->atom);
    if(str == nullptr) {
        return {};
    }
    std::string result{str};
    JS_FreeCString(this->ctx, str);
    return result;
}

Value Object::get_property(const char* prop_name) const {
    auto ret = Value{this->context(), JS_GetPropertyStr(this->context(), this->value(), prop_name)};
    if(JS_HasException(this->context())) {
        throw qjs::JSException::dump(this->context());
    }
    return ret;
}

std::optional<Value> Object::get_optional_property(const char* prop_name) const noexcept {
    try {
        if(auto ret = get_property(prop_name); ret.is_undefined()) {
            return std::nullopt;
        } else {
            return ret;
        }
    } catch(const qjs::Exception&) {
        return std::nullopt;
    }
}

Object Object::empty_one(JSContext* ctx) noexcept {
    return Object{ctx, JS_NewObject(ctx)};
}

std::string Error::message() const {
    return this->get_property("message").as<std::string>();
}

std::string Error::stack() const {
    return this->get_property("stack").as<std::string>();
}

std::string Error::name() const {
    return this->get_property("name").as<std::string>();
}

std::string Error::format() const {
    return std::format("{}: {}\nStack Trace:\n{}", this->name(), this->message(), this->stack());
}

namespace detail {

std::vector<JSValueConst> make_argv_view(const Parameters& params) {
    std::vector<JSValueConst> argv;
    argv.reserve(params.size());
    for(const auto& param: params) {
        if(!param.is_valid()) {
            throw TypeException("Function argument contains an invalid value");
        }
        argv.push_back(param.value());
    }
    return argv;
}

}  // namespace detail

bool Promise::is_pending() const {
    return this->state() == JS_PROMISE_PENDING;
}

bool Promise::is_fulfilled() const {
    return this->state() == JS_PROMISE_FULFILLED;
}

bool Promise::is_rejected() const {
    return this->state() == JS_PROMISE_REJECTED;
}

Value Promise::result() const {
    return Value{context(), JS_PromiseResult(context(), value())};
}

namespace detail {

Value value_trans<bool>::from(JSContext* ctx, bool value) noexcept {
    return Value{ctx, JS_NewBool(ctx, value)};
}

bool value_trans<bool>::as(const Value& val) {
    if(!JS_IsBool(val.value())) {
        throw TypeException("Value is not a boolean");
    }
    return JS_ToBool(val.context(), val.value());
}

std::optional<bool> value_trans<bool>::to(const Value& val) noexcept {
    try {
        return as(val);
    } catch(const TypeException&) {
        return std::nullopt;
    }
}

Value value_trans<std::string>::from(JSContext* ctx, const std::string& value) noexcept {
    return Value{ctx, JS_NewStringLen(ctx, value.data(), value.size())};
}

std::string value_trans<std::string>::as(const Value& val) {
    if(!JS_IsString(val.value())) {
        throw TypeException("Value is not a string");
    }
    size_t len;
    const char* str = JS_ToCStringLen(val.context(), &len, val.value());
    if(str == nullptr) {
        throw TypeException("Failed to convert value to string");
    }
    std::string result{str, len};
    JS_FreeCString(val.context(), str);
    return result;
}

std::optional<std::string> value_trans<std::string>::to(const Value& val) noexcept {
    try {
        return as(val);
    } catch(const TypeException&) {
        return std::nullopt;
    }
}

Value value_trans<Object>::from(const Object& value) noexcept {
    return Value{value.context(), value.value()};
}

Value value_trans<Object>::from(Object&& value) noexcept {
    auto ctx = value.context();
    return Value{ctx, value.release()};
}

Object value_trans<Object>::as(const Value& val) {
    if(!JS_IsObject(val.value())) {
        throw TypeException("Value is not an object");
    }
    return Object{val.context(), val.value()};
}

std::optional<Object> value_trans<Object>::to(const Value& val) noexcept {
    try {
        return as(val);
    } catch(const TypeException&) {
        return std::nullopt;
    }
}

Value value_trans<Error>::from(const Error& value) noexcept {
    return Value{value.context(), value.value()};
}

Value value_trans<Error>::from(Error&& value) noexcept {
    auto ctx = value.context();
    return Value{ctx, value.release()};
}

Error value_trans<Error>::as(const Value& val) {
    return val.as<Object>().as<Error>();
}

std::optional<Error> value_trans<Error>::to(const Value& val) noexcept {
    try {
        return as(val);
    } catch(const TypeException&) {
        return std::nullopt;
    }
}

Value value_trans<Promise>::from(const Promise& value) noexcept {
    return Value{value.context(), value.value()};
}

Value value_trans<Promise>::from(Promise&& value) noexcept {
    auto ctx = value.context();
    return Value{ctx, value.release()};
}

Promise value_trans<Promise>::as(const Value& val) {
    auto obj = val.as<Object>();
    if(!JS_IsPromise(obj.value())) {
        throw TypeException("Value is not a promise");
    }
    return Promise{obj.context(), obj.value()};
}

std::optional<Promise> value_trans<Promise>::to(const Value& val) noexcept {
    try {
        return as(val);
    } catch(const TypeException&) {
        return std::nullopt;
    }
}

Object object_trans<Error>::from(const Error& value) noexcept {
    return Object{value.context(), value.value()};
}

Object object_trans<Error>::from(Error&& value) noexcept {
    auto ctx = value.context();
    return Object{ctx, value.release()};
}

Error object_trans<Error>::as(const Object& obj) {
    if(!JS_IsError(obj.value())) {
        throw TypeException("Object is not an error");
    }
    return Error{obj.context(), obj.value()};
}

std::optional<Error> object_trans<Error>::to(const Object& obj) noexcept {
    try {
        return as(obj);
    } catch(const TypeException&) {
        return std::nullopt;
    }
}

Object object_trans<Promise>::from(const Promise& value) noexcept {
    return Object{value.context(), value.value()};
}

Object object_trans<Promise>::from(Promise&& value) noexcept {
    auto ctx = value.context();
    return Object{ctx, value.release()};
}

Promise object_trans<Promise>::as(const Object& obj) {
    if(!JS_IsPromise(obj.value())) {
        throw TypeException("Object is not a promise");
    }
    return Promise{obj.context(), obj.value()};
}

std::optional<Promise> object_trans<Promise>::to(const Object& obj) noexcept {
    try {
        return as(obj);
    } catch(const TypeException&) {
        return std::nullopt;
    }
}

}  // namespace detail

const CModule& CModule::export_bare_functor(const std::string& export_name,
                                            JSCFunction func,
                                            int argc) const {
    this->exports_list().push_back(kv{
        export_name,
        Value{this->ctx, JS_NewCFunction(this->ctx, func, export_name.c_str(), argc)}
    });
    if(JS_AddModuleExport(this->ctx, m, export_name.c_str()) < 0) {
        throw qjs::Exception("Failed to add export '{}' to module '{}'", export_name, this->name);
    }
    return *this;
}

const CModule& CModule::export_functor_value(const std::string& export_name,
                                             JSValueConst value) const {
    this->exports_list().push_back(kv{
        export_name,
        Value{this->ctx, value}
    });
    if(JS_AddModuleExport(this->ctx, m, export_name.c_str()) < 0) {
        throw qjs::Exception("Failed to add export '{}' to module '{}'", export_name, this->name);
    }
    return *this;
}

CModule::CModule(JSContext* ctx, JSModuleDef* m, const std::string& name) noexcept :
    ctx(ctx), m(m), name(name) {}

std::vector<CModule::kv>& CModule::exports_list() const noexcept {
    return *this->exports;
}

Context::Raw::Raw(JSContext* ctx) : ctx(ctx) {}

std::unique_ptr<Context::Raw> Context::Raw::create(JSContext* ctx) noexcept {
    auto ret = std::make_unique<Raw>(ctx);
    JS_SetContextOpaque(ctx, ret.get());
    return ret;
}

Context::Raw* Context::Raw::from(JSContext* ctx) noexcept {
    return static_cast<Context::Raw*>(JS_GetContextOpaque(ctx));
}

void Context::Raw::JSContextDeleter::operator() (JSContext* ctx) const noexcept {
    JS_FreeContext(ctx);
}

const CModule& Context::cmodule(const std::string& name) const {
    auto raw = Raw::from(this->js_context());
    assert(raw != nullptr && "Raw context should not be null when creating a C module");

    if(auto it = raw->modules.find(name); it != raw->modules.end()) {
        return it->second;
    }

    auto m = JS_NewCModule(this->js_context(), name.data(), [](JSContext* js_ctx, JSModuleDef* m) {
        auto* raw = Raw::from(js_ctx);
        assert(raw != nullptr && "Raw context should not be null when creating a C module");

        auto atom = Atom(js_ctx, JS_GetModuleName(js_ctx, m));
        auto& mod = raw->modules[atom.to_string()];
        for(auto& kv: mod.exports_list()) {
            JS_SetModuleExport(js_ctx, m, kv.name.c_str(), kv.value.release());
        }
        return 0;
    });
    if(m == nullptr) {
        throw qjs::Exception("Failed to create new C module");
    }

    return raw->modules.emplace(name, CModule(this->js_context(), m, name)).first->second;
}

Value
    Context::eval(const char* input, size_t input_len, const char* filename, int eval_flags) const {
    auto val = JS_Eval(this->js_context(), input, input_len, filename, eval_flags);

    if(this->has_exception()) {
        JS_FreeValue(this->js_context(), val);
        throw qjs::JSException::dump(this->js_context());
    }
    return Value{this->js_context(), std::move(val)};
}

Value Context::eval(std::string_view input, const char* filename, int eval_flags) const {
    return this->eval(input.data(), input.size(), filename, eval_flags);
}

Object Context::global_this() const noexcept {
    return Object{this->js_context(), JS_GetGlobalObject(this->js_context())};
}

bool Context::has_exception() const noexcept {
    return JS_HasException(this->js_context());
}

JSContext* Context::js_context() const noexcept {
    return this->ctx;
}

Context::operator bool() const noexcept {
    return this->ctx != nullptr;
}

Context::Context(JSContext* ctx) noexcept : ctx(ctx) {}

Runtime::Raw::Raw(JSRuntime* rt) noexcept : rt(rt) {
    JS_SetRuntimeOpaque(rt, next_runtime_token());
}

void Runtime::Raw::JSRuntimeDeleter::operator() (JSRuntime* rt) const noexcept {
    JS_FreeRuntime(rt);
}

Runtime Runtime::create() {
    auto js_rt = JS_NewRuntime();
    if(!js_rt) {
        throw qjs::Exception("Failed to create new JS runtime");
    }
    return Runtime(js_rt);
}

Context Runtime::context(const std::string& name) const {
    if(auto it = this->raw->ctxs.find(name); it != this->raw->ctxs.end()) {
        return {it->second->ctx.get()};
    }

    auto js_ctx = JS_NewContext(this->js_runtime());
    if(!js_ctx) {
        throw qjs::Exception("Failed to create new JS context");
    }
    return this->raw->ctxs.emplace(name, Context::Raw::create(js_ctx)).first->second->ctx.get();
}

JSRuntime* Runtime::js_runtime() const noexcept {
    return this->raw->rt.get();
}

void Runtime::set_module_loader(std::unique_ptr<ModuleLoader> loader) const noexcept {
    this->raw->module_loader = std::move(loader);

    return JS_SetModuleLoaderFunc(
        this->js_runtime(),
        [](JSContext* ctx, const char* module_base_name, const char* module_name, void* opaque)
            -> char* {
            auto raw = static_cast<Raw*>(opaque);
            assert(raw && raw->module_loader && "Module loader is not set");
            try {
                auto normalized_name =
                    raw->module_loader->normalizer(module_base_name, module_name);

                return js_strdup(ctx, normalized_name.c_str());
            } catch(const std::exception& e) {
                JS_ThrowInternalError(ctx, "Exception in module normalizer: %s", e.what());
                return nullptr;
            } catch(...) {
                JS_ThrowInternalError(ctx, "Unknown exception in module normalizer");
                return nullptr;
            }
        },
        [](JSContext* js_ctx, const char* module_name, void* opaque) -> JSModuleDef* {
            auto raw = static_cast<Raw*>(opaque);
            assert(raw && raw->module_loader && "Module loader is not set");

            try {
                auto source = raw->module_loader->loader(module_name);

                auto module_value = Value{js_ctx,
                                          JS_Eval(js_ctx,
                                                  source.c_str(),
                                                  source.size(),
                                                  module_name,
                                                  JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY)};

                if(module_value.is_exception()) {
                    return nullptr;
                }

                return (JSModuleDef*)JS_VALUE_GET_PTR(module_value.value());
            } catch(const std::exception& e) {
                JS_ThrowInternalError(js_ctx, "Exception in module loader: %s", e.what());
                return nullptr;
            } catch(...) {
                JS_ThrowInternalError(js_ctx, "Unknown exception in module loader");
                return nullptr;
            }
        },
        this->raw.get());
}

std::expected<bool, Value> Runtime::execute_pending_job() const noexcept {
    JSContext* ctx = nullptr;
    switch(JS_ExecutePendingJob(this->js_runtime(), &ctx)) {
        case 1: return true;
        case 0: return false;
        case -1:
            if(ctx) {
                return std::unexpected(Value{ctx, JS_GetException(ctx)});
            } else {
                return std::unexpected(Value{});
            }
        default: return std::unexpected(Value{});
    }
}

Runtime::operator bool() const noexcept {
    return this->raw != nullptr;
}

Runtime::Runtime(JSRuntime* js_rt) : raw(std::make_unique<Raw>(js_rt)) {}

namespace json {
std::string stringify(qjs::Value v) {
    auto ctx = v.context();
    auto val = v.value();
    auto json_str_val = qjs::Value{ctx, JS_JSONStringify(ctx, val, JS_UNDEFINED, JS_UNDEFINED)};
    if(JS_HasException(ctx)) {
        throw qjs::JSException::dump(ctx);
    }

    const char* json_cstr = JS_ToCString(ctx, json_str_val.value());
    if(json_cstr) {
        std::string result{json_cstr};
        JS_FreeCString(ctx, json_cstr);
        return result;
    }
    throw qjs::TypeException("Failed to convert value to JSON string");
};

qjs::Value parse(const std::string& json_str, const Context& ctx) {
    auto ret = qjs::Value{
        ctx.js_context(),
        JS_ParseJSON(ctx.js_context(), json_str.data(), json_str.size(), "<json input>")};

    if(ctx.has_exception()) {
        throw qjs::JSException::dump(ctx.js_context());
    }
    return ret;
}

}  // namespace json

}  // namespace catter::qjs
