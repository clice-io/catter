#include "js/async.h"

#include <cassert>
#include <utility>
#include <quickjs.h>

#include "js/qjs.h"
#include "util/guard.h"

namespace catter::js {
namespace {

bool drain_jobs_with_budget(qjs::Runtime* rt, std::size_t max_jobs) {
    bool ran = false;
    for(std::size_t i = 0; i < max_jobs && rt->has_job_pending(); ++i) {
        auto ret = rt->execute_pending_job();
        if(!ret.has_value()) {
            if(ret.error().is_error()) {
                throw ret.error().as<qjs::Error>().to_exception();
            } else {
                throw qjs::Exception("Unknown error while executing pending JS job.");
            }
        }
        if(ret.value() == false) {
            break;
        }
        ran = true;
    }

    return ran;
}
}  // namespace

JsLoop::JsLoop(std::size_t job_budget) :
    stopped_event(std::make_shared<kota::event>(true)),
    job_budget(job_budget == 0 ? 1 : job_budget) {}

JsLoop::~JsLoop() {
    assert(!this->loop && "JsLoop must be stopped before destruction.");
    assert(this->run_state == RunState::stopped && "JsLoop must be stopped before destruction.");
}

kota::task<> JsLoop::run(qjs::Runtime& runtime, kota::event_loop& event_loop) {
    if(!this->is_stopped()) {
        throw qjs::Exception("QuickJS async loop is already running.");
    }

    this->rt = &runtime;
    this->loop = &event_loop;
    this->idle = kota::idle::create(event_loop);
    this->relay.emplace(event_loop.create_relay());
    this->stopped_event = std::make_shared<kota::event>();
    this->run_state = RunState::running;

    return this->run_impl();
}

kota::task<> JsLoop::run_impl() {
    auto* owner = this->loop;
    auto finish = util::make_guard([this, owner] noexcept {
        this->cleanup_for(owner);
        if(owner) {
            owner->schedule([](std::shared_ptr<kota::event> event) -> kota::task<> {
                event->set();
                co_return;
            }(this->stopped_event));
        } else {
            this->stopped_event->set();
        }
    });

    while(this->run_state == RunState::running) {
        if(this->rt->has_job_pending()) {
            this->start_idle();
        } else {
            this->stop_idle();
        }

        co_await this->idle.wait();
        if(this->run_state != RunState::running) {
            break;
        }

        drain_jobs_with_budget(this->rt, this->job_budget);
    }

    co_return;
}

void JsLoop::wake() {
    assert(this->can_drive_jobs() && "QuickJS async loop is not running.");
    if(!this->can_drive_jobs()) {
        return;
    }

    this->start_idle();
}

void JsLoop::schedule(kota::task<>&& task) {
    assert(this->can_drive_jobs() && "QuickJS async loop is not running.");
    if(!this->can_drive_jobs()) {
        throw qjs::Exception("QuickJS async loop is not running.");
    }

    this->loop->schedule(std::move(task));
}

kota::task<> JsLoop::stop() {
    auto stopped = this->stopped_event;
    if(this->run_state == RunState::stopped) {
        co_return;
    }
    if(this->run_state == RunState::stopping) {
        co_await stopped->wait();
        co_return;
    }

    this->run_state = RunState::stopping;
    this->start_idle();
    co_await stopped->wait();
    co_return;
}

bool JsLoop::is_running() const noexcept {
    return this->run_state == RunState::running;
}

bool JsLoop::is_stopped() const noexcept {
    return this->run_state == RunState::stopped;
}

void JsLoop::cleanup_for(kota::event_loop* owner) noexcept {
    if(!owner || this->loop != owner) {
        return;
    }

    this->stop_idle();
    this->idle = {};
    this->relay.reset();
    this->run_state = RunState::stopped;
    this->rt = nullptr;
    this->loop = nullptr;
}

void JsLoop::start_idle() {
    if(!this->idle_started) {
        this->idle.start();
        this->idle_started = true;
    }
}

void JsLoop::stop_idle() noexcept {
    if(this->idle_started) {
        this->idle.stop();
        this->idle_started = false;
    }
}

bool JsLoop::can_drive_jobs() const noexcept {
    return this->run_state == RunState::running && this->rt && this->loop;
}

}  // namespace catter::js
