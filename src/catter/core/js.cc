#include <cstdio>
#include <cstring>
#include <stdexcept>

#include <eventide/reflection/enum.h>

#include "config/js-lib.h"

#include "js.h"
#include "qjs.h"
#include "apitool.h"

namespace catter::js {

using OnStart = qjs::Function<qjs::Object(qjs::Object config)>;

using OnFinish = qjs::Function<void()>;

using OnCommand = qjs::Function<qjs::Object(uint32_t id, qjs::Object data)>;

using OnExecution = qjs::Function<void(uint32_t id, qjs::Object data)>;

struct Self {
    RuntimeConfig global_config;
    qjs::Runtime rt;
    qjs::Object js_mod_obj;
    OnStart on_start;
    OnFinish on_finish;
    OnCommand on_command;
    OnExecution on_execution;
};

namespace {
Self self{};
}  // namespace

CatterConfig on_start(CatterConfig config) {
    if(!self.on_start) {
        throw std::runtime_error("service.onStart is not registered");
    }
    return CatterConfig::make(self.on_start(config.to_object(self.on_start.context())));
}

void on_finish() {
    if(!self.on_finish) {
        throw std::runtime_error("service.onFinish is not registered");
    }
    return self.on_finish();
}

Action on_command(uint32_t id, std::variant<CommandData, CatterErr> data) {
    if(!self.on_command) {
        throw std::runtime_error("service.onCommand is not registered");
    }
    return Action::make(self.on_command(
        id,
        std::visit([](const auto& v) { return v.to_object(self.on_command.context()); }, data)));
}

void on_execution(uint32_t id, ExecutionEvent event) {
    if(!self.on_execution) {
        throw std::runtime_error("service.onExecution is not registered");
    }
    return self.on_execution(id, event.to_object(self.on_execution.context()));
}

void set_on_start(qjs::Object cb) {
    self.on_start = cb.as<OnStart>();
}

void set_on_finish(qjs::Object cb) {
    self.on_finish = cb.as<OnFinish>();
}

void set_on_command(qjs::Object cb) {
    self.on_command = cb.as<OnCommand>();
}

void set_on_execution(qjs::Object cb) {
    self.on_execution = cb.as<OnExecution>();
}

const RuntimeConfig& get_global_runtime_config() {
    return self.global_config;
}

void sync_eval(std::string_view input, const char* filename, int eval_flags) {
    auto& ctx = self.rt.context();
    auto js_ctx = ctx.js_context();

    auto promise_obj = ctx.eval(input, filename, eval_flags).as<qjs::Object>();

    if(JS_IsPromise(promise_obj.value())) {
        using Then = qjs::Function<qjs::Object(qjs::Object resolve, qjs::Object reject)>;
        using Catch = qjs::Function<void(qjs::Object)>;

        using CallBack = qjs::Function<void(qjs::Parameters)>;

        enum { Pending, Fulfilled, Rejected } state = Pending;

        std::string error_strace;

        auto resolve = CallBack::from(js_ctx, [&](qjs::Parameters args) { state = Fulfilled; });

        auto reject = CallBack::from(js_ctx, [&](qjs::Parameters args) {
            state = Rejected;
            for(auto& arg: args) {
                error_strace += arg.stringify() + "\n";
            }
        });

        promise_obj["then"].as<Then>().invoke(promise_obj,
                                              qjs::Object::from(resolve),
                                              qjs::Object::from(reject));

        int err;
        JSContext* ctx1;

        while((err = JS_ExecutePendingJob(self.rt.js_runtime(), &ctx1)) != 0) {
            if(err < 0) {
                throw qjs::Exception("Error while executing pending job.");
            }
        }

        switch(state) {
            case Fulfilled: break;
            case Rejected: throw qjs::Exception(error_strace);
            case Pending:
                throw qjs::Exception("Promise is still pending after executing all pending jobs.");
        }
    }
};

void init_qjs(const RuntimeConfig& config) {
    self.rt = qjs::Runtime::create();
    self.global_config = config;

    const qjs::Context& ctx = self.rt.context();
    auto& mod = ctx.cmodule("catter-c");
    for(auto& reg: catter::apitool::api_registers()) {
        reg(mod, ctx);
    }
    auto js_lib_trim =
        config::data::js_lib.substr(0, config::data::js_lib.find_last_not_of('\0') + 1);
    // init js lib

    sync_eval(js_lib_trim, "catter", JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT);
    sync_eval("import * as catter from 'catter'; globalThis.__catter_mod = catter;",
              "get-mod.js",
              JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT);

    self.js_mod_obj = ctx.global_this()["__catter_mod"].as<qjs::Object>();
};

void run_js_file(std::string_view content, const std::string filepath) {
    sync_eval(content, filepath.data(), JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT);
}

qjs::Object& js_mod_object() {
    return self.js_mod_obj;
}

}  // namespace catter::js
