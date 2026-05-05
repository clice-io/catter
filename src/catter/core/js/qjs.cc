#include "qjs.h"

#include <utility>

namespace catter::qjs {

Exception::Exception(const std::string& details) : cpptrace::runtime_error(std::string(details)) {}

Exception::Exception(std::string&& details) : cpptrace::runtime_error(std::move(details)) {}

TypeException::TypeException(const std::string& details) :
    Exception(std::format("TypeError: {}", details)) {}

JSException::JSException(const Error& error) : Exception(error.format()) {}

JSException JSException::dump(JSContext* ctx) {
    return JSException(Error(ctx, JS_GetException(ctx)));
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

Value Object::get_property(const std::string& prop_name) const {
    auto ret = Value{this->context(),
                     JS_GetPropertyStr(this->context(), this->value(), prop_name.c_str())};
    if(ret.is_exception()) {
        throw qjs::JSException::dump(this->context());
    }
    return ret;
}

std::optional<Value> Object::get_optional_property(const std::string& prop_name) const noexcept {
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

Error::Error(JSContext* ctx, const JSValue& val) : Object(ctx, val) {}

Error::Error(JSContext* ctx, JSValue&& val) : Object(ctx, std::move(val)) {}

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

PromiseCapability Promise::create(JSContext* ctx) {
    JSValue funcs[2]{};
    JSValue promise = JS_NewPromiseCapability(ctx, funcs);

    if(JS_IsException(promise)) {
        throw JSException::dump(ctx);
    }

    return PromiseCapability{
        Promise{ctx, std::move(promise) },
        Value{ctx, std::move(funcs[0])},
        Value{ctx, std::move(funcs[1])}
    };
}

Promise Promise::from_value(Value&& value) {
    if(!JS_IsPromise(value.value())) {
        throw TypeException("Value is not a promise");
    }

    auto ctx = value.context();
    return Promise{ctx, value.release()};
}

bool Promise::is_pending() const {
    return JS_PromiseState(context(), value()) == JS_PROMISE_PENDING;
}

bool Promise::is_fulfilled() const {
    return JS_PromiseState(context(), value()) == JS_PROMISE_FULFILLED;
}

bool Promise::is_rejected() const {
    return JS_PromiseState(context(), value()) == JS_PROMISE_REJECTED;
}

Value Promise::result() const {
    return Value{context(), JS_PromiseResult(context(), value())};
}

Promise Promise::then_with_args(const qjs::Parameters& args) const {
    return this->call_promise_method("then", args);
}

Promise Promise::call_promise_method(const char* method_name, const qjs::Parameters& args) const {
    using Method = qjs::Function<qjs::Promise(qjs::Parameters)>;
    auto method = this->get_property(method_name).as<Method>();
    auto next = method.invoke(Object{context(), value()}, args);
    return next;
}

PromiseCapability::PromiseCapability(Promise promise,
                                     Value resolve_func,
                                     Value reject_func) noexcept :
    promise(std::move(promise)), resolve_func(std::move(resolve_func)),
    reject_func(std::move(reject_func)) {}

const Value& PromiseCapability::resolve_function() const noexcept {
    return resolve_func;
}

const Value& PromiseCapability::reject_function() const noexcept {
    return reject_func;
}

std::string format_rejection(Parameters& args) {
    if(args.empty()) {
        return "Promise rejected without a reason.\n";
    }

    auto format_value = [](Value& value) -> std::string {
        try {
            return value.as<Error>().format();
        } catch(const Exception&) {}

        try {
            return value.as<std::string>();
        } catch(const Exception&) {}

        try {
            auto ctx = value.context();
            auto json =
                Value{ctx, JS_JSONStringify(ctx, value.value(), JS_UNDEFINED, JS_UNDEFINED)};
            if(json.is_exception()) {
                throw JSException::dump(ctx);
            }
            return json.as<std::string>();
        } catch(const Exception&) {
            return "<non-string rejection value>";
        }
    };

    std::string trace;
    for(auto& arg: args) {
        trace += format_value(arg) + "\n";
    }
    return trace;
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

Value value_trans<Promise>::from(JSContext*, const Promise& value) noexcept {
    return from(value);
}

Value value_trans<Promise>::from(JSContext*, Promise&& value) noexcept {
    return from(std::move(value));
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

void Context::Raw::JSContextDeleter::operator() (JSContext* ctx) const noexcept {
    JS_FreeContext(ctx);
}

const CModule& Context::cmodule(const std::string& name) const {
    if(auto it = this->raw->modules.find(name); it != this->raw->modules.end()) {
        return it->second;
    }

    auto m = JS_NewCModule(this->js_context(), name.data(), [](JSContext* js_ctx, JSModuleDef* m) {
        auto* ctx = Context::get_opaque(js_ctx);
        auto atom = Atom(js_ctx, JS_GetModuleName(js_ctx, m));

        if(!ctx) {
            return -1;
        }

        auto& mod = ctx->modules[atom.to_string()];
        for(auto& kv: mod.exports_list()) {
            JS_SetModuleExport(js_ctx, m, kv.name.c_str(), kv.value.release());
        }
        return 0;
    });
    if(m == nullptr) {
        throw qjs::Exception("Failed to create new C module");
    }

    return this->raw->modules.emplace(name, CModule(this->js_context(), m, name)).first->second;
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
    return this->raw->ctx.get();
}

Context::operator bool() const noexcept {
    return this->raw != nullptr;
}

void Context::set_opaque() noexcept {
    JS_SetContextOpaque(this->js_context(), this->raw.get());
}

Context::Raw* Context::get_opaque(JSContext* ctx) noexcept {
    return static_cast<Raw*>(JS_GetContextOpaque(ctx));
}

Context::Context(JSContext* js_ctx) noexcept : raw(std::make_unique<Raw>(js_ctx)) {
    this->set_opaque();
}

Runtime::Raw::Raw(JSRuntime* rt) noexcept : rt(rt) {}

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

const Context& Runtime::context(const std::string& name) const {
    if(auto it = this->raw->ctxs.find(name); it != this->raw->ctxs.end()) {
        return it->second;
    }

    auto js_ctx = JS_NewContext(this->js_runtime());
    if(!js_ctx) {
        throw qjs::Exception("Failed to create new JS context");
    }
    return this->raw->ctxs.emplace(name, Context(js_ctx)).first->second;
}

JSRuntime* Runtime::js_runtime() const noexcept {
    return this->raw->rt.get();
}

Runtime::operator bool() const noexcept {
    return this->raw != nullptr;
}

Runtime::Runtime(JSRuntime* js_rt) : raw(std::make_unique<Raw>(js_rt)) {}

namespace json {

qjs::Value parse(const std::string& json_str, const Context& ctx) {
    auto ret = qjs::Value{
        ctx.js_context(),
        JS_ParseJSON(ctx.js_context(), json_str.data(), json_str.size(), "<json input>")};

    if(ret.is_exception()) {
        throw qjs::JSException::dump(ctx.js_context());
    }
    return ret;
}

}  // namespace json

}  // namespace catter::qjs
