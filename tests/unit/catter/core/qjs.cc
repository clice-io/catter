#include "js/qjs.h"

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <cpptrace/exceptions.hpp>
#include <kota/zest/macro.h>
#include <kota/zest/zest.h>
#include <kota/async/async.h>

#include "js/async.h"

using namespace catter;

namespace {

template <typename Fn>
bool throws_with_message(Fn&& fn, std::string_view needle) {
    try {
        fn();
    } catch(const qjs::Exception& e) {
        return std::string_view(e.what()).contains(needle);
    } catch(...) {
        return false;
    }
    return false;
}

int64_t add_one_raw(int64_t value) {
    return value + 1;
}

int64_t add_two_with_ctx(JSContext* ctx, int64_t value) {
    return ctx ? value + 2 : value;
}

int64_t sum_all_raw(qjs::Parameters args) {
    int64_t sum = 0;
    for(auto& arg: args) {
        sum += arg.as<int64_t>();
    }
    return sum;
}

int64_t count_args_with_ctx(JSContext* ctx, qjs::Parameters args) {
    return ctx ? static_cast<int64_t>(args.size()) : -1;
}

kota::task<std::string> async_greet(std::string name) {
    co_return "hello " + name;
}

kota::task<int64_t, std::string> async_add_or_fail(int64_t value) {
    if(value < 0) {
        co_await kota::fail(std::string{"negative value"});
    }

    co_return value + 1;
}

struct IncrementFunctor {
    int64_t delta = 0;

    int64_t operator() (int64_t value) const {
        return value + delta;
    }
};

struct CountingFunctor {
    int64_t calls = 0;

    int64_t operator() (int64_t value) {
        ++calls;
        return value + calls;
    }
};

JSValue forty_two_raw(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewInt64(ctx, 42);
}

void drain_jobs(JSRuntime* rt) {
    JSContext* job_ctx = nullptr;
    while(JS_IsJobPending(rt)) {
        int ret = JS_ExecutePendingJob(rt, &job_ctx);
        if(ret < 0) {
            throw qjs::JSException::dump(job_ctx);
        }
    }
}

constexpr int eval_flags = JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_STRICT;

}  // namespace

TEST_SUITE(qjs_tests) {

TEST_CASE(runtime_context_and_eval_cover_success_and_error_paths) {
    auto f = [&]() {
        auto runtime = qjs::Runtime::create();
        EXPECT_TRUE(runtime);

        auto& default_ctx = runtime.context();
        auto& same_ctx = runtime.context();
        auto& other_ctx = runtime.context("other");

        EXPECT_TRUE(default_ctx.js_context() == same_ctx.js_context());
        EXPECT_TRUE(default_ctx.js_context() != other_ctx.js_context());

        auto value = default_ctx.eval("1 + 2", "<eval>", eval_flags);
        EXPECT_TRUE(value.as<int64_t>() == 3);

        auto undefined_value = default_ctx.eval("undefined", "<eval>", eval_flags);
        EXPECT_TRUE(undefined_value.is_undefined());
        EXPECT_TRUE(undefined_value.is_nothing());

        auto null_value = default_ctx.eval("null", "<eval>", eval_flags);
        EXPECT_TRUE(null_value.is_null());
        EXPECT_TRUE(null_value.is_nothing());

        auto global = default_ctx.global_this();
        global.set_property("from_cpp", int64_t{9});
        EXPECT_TRUE(global.get_property("from_cpp").as<int64_t>() == 9);

        auto wrapped_global = qjs::Value::from(global);
        EXPECT_TRUE(wrapped_global.as<qjs::Object>().get_property("from_cpp").as<int64_t>() == 9);

        auto atom =
            qjs::Atom(default_ctx.js_context(), JS_NewAtom(default_ctx.js_context(), "qjs-test"));
        EXPECT_TRUE(atom.to_string() == "qjs-test");
    };

    EXPECT_NOTHROWS(f());

    auto runtime = qjs::Runtime::create();
    auto& ctx = runtime.context();
    EXPECT_TRUE(throws_with_message(
        [&]() { ctx.eval("throw new TypeError('boom')", "<eval>", eval_flags); },
        "TypeError"));
    EXPECT_TRUE(!ctx.has_exception());
};

TEST_CASE(value_conversions_cover_supported_types_copy_move_and_type_mismatch) {
    auto f = [&]() {
        auto runtime = qjs::Runtime::create();
        auto& ctx = runtime.context();

        auto bool_value = qjs::Value::from(ctx.js_context(), true);
        auto signed_value = qjs::Value::from(ctx.js_context(), int64_t{-7});
        auto unsigned_value = qjs::Value::from(ctx.js_context(), uint32_t{11});
        auto string_value = qjs::Value::from(ctx.js_context(), std::string{"hello"});

        EXPECT_TRUE(bool_value.to<bool>().value_or(false));
        EXPECT_TRUE(signed_value.as<int64_t>() == -7);
        EXPECT_TRUE(unsigned_value.as<uint32_t>() == 11);
        EXPECT_TRUE(string_value.as<std::string>() == "hello");

        auto copied_string = string_value;
        EXPECT_TRUE(copied_string.as<std::string>() == "hello");

        auto moved_string = std::move(copied_string);
        EXPECT_TRUE(!copied_string.is_valid());
        EXPECT_TRUE(moved_string.as<std::string>() == "hello");

        auto released = qjs::Value::from(ctx.js_context(), int64_t{27});
        auto raw_value = released.release();
        EXPECT_TRUE(!released.is_valid());
        auto rebound = qjs::Value(ctx.js_context(), std::move(raw_value));
        EXPECT_TRUE(rebound.as<int64_t>() == 27);
    };

    EXPECT_NOTHROWS(f());

    auto runtime = qjs::Runtime::create();
    auto& ctx = runtime.context();
    auto bool_value = qjs::Value::from(ctx.js_context(), true);
    EXPECT_FALSE(bool_value.to<std::string>().has_value());
    EXPECT_TRUE(throws_with_message([&]() { (void)bool_value.as<std::string>(); },
                                    "Value is not a string"));
};

TEST_CASE(object_property_apis_cover_reads_writes_and_exceptional_access) {
    auto f = [&]() {
        auto runtime = qjs::Runtime::create();
        auto& ctx = runtime.context();

        auto object = qjs::json::parse(R"({"existing":1})", ctx).as<qjs::Object>();

        object.set_property("number", int64_t{42});
        object.set_property("flag", true);

        auto wrapped = qjs::Value::from(ctx.js_context(), std::string{"wrapped"});
        object.set_property("wrapped", wrapped);

        auto raw_js_value = JS_NewInt64(ctx.js_context(), 99);
        object.set_property("raw", raw_js_value);
        JS_FreeValue(ctx.js_context(), raw_js_value);

        EXPECT_TRUE(object.get_property("number").as<int64_t>() == 42);
        EXPECT_TRUE(object["flag"].as<bool>());
        EXPECT_TRUE(object.get_property("wrapped").as<std::string>() == "wrapped");
        EXPECT_TRUE(object.get_property("raw").as<int64_t>() == 99);
        EXPECT_FALSE(object.get_optional_property("missing").has_value());

        auto same_object = object;
        EXPECT_TRUE(same_object.get_property("number").as<int64_t>() == 42);
    };

    EXPECT_NOTHROWS(f());

    auto runtime = qjs::Runtime::create();
    auto& ctx = runtime.context();
    auto throwing_object =
        ctx.eval(
               "Object.defineProperty({}, 'boom', { get() { throw new Error('property boom'); } })",
               "<eval>",
               eval_flags)
            .as<qjs::Object>();

    EXPECT_TRUE(throws_with_message([&]() { (void)throwing_object.get_property("boom"); },
                                    "property boom"));
    EXPECT_FALSE(throwing_object.get_optional_property("boom").has_value());
    JS_FreeValue(ctx.js_context(), JS_GetException(ctx.js_context()));
};

TEST_CASE(array_conversions_cover_roundtrip_element_failures_and_push_errors) {
    auto f = [&]() {
        auto runtime = qjs::Runtime::create();
        auto& ctx = runtime.context();

        auto values = std::vector<int64_t>{1, 2, 3};
        auto array = qjs::Array<int64_t>::from(ctx.js_context(), values);

        EXPECT_TRUE(array.length() == 3);
        EXPECT_TRUE(array[0] == 1);
        EXPECT_TRUE(array.as<std::vector<int64_t>>() == values);

        auto array_value = qjs::Value::from(array);
        EXPECT_TRUE(array_value.as<qjs::Array<int64_t>>().length() == 3);
        EXPECT_TRUE(array_value.to<qjs::Array<int64_t>>().has_value());

        auto array_object = qjs::Object::from(array);
        EXPECT_TRUE(array_object.as<qjs::Array<int64_t>>().length() == 3);

        auto empty = qjs::Array<int64_t>::empty_one(ctx.js_context());
        empty.push(7);
        EXPECT_TRUE(empty.length() == 1);
        EXPECT_TRUE(empty[0] == 7);
    };

    EXPECT_NOTHROWS(f());

    auto runtime = qjs::Runtime::create();
    auto& ctx = runtime.context();

    auto not_an_array = qjs::json::parse(R"({"length":1})", ctx).as<qjs::Object>();
    EXPECT_TRUE(throws_with_message([&]() { (void)not_an_array.as<qjs::Array<int64_t>>(); },
                                    "Object is not an array"));

    auto frozen_array =
        ctx.eval("const arr = []; Object.preventExtensions(arr); arr", "<eval>", eval_flags)
            .as<qjs::Object>()
            .as<qjs::Array<int64_t>>();
    EXPECT_TRUE(throws_with_message([&]() { frozen_array.push(1); }, "TypeError"));
};

TEST_CASE(array_conversions_cover_string_roundtrip) {
    auto f = [&]() {
        auto runtime = qjs::Runtime::create();
        auto& ctx = runtime.context();

        auto string_values = std::vector<std::string>{"alpha", "beta", "gamma"};
        auto string_array = qjs::Array<std::string>::from(ctx.js_context(), string_values);
        EXPECT_TRUE(string_array.length() == 3);
        EXPECT_TRUE(string_array[0] == "alpha");
        EXPECT_TRUE(string_array[2] == "gamma");
        EXPECT_TRUE(string_array.as<std::vector<std::string>>() == string_values);

        auto string_array_object = qjs::Object::from(string_array);
        EXPECT_TRUE(string_array_object.to<qjs::Array<std::string>>().has_value());
    };

    EXPECT_NOTHROWS(f());
};

TEST_CASE(function_wrappers_cover_cpp_js_and_raw_function_invocation) {
    auto f = [&]() {
        auto runtime = qjs::Runtime::create();
        auto& ctx = runtime.context();
        int64_t remembered = 0;

        auto plus_three =
            qjs::Function<int64_t(int64_t)>::from(ctx.js_context(),
                                                  [](int64_t value) { return value + 3; });
        auto read_value =
            qjs::Function<int64_t(qjs::Object)>::from(ctx.js_context(), [](qjs::Object object) {
                return object.get_property("value").as<int64_t>();
            });
        auto remember_value =
            qjs::Function<void(int64_t)>::from(ctx.js_context(), [&remembered](int64_t value) {
                remembered = value;
            });
        auto make_object = qjs::Function<qjs::Object()>::from(ctx.js_context(), [&ctx]() {
            return ctx.eval("({ kind: 'native-object' })", "<eval>", eval_flags).as<qjs::Object>();
        });
        auto add_one =
            qjs::Function<int64_t(int64_t)>::from_raw<&add_one_raw>(ctx.js_context(), "addOne");
        auto add_two =
            qjs::Function<int64_t(int64_t)>::from_raw<&add_two_with_ctx>(ctx.js_context(),
                                                                         "addTwo");

        ctx.global_this().set_property("plusThree", plus_three);
        ctx.global_this().set_property("readValue", read_value);
        ctx.global_this().set_property("rememberValue", remember_value);
        ctx.global_this().set_property("makeObject", make_object);
        ctx.global_this().set_property("addOne", add_one);
        ctx.global_this().set_property("addTwo", add_two);

        EXPECT_TRUE(plus_three(4) == 7);
        EXPECT_TRUE(plus_three.as()(5) == 8);
        EXPECT_TRUE(plus_three.value().u.ptr != nullptr);
        EXPECT_TRUE(add_one(9) == 10);
        EXPECT_TRUE(add_two(9) == 11);

        auto object_arg = qjs::json::parse(R"({"value":33})", ctx).as<qjs::Object>();
        EXPECT_TRUE(object_arg.get_property("value").as<int64_t>() == 33);
        EXPECT_TRUE(ctx.eval("readValue({ value: 44 })", "<eval>", eval_flags).as<int64_t>() == 44);

        remember_value(21);
        EXPECT_TRUE(remembered == 21);

        ctx.eval("rememberValue(34)", "<eval>", eval_flags);
        EXPECT_TRUE(remembered == 34);

        EXPECT_TRUE(make_object().get_property("kind").as<std::string>() == "native-object");
        EXPECT_TRUE(ctx.eval("makeObject().kind", "<eval>", eval_flags).as<std::string>() ==
                    "native-object");

        auto js_function =
            ctx.eval("(function (value) { return value * 2; })", "<eval>", eval_flags)
                .as<qjs::Object>()
                .as<qjs::Function<int64_t(int64_t)>>();
        EXPECT_TRUE(js_function(6) == 12);
        EXPECT_TRUE(js_function.as()(7) == 14);

        auto function_value = qjs::Value::from(js_function);
        EXPECT_TRUE(function_value.as<qjs::Function<int64_t(int64_t)>>()(8) == 16);
        EXPECT_TRUE(function_value.is_object());
        EXPECT_TRUE(function_value.is_function());
    };

    EXPECT_NOTHROWS(f());

    auto runtime = qjs::Runtime::create();
    auto& ctx = runtime.context();
    auto zero_arg_js_function =
        ctx.eval("(function () { return 7; })", "<eval>", eval_flags).as<qjs::Object>();
    EXPECT_TRUE(throws_with_message(
        [&]() { (void)zero_arg_js_function.as<qjs::Function<std::string()>>()(); },
        "Value is not a string"));

    auto single_arg_js_function =
        ctx.eval("(function (value) { return value; })", "<eval>", eval_flags).as<qjs::Object>();
    EXPECT_TRUE(throws_with_message(
        [&]() { (void)single_arg_js_function.as<qjs::Function<int64_t(int64_t, int64_t)>>(); },
        "incorrect number of arguments"));

    auto plain_object = qjs::json::parse(R"({"value":1})", ctx).as<qjs::Object>();
    EXPECT_TRUE(throws_with_message([&]() { (void)plain_object.as<qjs::Function<int64_t()>>(); },
                                    "Object is not a function"));
};

TEST_CASE(function_wrappers_surface_argument_and_exception_failures) {
    auto runtime = qjs::Runtime::create();
    auto& ctx = runtime.context();

    auto expect_number = qjs::Function<int64_t(int64_t)>::from(ctx.js_context(),
                                                               [](int64_t value) { return value; });
    auto qjs_thrower = qjs::Function<int64_t()>::from(ctx.js_context(), []() -> int64_t {
        throw qjs::Exception("custom boom");
    });
    auto std_thrower = qjs::Function<int64_t()>::from(ctx.js_context(), []() -> int64_t {
        throw cpptrace::runtime_error("std boom");
    });

    auto global = ctx.global_this();
    global.set_property("expectNumber", expect_number);
    global.set_property("qjsThrower", qjs_thrower);
    global.set_property("stdThrower", std_thrower);

    EXPECT_TRUE(
        throws_with_message([&]() { ctx.eval("expectNumber('bad')", "<eval>", eval_flags); },
                            "Value is not a number"));
    EXPECT_TRUE(throws_with_message([&]() { ctx.eval("expectNumber()", "<eval>", eval_flags); },
                                    "Incorrect number of arguments"));
    EXPECT_TRUE(throws_with_message([&]() { ctx.eval("qjsThrower()", "<eval>", eval_flags); },
                                    "Exception in C++ function: custom boom"));
    EXPECT_TRUE(throws_with_message([&]() { ctx.eval("stdThrower()", "<eval>", eval_flags); },
                                    "Unexpected exception: std boom"));
};

TEST_CASE(function_wrappers_cover_lvalue_functor_storage) {
    auto runtime = qjs::Runtime::create();
    auto& ctx = runtime.context();

    CountingFunctor counting_functor{};
    auto lvalue_functor = qjs::Function<int64_t(int64_t)>::from(ctx.js_context(), counting_functor);

    auto global = ctx.global_this();
    global.set_property("countingFunctor", lvalue_functor);

    EXPECT_TRUE(lvalue_functor(10) == 11);
    EXPECT_TRUE(counting_functor.calls == 1);
    EXPECT_TRUE(ctx.eval("countingFunctor(10)", "<eval>", eval_flags).as<int64_t>() == 12);
    EXPECT_TRUE(counting_functor.calls == 2);

    using Register = qjs::Object::Register<CountingFunctor&>;
    EXPECT_TRUE(Register::get(runtime.js_runtime()) != JS_INVALID_CLASS_ID);
};

TEST_CASE(function_wrappers_cover_variadic_parameter_list_api) {
    auto f = [&]() {
        auto runtime = qjs::Runtime::create();
        auto& ctx = runtime.context();

        auto sum_all = qjs::Function<int64_t(qjs::Parameters)>::from(ctx.js_context(),
                                                                     [](qjs::Parameters args) {
                                                                         int64_t sum = 0;
                                                                         for(auto& arg: args) {
                                                                             sum +=
                                                                                 arg.as<int64_t>();
                                                                         }
                                                                         return sum;
                                                                     });
        auto join_all =
            qjs::Function<std::string(qjs::Parameters)>::from(ctx.js_context(),
                                                              [](qjs::Parameters args) {
                                                                  std::string out;
                                                                  for(auto& arg: args) {
                                                                      out += arg.as<std::string>();
                                                                  }
                                                                  return out;
                                                              });
        auto sum_raw =
            qjs::Function<int64_t(qjs::Parameters)>::from_raw<&sum_all_raw>(ctx.js_context(),
                                                                            "sumRaw");
        auto count_raw = qjs::Function<int64_t(qjs::Parameters)>::from_raw<&count_args_with_ctx>(
            ctx.js_context(),
            "countRaw");

        auto global = ctx.global_this();
        global.set_property("sumAll", sum_all);
        global.set_property("joinAll", join_all);
        global.set_property("sumRaw", sum_raw);
        global.set_property("countRaw", count_raw);

        qjs::Parameters cpp_args{};
        cpp_args.push_back(qjs::Value::from(ctx.js_context(), int64_t{2}));
        cpp_args.push_back(qjs::Value::from(ctx.js_context(), int64_t{3}));
        cpp_args.push_back(qjs::Value::from(ctx.js_context(), int64_t{4}));

        EXPECT_TRUE(sum_all(cpp_args) == 9);
        EXPECT_TRUE(sum_all.as()(cpp_args) == 9);
        EXPECT_TRUE(sum_raw(cpp_args) == 9);
        EXPECT_TRUE(count_raw(cpp_args) == 3);

        EXPECT_TRUE(ctx.eval("sumAll()", "<eval>", eval_flags).as<int64_t>() == 0);
        EXPECT_TRUE(ctx.eval("sumAll(1, 2, 3, 4)", "<eval>", eval_flags).as<int64_t>() == 10);
        EXPECT_TRUE(ctx.eval("sumRaw(5, 6, 7)", "<eval>", eval_flags).as<int64_t>() == 18);
        EXPECT_TRUE(ctx.eval("countRaw(1, 2, 3, 4)", "<eval>", eval_flags).as<int64_t>() == 4);
        EXPECT_TRUE(ctx.eval("joinAll('cat', 'ter')", "<eval>", eval_flags).as<std::string>() ==
                    "catter");

        auto js_variadic =
            ctx.eval("(function (...values) { return values.length; })", "<eval>", eval_flags)
                .as<qjs::Object>()
                .as<qjs::Function<int64_t(qjs::Parameters)>>();
        EXPECT_TRUE(js_variadic({}) == 0);
        EXPECT_TRUE(js_variadic({qjs::Value::from(ctx.js_context(), int64_t{1}),
                                 qjs::Value::from(ctx.js_context(), int64_t{2})}) == 2);
    };

    EXPECT_NOTHROWS(f());

    auto runtime = qjs::Runtime::create();
    auto& ctx = runtime.context();

    auto plain_object = qjs::json::parse(R"({"value":1})", ctx).as<qjs::Object>();
    EXPECT_TRUE(throws_with_message(
        [&]() { (void)plain_object.as<qjs::Function<int64_t(qjs::Parameters)>>(); },
        "Object is not a function"));

    auto sum_all =
        qjs::Function<int64_t(qjs::Parameters)>::from(ctx.js_context(), [](qjs::Parameters args) {
            int64_t sum = 0;
            for(auto& arg: args) {
                sum += arg.as<int64_t>();
            }
            return sum;
        });
    ctx.global_this().set_property("sumAll", sum_all);

    EXPECT_TRUE(throws_with_message([&]() { ctx.eval("sumAll(1, 'x')", "<eval>", eval_flags); },
                                    "Value is not a number"));
};

TEST_CASE(error_and_json_helpers_cover_metadata_stringify_and_invalid_variadic_args) {
    auto f = [&]() {
        auto runtime = qjs::Runtime::create();
        auto& ctx = runtime.context();

        auto error = ctx.eval("new TypeError('boom')", "<eval>", eval_flags)
                         .as<qjs::Object>()
                         .as<qjs::Error>();

        EXPECT_TRUE(error.name() == "TypeError");
        EXPECT_TRUE(error.message() == "boom");
        EXPECT_TRUE(error.stack().contains("<eval>:1:4"));
        EXPECT_TRUE(error.format().contains("Stack Trace:"));

        auto parsed = qjs::json::parse(R"({"number":1,"text":"ok"})", ctx).as<qjs::Object>();
        auto dumped = qjs::json::stringify(parsed);
        EXPECT_TRUE(dumped.contains(R"("number":1)"));
        EXPECT_TRUE(dumped.contains(R"("text":"ok")"));

        auto cyclic =
            ctx.eval("const x = {}; x.self = x; x;", "<eval>", eval_flags).as<qjs::Object>();
        EXPECT_TRUE(
            throws_with_message([&]() { (void)qjs::json::stringify(cyclic); }, "TypeError"));

        auto count_args = qjs::Function<int64_t(qjs::Parameters)>::from(
            ctx.js_context(),
            [](qjs::Parameters args) { return static_cast<int64_t>(args.size()); });
        qjs::Parameters invalid_args{};
        invalid_args.emplace_back();
        EXPECT_TRUE(
            throws_with_message([&]() { (void)count_args(invalid_args); }, "invalid value"));
    };
    EXPECT_NOTHROWS(f());
};

TEST_CASE(object_register_reuses_class_id_per_runtime_and_separates_runtimes) {
    auto runtime_a = qjs::Runtime::create();
    auto& ctx_a = runtime_a.context();
    auto runtime_b = qjs::Runtime::create();
    auto& ctx_b = runtime_b.context();

    using Register = qjs::Object::Register<IncrementFunctor&&>;

    EXPECT_TRUE(Register::get(runtime_a.js_runtime()) == JS_INVALID_CLASS_ID);
    EXPECT_TRUE(Register::get(runtime_b.js_runtime()) == JS_INVALID_CLASS_ID);

    auto f = [&]() {
        auto functor_fn_1 =
            qjs::Function<int64_t(int64_t)>::from(ctx_a.js_context(), IncrementFunctor{.delta = 5});
        auto first_id = Register::get(runtime_a.js_runtime());
        EXPECT_TRUE(first_id != JS_INVALID_CLASS_ID);
        EXPECT_TRUE(functor_fn_1(7) == 12);
        EXPECT_TRUE(Register::get(runtime_b.js_runtime()) == JS_INVALID_CLASS_ID);

        auto functor_fn_2 =
            qjs::Function<int64_t(int64_t)>::from(ctx_a.js_context(), IncrementFunctor{.delta = 5});
        EXPECT_TRUE(Register::get(runtime_a.js_runtime()) == first_id);
        EXPECT_TRUE(functor_fn_2(9) == 14);

        auto other_runtime_fn =
            qjs::Function<int64_t(int64_t)>::from(ctx_b.js_context(), IncrementFunctor{.delta = 5});
        auto second_id = Register::get(runtime_b.js_runtime());
        EXPECT_TRUE(second_id != JS_INVALID_CLASS_ID);
        EXPECT_TRUE(other_runtime_fn(11) == 16);
        EXPECT_TRUE(Register::get(runtime_b.js_runtime()) == second_id);
    };

    EXPECT_NOTHROWS(f());
};

TEST_CASE(cmodule_exports_cover_functor_export_bare_export_and_module_cache) {
    auto f = [&]() {
        auto runtime = qjs::Runtime::create();
        auto& ctx = runtime.context();

        auto& module = ctx.cmodule("native-test");
        auto& same_module = ctx.cmodule("native-test");
        EXPECT_TRUE(&module == &same_module);

        module.export_functor("concat",
                              qjs::Function<std::string(std::string, std::string)>::from(
                                  ctx.js_context(),
                                  [](std::string lhs, std::string rhs) { return lhs + rhs; }));
        module.export_bare_functor("fortyTwo", forty_two_raw, 0);

        ctx.eval(R"(
				import { concat, fortyTwo } from 'native-test';
				globalThis.moduleConcat = concat('cat', 'ter');
				globalThis.moduleValue = fortyTwo();
			)",
                 "native-test.mjs",
                 JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT);

        auto global = ctx.global_this();
        EXPECT_TRUE(global.get_property("moduleConcat").as<std::string>() == "catter");
        EXPECT_TRUE(global.get_property("moduleValue").as<int64_t>() == 42);
    };

    EXPECT_NOTHROWS(f());
};

TEST_CASE(promise_capability_and_then_cover_fulfilled_and_rejected_paths) {
    auto f = [&]() {
        auto runtime = qjs::Runtime::create();
        auto& ctx = runtime.context();

        auto fulfilled_cap = qjs::Promise::create(ctx.js_context());
        auto fulfilled_promise_value = qjs::Value::from(fulfilled_cap.promise);
        EXPECT_TRUE(fulfilled_promise_value.as<qjs::Promise>().is_pending());

        auto fulfilled_object = qjs::Object::from(fulfilled_cap.promise);
        EXPECT_TRUE(fulfilled_object.as<qjs::Promise>().is_pending());

        std::string fulfilled_value;
        auto on_fulfilled =
            qjs::Function<void(std::string)>::from(ctx.js_context(), [&](std::string value) {
                fulfilled_value = std::move(value);
            });
        auto fulfilled_next = fulfilled_cap.promise.then(on_fulfilled);

        fulfilled_cap.resolve("resolved");
        drain_jobs(runtime.js_runtime());

        EXPECT_TRUE(fulfilled_cap.promise.is_fulfilled());
        EXPECT_TRUE(fulfilled_next.is_fulfilled());
        EXPECT_TRUE(fulfilled_value == "resolved");

        auto rejected_cap = qjs::Promise::create(ctx.js_context());
        std::string rejected_reason;
        auto unexpected_fulfilled = qjs::Promise::ThenCallback::from(
            ctx.js_context(),
            []([[maybe_unused]] qjs::Parameters args) { throw qjs::Exception("unexpected"); });
        auto on_rejected =
            qjs::Function<std::string(std::string)>::from(ctx.js_context(),
                                                          [&](std::string reason) {
                                                              rejected_reason = std::move(reason);
                                                              return std::string("handled");
                                                          });
        auto rejected_next = rejected_cap.promise.then(unexpected_fulfilled, on_rejected);

        rejected_cap.reject("rejected");
        drain_jobs(runtime.js_runtime());

        EXPECT_TRUE(rejected_cap.promise.is_rejected());
        EXPECT_TRUE(rejected_next.is_fulfilled());
        EXPECT_TRUE(rejected_reason == "rejected");

        auto caught_cap = qjs::Promise::create(ctx.js_context());
        std::string caught_reason;
        auto on_caught =
            qjs::Function<std::string(std::string)>::from(ctx.js_context(),
                                                          [&](std::string reason) {
                                                              caught_reason = std::move(reason);
                                                              return std::string("caught");
                                                          });
        auto caught_next = caught_cap.promise.when_err(on_caught);

        caught_cap.reject("catch-path");
        drain_jobs(runtime.js_runtime());

        EXPECT_TRUE(caught_cap.promise.is_rejected());
        EXPECT_TRUE(caught_next.is_fulfilled());
        EXPECT_TRUE(caught_reason == "catch-path");
    };

    EXPECT_NOTHROWS(f());
};

TEST_CASE(async_function_wraps_tasks_as_promises) {
    auto f = [&]() {
        auto task = []() -> kota::task<> {
            auto runtime = qjs::Runtime::create();
            auto& ctx = runtime.context();
            js::JsLoop js_loop;
            js::JsLoopScope js_loop_scope(js_loop);

            auto& loop = kota::event_loop::current();
            loop.schedule(js_loop.run(runtime, loop));

            auto global = ctx.global_this();
            using AsyncGreet = qjs::Function<qjs::Promise(std::string)>;
            using AsyncAddOrFail = qjs::Function<qjs::Promise(int64_t)>;

            global.set_property(
                "asyncGreet",
                AsyncGreet::from(ctx.js_context(), [ctx = ctx.js_context()](std::string name) {
                    return js::task_to_promise(ctx, async_greet(std::move(name)));
                }));
            global.set_property(
                "asyncAddOrFail",
                AsyncAddOrFail::from(ctx.js_context(), [ctx = ctx.js_context()](int64_t value) {
                    return js::task_to_promise(ctx, async_add_or_fail(value));
                }));

            auto bridged = co_await js::promise_to_task<std::string>(
                js::task_to_promise(ctx.js_context(), async_greet("bridge")));
            if(!bridged) {
                throw std::move(bridged.error());
            }
            EXPECT_TRUE(bridged.value() == "hello bridge");

            auto eval_result = ctx.eval(R"(
                    (async () => {
                        globalThis.asyncGreetResult = await asyncGreet('catter');
                        globalThis.asyncAddResult = await asyncAddOrFail(41);

                        try {
                            await asyncAddOrFail(-1);
                        } catch (error) {
                            globalThis.asyncErrorResult = String(error);
                        }
                    })();
                )",
                                        "async-function-test.js",
                                        eval_flags);
            auto result =
                co_await js::promise_to_task(qjs::Promise::from_value(std::move(eval_result)));
            co_await js_loop.stop();
            if(!result) {
                throw std::move(result.error());
            }

            EXPECT_TRUE(global["asyncGreetResult"].as<std::string>() == "hello catter");
            EXPECT_TRUE(global["asyncAddResult"].as<int64_t>() == 42);
            EXPECT_TRUE(global["asyncErrorResult"].as<std::string>().contains("negative value"));
        }();

        kota::event_loop loop;
        loop.schedule(task);
        loop.run();
        task.result();
    };

    EXPECT_NOTHROWS(f());
};
};  // TEST_SUITE(qjs_tests)
