#include "backend.hpp"

#if defined(__linux__) && !defined(__ANDROID__)

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <thread>
#include <utility>

#if defined(__has_include)
#if __has_include(<pulse/introspect.h>) && __has_include(<pulse/simple.h>) && __has_include(<pulse/stream.h>)
#define FIZMO_AUDIO_HAVE_PULSE_HEADERS 1
#include <pulse/introspect.h>
#include <pulse/simple.h>
#include <pulse/stream.h>
#endif
#endif

namespace fizmo {
namespace audio {
namespace detail {

namespace {

namespace pa {

struct mainloop;
struct mainloop_api;
struct context;
struct operation;
struct simple;
struct stream;

struct sample_spec {
    int format;
    std::uint32_t rate;
    std::uint8_t channels;
};

struct channel_map {
    std::uint8_t channels;
    int map[32];
};

struct buffer_attr {
    std::uint32_t maxlength;
    std::uint32_t tlength;
    std::uint32_t prebuf;
    std::uint32_t minreq;
    std::uint32_t fragsize;
};

struct server_info {
    const char* user_name;
    const char* host_name;
    const char* server_version;
    const char* server_name;
    sample_spec spec;
    const char* default_sink_name;
    const char* default_source_name;
};

struct sink_info {
    const char* name;
    std::uint32_t index;
    const char* description;
    sample_spec spec;
};

struct cvolume {
    std::uint8_t channels;
    std::uint32_t values[32];
};

struct source_info {
    const char* name;
    std::uint32_t index;
    const char* description;
    sample_spec spec;
    channel_map map;
    std::uint32_t owner_module;
    cvolume volume;
    int mute;
    std::uint32_t monitor_of_sink;
};

constexpr int sample_float32le = 5;
constexpr int stream_playback = 1;
constexpr int stream_record = 2;
constexpr int context_ready = 4;
constexpr int context_failed = 5;
constexpr int context_terminated = 6;
constexpr int operation_running = 0;
constexpr int context_noautospawn = 1;
constexpr std::uint32_t invalid_index = 0xFFFFFFFFu;
constexpr int stream_ready = 2;
constexpr int stream_failed = 3;
constexpr int stream_terminated = 4;
constexpr int stream_start_corked = 0x0001;
constexpr int stream_dont_move = 0x0200;
constexpr int stream_adjust_latency = 0x2000;
constexpr int seek_relative = 0;
constexpr int error_noentity = 5;
constexpr int error_killed = 12;

using context_notify_cb = void (*)(context*, void*);
using stream_notify_cb = void (*)(stream*, void*);
using stream_request_cb = void (*)(stream*, std::size_t, void*);
using stream_success_cb = void (*)(stream*, int, void*);
using free_cb = void (*)(void*);
using server_info_cb = void (*)(context*, const server_info*, void*);
using sink_info_cb = void (*)(context*, const sink_info*, int, void*);
using source_info_cb = void (*)(context*, const source_info*, int, void*);

} // namespace pa

#if defined(FIZMO_AUDIO_HAVE_PULSE_HEADERS)
static_assert(sizeof(pa::sample_spec) == sizeof(pa_sample_spec), "pa_sample_spec layout");
static_assert(sizeof(pa::channel_map) == sizeof(pa_channel_map), "pa_channel_map layout");
static_assert(sizeof(pa::buffer_attr) == sizeof(pa_buffer_attr), "pa_buffer_attr layout");
static_assert(sizeof(pa::cvolume) == sizeof(pa_cvolume), "pa_cvolume layout");
static_assert(offsetof(pa::server_info, default_sink_name) == offsetof(pa_server_info, default_sink_name), "pa_server_info layout");
static_assert(offsetof(pa::server_info, default_source_name) == offsetof(pa_server_info, default_source_name), "pa_server_info layout");
static_assert(offsetof(pa::sink_info, description) == offsetof(pa_sink_info, description), "pa_sink_info layout");
static_assert(offsetof(pa::sink_info, spec) == offsetof(pa_sink_info, sample_spec), "pa_sink_info layout");
static_assert(offsetof(pa::source_info, spec) == offsetof(pa_source_info, sample_spec), "pa_source_info layout");
static_assert(offsetof(pa::source_info, monitor_of_sink) == offsetof(pa_source_info, monitor_of_sink), "pa_source_info layout");
static_assert(PA_SAMPLE_FLOAT32LE == pa::sample_float32le, "sample format value");
static_assert(PA_STREAM_PLAYBACK == pa::stream_playback && PA_STREAM_RECORD == pa::stream_record, "stream direction values");
static_assert(PA_CONTEXT_READY == pa::context_ready && PA_CONTEXT_FAILED == pa::context_failed && PA_CONTEXT_TERMINATED == pa::context_terminated, "context state values");
static_assert(PA_OPERATION_RUNNING == pa::operation_running, "operation state value");
static_assert(PA_CONTEXT_NOAUTOSPAWN == pa::context_noautospawn, "context flag value");
static_assert(PA_STREAM_READY == pa::stream_ready && PA_STREAM_FAILED == pa::stream_failed && PA_STREAM_TERMINATED == pa::stream_terminated, "stream state values");
static_assert(PA_STREAM_START_CORKED == pa::stream_start_corked && PA_STREAM_DONT_MOVE == pa::stream_dont_move && PA_STREAM_ADJUST_LATENCY == pa::stream_adjust_latency, "stream flag values");
static_assert(PA_SEEK_RELATIVE == pa::seek_relative, "seek mode value");
static_assert(PA_ERR_NOENTITY == pa::error_noentity && PA_ERR_KILLED == pa::error_killed, "error code values");
#endif

struct PulseApi {
    DynamicLibrary lib;
    DynamicLibrary simple_lib;

    pa::mainloop* (*threaded_mainloop_new)() = nullptr;
    int (*threaded_mainloop_start)(pa::mainloop*) = nullptr;
    void (*threaded_mainloop_stop)(pa::mainloop*) = nullptr;
    void (*threaded_mainloop_free)(pa::mainloop*) = nullptr;
    void (*threaded_mainloop_lock)(pa::mainloop*) = nullptr;
    void (*threaded_mainloop_unlock)(pa::mainloop*) = nullptr;
    void (*threaded_mainloop_wait)(pa::mainloop*) = nullptr;
    void (*threaded_mainloop_signal)(pa::mainloop*, int) = nullptr;
    pa::mainloop_api* (*threaded_mainloop_get_api)(pa::mainloop*) = nullptr;
    pa::context* (*context_new)(pa::mainloop_api*, const char*) = nullptr;
    void (*context_set_state_callback)(pa::context*, pa::context_notify_cb, void*) = nullptr;
    int (*context_connect)(pa::context*, const char*, int, const void*) = nullptr;
    int (*context_get_state)(const pa::context*) = nullptr;
    void (*context_disconnect)(pa::context*) = nullptr;
    void (*context_unref)(pa::context*) = nullptr;
    int (*context_errno)(const pa::context*) = nullptr;
    pa::operation* (*context_get_server_info)(pa::context*, pa::server_info_cb, void*) = nullptr;
    pa::operation* (*context_get_sink_info_list)(pa::context*, pa::sink_info_cb, void*) = nullptr;
    pa::operation* (*context_get_source_info_list)(pa::context*, pa::source_info_cb, void*) = nullptr;
    int (*operation_get_state)(const pa::operation*) = nullptr;
    void (*operation_unref)(pa::operation*) = nullptr;
    const char* (*strerror)(int) = nullptr;
    pa::stream* (*stream_new)(pa::context*, const char*, const pa::sample_spec*, const pa::channel_map*) = nullptr;
    void (*stream_unref)(pa::stream*) = nullptr;
    int (*stream_connect_playback)(pa::stream*, const char*, const pa::buffer_attr*, int, const void*, pa::stream*) = nullptr;
    int (*stream_connect_record)(pa::stream*, const char*, const pa::buffer_attr*, int) = nullptr;
    int (*stream_disconnect)(pa::stream*) = nullptr;
    int (*stream_get_state)(const pa::stream*) = nullptr;
    void (*stream_set_state_callback)(pa::stream*, pa::stream_notify_cb, void*) = nullptr;
    void (*stream_set_write_callback)(pa::stream*, pa::stream_request_cb, void*) = nullptr;
    void (*stream_set_read_callback)(pa::stream*, pa::stream_request_cb, void*) = nullptr;
    void (*stream_set_moved_callback)(pa::stream*, pa::stream_notify_cb, void*) = nullptr;
    int (*stream_write)(pa::stream*, const void*, std::size_t, pa::free_cb, std::int64_t, int) = nullptr;
    int (*stream_peek)(pa::stream*, const void**, std::size_t*) = nullptr;
    int (*stream_drop)(pa::stream*) = nullptr;
    std::size_t (*stream_writable_size)(const pa::stream*) = nullptr;
    pa::operation* (*stream_cork)(pa::stream*, int, pa::stream_success_cb, void*) = nullptr;
    pa::operation* (*stream_flush)(pa::stream*, pa::stream_success_cb, void*) = nullptr;
    const char* (*stream_get_device_name)(const pa::stream*) = nullptr;

    pa::simple* (*simple_new)(const char*, const char*, int, const char*, const char*, const pa::sample_spec*, const pa::channel_map*, const pa::buffer_attr*, int*) = nullptr;
    int (*simple_write)(pa::simple*, const void*, std::size_t, int*) = nullptr;
    int (*simple_read)(pa::simple*, void*, std::size_t, int*) = nullptr;
    int (*simple_flush)(pa::simple*, int*) = nullptr;
    std::uint64_t (*simple_get_latency)(pa::simple*, int*) = nullptr;
    void (*simple_free)(pa::simple*) = nullptr;

    bool load(std::string& why) {
        if (!lib.open({ "libpulse.so.0", "libpulse.so" })) {
            why = "libpulse isn't installed";
            return false;
        }
        if (!simple_lib.open({ "libpulse-simple.so.0", "libpulse-simple.so" })) {
            why = "libpulse-simple isn't installed";
            return false;
        }
        bool ok = true;
        ok &= lib.load(threaded_mainloop_new, "pa_threaded_mainloop_new");
        ok &= lib.load(threaded_mainloop_start, "pa_threaded_mainloop_start");
        ok &= lib.load(threaded_mainloop_stop, "pa_threaded_mainloop_stop");
        ok &= lib.load(threaded_mainloop_free, "pa_threaded_mainloop_free");
        ok &= lib.load(threaded_mainloop_lock, "pa_threaded_mainloop_lock");
        ok &= lib.load(threaded_mainloop_unlock, "pa_threaded_mainloop_unlock");
        ok &= lib.load(threaded_mainloop_wait, "pa_threaded_mainloop_wait");
        ok &= lib.load(threaded_mainloop_signal, "pa_threaded_mainloop_signal");
        ok &= lib.load(threaded_mainloop_get_api, "pa_threaded_mainloop_get_api");
        ok &= lib.load(context_new, "pa_context_new");
        ok &= lib.load(context_set_state_callback, "pa_context_set_state_callback");
        ok &= lib.load(context_connect, "pa_context_connect");
        ok &= lib.load(context_get_state, "pa_context_get_state");
        ok &= lib.load(context_disconnect, "pa_context_disconnect");
        ok &= lib.load(context_unref, "pa_context_unref");
        ok &= lib.load(context_errno, "pa_context_errno");
        ok &= lib.load(context_get_server_info, "pa_context_get_server_info");
        ok &= lib.load(context_get_sink_info_list, "pa_context_get_sink_info_list");
        ok &= lib.load(context_get_source_info_list, "pa_context_get_source_info_list");
        ok &= lib.load(operation_get_state, "pa_operation_get_state");
        ok &= lib.load(operation_unref, "pa_operation_unref");
        ok &= lib.load(strerror, "pa_strerror");
        ok &= lib.load(stream_new, "pa_stream_new");
        ok &= lib.load(stream_unref, "pa_stream_unref");
        ok &= lib.load(stream_connect_playback, "pa_stream_connect_playback");
        ok &= lib.load(stream_connect_record, "pa_stream_connect_record");
        ok &= lib.load(stream_disconnect, "pa_stream_disconnect");
        ok &= lib.load(stream_get_state, "pa_stream_get_state");
        ok &= lib.load(stream_set_state_callback, "pa_stream_set_state_callback");
        ok &= lib.load(stream_set_write_callback, "pa_stream_set_write_callback");
        ok &= lib.load(stream_set_read_callback, "pa_stream_set_read_callback");
        ok &= lib.load(stream_set_moved_callback, "pa_stream_set_moved_callback");
        ok &= lib.load(stream_write, "pa_stream_write");
        ok &= lib.load(stream_peek, "pa_stream_peek");
        ok &= lib.load(stream_drop, "pa_stream_drop");
        ok &= lib.load(stream_writable_size, "pa_stream_writable_size");
        ok &= lib.load(stream_cork, "pa_stream_cork");
        ok &= lib.load(stream_flush, "pa_stream_flush");
        ok &= lib.load(stream_get_device_name, "pa_stream_get_device_name");
        ok &= simple_lib.load(simple_new, "pa_simple_new");
        ok &= simple_lib.load(simple_write, "pa_simple_write");
        ok &= simple_lib.load(simple_read, "pa_simple_read");
        ok &= simple_lib.load(simple_flush, "pa_simple_flush");
        ok &= simple_lib.load(simple_get_latency, "pa_simple_get_latency");
        ok &= simple_lib.load(simple_free, "pa_simple_free");
        if (!ok) why = "the installed libpulse is missing functions fizmo needs";
        return ok;
    }

    std::string error_text(int code) const {
        const char* s = strerror ? strerror(code) : nullptr;
        return s ? std::string(s) : "PulseAudio error " + std::to_string(code);
    }
};

struct Snapshot {
    bool ok = false;
    std::string error;
    std::string default_sink;
    std::string default_source;
    std::vector<DeviceInfo> list;
};

struct Query {
    const PulseApi* api = nullptr;
    pa::mainloop* loop = nullptr;
    Snapshot* out = nullptr;
};

void on_state(pa::context*, void* user) {
    Query* q = static_cast<Query*>(user);
    q->api->threaded_mainloop_signal(q->loop, 0);
}

void on_server(pa::context*, const pa::server_info* info, void* user) {
    Query* q = static_cast<Query*>(user);
    if (info) {
        q->out->default_sink = info->default_sink_name ? info->default_sink_name : "";
        q->out->default_source = info->default_source_name ? info->default_source_name : "";
    }
    q->api->threaded_mainloop_signal(q->loop, 0);
}

void on_sink(pa::context*, const pa::sink_info* info, int eol, void* user) {
    Query* q = static_cast<Query*>(user);
    if (eol || !info) {
        q->api->threaded_mainloop_signal(q->loop, 0);
        return;
    }
    DeviceInfo d;
    d.id = info->name ? info->name : "";
    d.name = info->description ? info->description : d.id;
    d.kind = DeviceKind::output;
    d.sample_rate = info->spec.rate;
    d.channels = info->spec.channels;
    d.backend = "pulse";
    q->out->list.push_back(d);
}

void on_source(pa::context*, const pa::source_info* info, int eol, void* user) {
    Query* q = static_cast<Query*>(user);
    if (eol || !info) {
        q->api->threaded_mainloop_signal(q->loop, 0);
        return;
    }
    DeviceInfo d;
    d.id = info->name ? info->name : "";
    d.name = info->description ? info->description : d.id;
    d.kind = info->monitor_of_sink != pa::invalid_index ? DeviceKind::loopback : DeviceKind::input;
    d.sample_rate = info->spec.rate;
    d.channels = info->spec.channels;
    d.backend = "pulse";
    q->out->list.push_back(d);
}

bool wait_for(const PulseApi& api, pa::mainloop* loop, pa::context* ctx, pa::operation* op) {
    if (!op) return false;
    while (api.operation_get_state(op) == pa::operation_running) {
        const int s = api.context_get_state(ctx);
        if (s == pa::context_failed || s == pa::context_terminated) break;
        api.threaded_mainloop_wait(loop);
    }
    api.operation_unref(op);
    return true;
}

Snapshot snapshot(const PulseApi& api, const char* app_name) {
    Snapshot snap;
    pa::mainloop* loop = api.threaded_mainloop_new();
    if (!loop) {
        snap.error = "can't create a PulseAudio main loop";
        return snap;
    }
    pa::context* ctx = api.context_new(api.threaded_mainloop_get_api(loop), app_name);
    if (!ctx) {
        api.threaded_mainloop_free(loop);
        snap.error = "can't create a PulseAudio context";
        return snap;
    }
    Query q;
    q.api = &api;
    q.loop = loop;
    q.out = &snap;
    api.context_set_state_callback(ctx, on_state, &q);
    if (api.context_connect(ctx, nullptr, pa::context_noautospawn, nullptr) < 0) {
        snap.error = "can't connect to the sound server: " + api.error_text(api.context_errno(ctx));
        api.context_unref(ctx);
        api.threaded_mainloop_free(loop);
        return snap;
    }
    api.threaded_mainloop_lock(loop);
    if (api.threaded_mainloop_start(loop) < 0) {
        api.threaded_mainloop_unlock(loop);
        api.context_disconnect(ctx);
        api.context_unref(ctx);
        api.threaded_mainloop_free(loop);
        snap.error = "can't start the PulseAudio main loop";
        return snap;
    }
    int state = api.context_get_state(ctx);
    while (state != pa::context_ready && state != pa::context_failed && state != pa::context_terminated) {
        api.threaded_mainloop_wait(loop);
        state = api.context_get_state(ctx);
    }
    if (state == pa::context_ready) {
        wait_for(api, loop, ctx, api.context_get_server_info(ctx, on_server, &q));
        wait_for(api, loop, ctx, api.context_get_sink_info_list(ctx, on_sink, &q));
        wait_for(api, loop, ctx, api.context_get_source_info_list(ctx, on_source, &q));
        snap.ok = api.context_get_state(ctx) == pa::context_ready;
        if (!snap.ok) snap.error = "the sound server connection dropped";
    } else {
        snap.error = "can't connect to the sound server: " + api.error_text(api.context_errno(ctx));
    }
    api.context_disconnect(ctx);
    api.threaded_mainloop_unlock(loop);
    api.threaded_mainloop_stop(loop);
    api.context_unref(ctx);
    api.threaded_mainloop_free(loop);
    for (DeviceInfo& d : snap.list) {
        if (d.kind == DeviceKind::output) d.is_default = d.id == snap.default_sink;
        else if (d.kind == DeviceKind::input) d.is_default = d.id == snap.default_source;
        else d.is_default = d.id == snap.default_sink + ".monitor";
    }
    return snap;
}

class PulseStream : public BackendStream {
public:
    PulseStream(std::shared_ptr<const PulseApi> api, std::shared_ptr<StreamShared> shared, pa::simple* handle, std::uint32_t period, std::uint32_t latency, std::string device)
        : m_api(std::move(api)), m_shared(std::move(shared)), m_handle(handle), m_period(period), m_latency(latency), m_device(std::move(device)) {}

    ~PulseStream() override {
        stop();
        if (m_handle) m_api->simple_free(m_handle);
    }

    bool start(std::string&) override {
        if (m_thread.joinable()) return true;
        m_run.store(true);
        if (m_shared->kind != DeviceKind::output) {
            int err = 0;
            m_api->simple_flush(m_handle, &err);
        }
        m_thread = std::thread([this] { loop(); });
        return true;
    }

    void stop() override {
        m_run.store(false);
        if (m_thread.joinable()) m_thread.join();
        if (m_handle && m_shared->kind == DeviceKind::output) {
            int err = 0;
            m_api->simple_flush(m_handle, &err);
        }
    }

    std::uint32_t latency_frames() const override { return m_latency; }
    std::string device_name() const override { return m_device; }

private:
    void loop() {
        const std::size_t channels = m_shared->channels;
        std::vector<float> buf(static_cast<std::size_t>(m_period) * channels, 0.0f);
        const std::size_t bytes = buf.size() * sizeof(float);
        while (m_run.load(std::memory_order_relaxed)) {
            int err = 0;
            if (m_shared->kind == DeviceKind::output) {
                m_shared->render(buf.data(), m_period);
                if (m_api->simple_write(m_handle, buf.data(), bytes, &err) < 0) {
                    m_shared->fail("the sound server stopped the stream: " + m_api->error_text(err));
                    break;
                }
            } else {
                if (m_api->simple_read(m_handle, buf.data(), bytes, &err) < 0) {
                    m_shared->fail("the sound server stopped the stream: " + m_api->error_text(err));
                    break;
                }
                m_shared->deliver(buf.data(), m_period);
            }
        }
    }

    std::shared_ptr<const PulseApi> m_api;
    std::shared_ptr<StreamShared> m_shared;
    pa::simple* m_handle = nullptr;
    std::uint32_t m_period = 0;
    std::uint32_t m_latency = 0;
    std::string m_device;
    std::atomic<bool> m_run{ false };
    std::thread m_thread;
};

class PinnedPulseStream : public BackendStream {
public:
    PinnedPulseStream(std::shared_ptr<const PulseApi> api, std::shared_ptr<StreamShared> shared, std::uint32_t period, std::uint32_t latency, std::string device, std::string shown)
        : m_api(std::move(api)), m_shared(std::move(shared)), m_period(period), m_latency(latency), m_device(std::move(device)), m_shown(std::move(shown)) {
        m_output = m_shared->kind == DeviceKind::output;
        m_frame_bytes = static_cast<std::size_t>(m_shared->channels) * sizeof(float);
        m_stage.assign(static_cast<std::size_t>(m_period) * m_shared->channels, 0.0f);
        m_stage_pos = m_output ? m_period : 0;
    }

    ~PinnedPulseStream() override {
        stop();
        if (!m_loop) return;
        m_api->threaded_mainloop_lock(m_loop);
        m_closing = true;
        if (m_stream) {
            m_api->stream_set_state_callback(m_stream, nullptr, nullptr);
            m_api->stream_set_write_callback(m_stream, nullptr, nullptr);
            m_api->stream_set_read_callback(m_stream, nullptr, nullptr);
            m_api->stream_set_moved_callback(m_stream, nullptr, nullptr);
            m_api->stream_disconnect(m_stream);
            m_api->stream_unref(m_stream);
            m_stream = nullptr;
        }
        if (m_context) {
            m_api->context_set_state_callback(m_context, nullptr, nullptr);
            m_api->context_disconnect(m_context);
        }
        m_api->threaded_mainloop_unlock(m_loop);
        if (m_loop_started) m_api->threaded_mainloop_stop(m_loop);
        if (m_context) m_api->context_unref(m_context);
        m_api->threaded_mainloop_free(m_loop);
    }

    bool connect(const std::string& app_name, const pa::sample_spec& spec, const pa::buffer_attr& attr, std::string& error) {
        m_loop = m_api->threaded_mainloop_new();
        if (!m_loop) {
            error = "can't create a PulseAudio main loop";
            return false;
        }
        m_context = m_api->context_new(m_api->threaded_mainloop_get_api(m_loop), app_name.c_str());
        if (!m_context) {
            error = "can't create a PulseAudio context";
            return false;
        }
        m_api->context_set_state_callback(m_context, on_context_state, this);
        if (m_api->context_connect(m_context, nullptr, pa::context_noautospawn, nullptr) < 0) {
            error = "can't connect to the sound server: " + m_api->error_text(m_api->context_errno(m_context));
            return false;
        }
        Locked guard(*this);
        if (m_api->threaded_mainloop_start(m_loop) < 0) {
            error = "can't start the PulseAudio main loop";
            return false;
        }
        m_loop_started = true;
        int state = m_api->context_get_state(m_context);
        while (state != pa::context_ready && state != pa::context_failed && state != pa::context_terminated) {
            m_api->threaded_mainloop_wait(m_loop);
            state = m_api->context_get_state(m_context);
        }
        if (state != pa::context_ready) {
            error = "can't connect to the sound server: " + m_api->error_text(m_api->context_errno(m_context));
            return false;
        }
        m_stream = m_api->stream_new(m_context, app_name.c_str(), &spec, nullptr);
        if (!m_stream) {
            error = "can't create a PulseAudio stream: " + m_api->error_text(m_api->context_errno(m_context));
            return false;
        }
        m_api->stream_set_state_callback(m_stream, on_stream_state, this);
        m_api->stream_set_moved_callback(m_stream, on_moved, this);
        if (m_output) m_api->stream_set_write_callback(m_stream, on_write, this);
        else m_api->stream_set_read_callback(m_stream, on_read, this);
        const int flags = pa::stream_start_corked | pa::stream_dont_move | pa::stream_adjust_latency;
        const int r = m_output ? m_api->stream_connect_playback(m_stream, m_device.c_str(), &attr, flags, nullptr, nullptr) : m_api->stream_connect_record(m_stream, m_device.c_str(), &attr, flags);
        if (r < 0) {
            error = "can't open '" + m_device + "': " + m_api->error_text(m_api->context_errno(m_context));
            return false;
        }
        state = m_api->stream_get_state(m_stream);
        while (state != pa::stream_ready && state != pa::stream_failed && state != pa::stream_terminated) {
            m_api->threaded_mainloop_wait(m_loop);
            state = m_api->stream_get_state(m_stream);
        }
        if (state != pa::stream_ready) {
            error = "can't open '" + m_device + "': " + m_api->error_text(m_api->context_errno(m_context));
            return false;
        }
        m_ready = true;
        return true;
    }

    bool start(std::string& error) override {
        Locked guard(*this);
        if (m_shared->failed.load()) {
            error = m_shared->last_error();
            return false;
        }
        if (m_active) return true;
        if (m_output) {
            m_stage_pos = m_period;
            m_active = true;
            const std::size_t room = m_api->stream_writable_size(m_stream);
            if (room != static_cast<std::size_t>(-1)) write_some(room);
        } else {
            wait_op(m_api->stream_flush(m_stream, on_success, this));
            m_stage_pos = 0;
            m_active = true;
        }
        wait_op(m_api->stream_cork(m_stream, 0, on_success, this));
        if (m_shared->failed.load()) {
            m_active = false;
            error = m_shared->last_error();
            return false;
        }
        return true;
    }

    void stop() override {
        if (!m_loop || !m_stream) return;
        Locked guard(*this);
        if (!m_active) return;
        m_active = false;
        if (!stream_alive()) return;
        wait_op(m_api->stream_cork(m_stream, 1, on_success, this));
        if (m_output) wait_op(m_api->stream_flush(m_stream, on_success, this));
    }

    std::uint32_t latency_frames() const override { return m_latency; }
    std::string device_name() const override { return m_shown; }

private:
    class Locked {
    public:
        explicit Locked(PinnedPulseStream& s) : m_s(s) { m_s.m_api->threaded_mainloop_lock(m_s.m_loop); }
        ~Locked() { m_s.m_api->threaded_mainloop_unlock(m_s.m_loop); }
        Locked(const Locked&) = delete;
        Locked& operator=(const Locked&) = delete;

    private:
        PinnedPulseStream& m_s;
    };

    bool stream_alive() const {
        return m_stream && m_api->stream_get_state(m_stream) == pa::stream_ready && m_api->context_get_state(m_context) == pa::context_ready;
    }

    void wait_op(pa::operation* op) {
        if (!op) return;
        while (m_api->operation_get_state(op) == pa::operation_running && stream_alive()) m_api->threaded_mainloop_wait(m_loop);
        m_api->operation_unref(op);
    }

    void lost(const std::string& message) {
        m_active = false;
        if (!m_closing) m_shared->fail(message);
        m_api->threaded_mainloop_signal(m_loop, 0);
    }

    void write_some(std::size_t bytes) {
        std::size_t wanted = bytes / m_frame_bytes;
        const std::size_t channels = m_shared->channels;
        while (wanted > 0 && m_active) {
            if (m_stage_pos >= m_period) {
                m_shared->render(m_stage.data(), m_period);
                m_stage_pos = 0;
            }
            const std::size_t n = std::min<std::size_t>(wanted, m_period - m_stage_pos);
            if (m_api->stream_write(m_stream, m_stage.data() + m_stage_pos * channels, n * m_frame_bytes, nullptr, 0, pa::seek_relative) < 0) {
                lost("the sound server stopped the stream: " + m_api->error_text(m_api->context_errno(m_context)));
                return;
            }
            m_stage_pos += static_cast<std::uint32_t>(n);
            wanted -= n;
        }
    }

    void read_all() {
        const std::size_t channels = m_shared->channels;
        for (;;) {
            const void* data = nullptr;
            std::size_t bytes = 0;
            if (m_api->stream_peek(m_stream, &data, &bytes) < 0) {
                lost("the sound server stopped the stream: " + m_api->error_text(m_api->context_errno(m_context)));
                return;
            }
            if (bytes == 0) return;
            if (m_active) {
                const float* in = static_cast<const float*>(data);
                std::size_t frames = bytes / m_frame_bytes;
                while (frames > 0) {
                    const std::size_t n = std::min<std::size_t>(frames, m_period - m_stage_pos);
                    float* dst = m_stage.data() + static_cast<std::size_t>(m_stage_pos) * channels;
                    if (in) {
                        std::memcpy(dst, in, n * m_frame_bytes);
                        in += n * channels;
                    } else {
                        std::memset(dst, 0, n * m_frame_bytes);
                    }
                    m_stage_pos += static_cast<std::uint32_t>(n);
                    frames -= n;
                    if (m_stage_pos == m_period) {
                        m_shared->deliver(m_stage.data(), m_period);
                        m_stage_pos = 0;
                    }
                }
            }
            m_api->stream_drop(m_stream);
        }
    }

    static void on_context_state(pa::context* ctx, void* user) {
        PinnedPulseStream* self = static_cast<PinnedPulseStream*>(user);
        const int state = self->m_api->context_get_state(ctx);
        if (self->m_ready && (state == pa::context_failed || state == pa::context_terminated)) {
            self->lost("lost the connection to the sound server: " + self->m_api->error_text(self->m_api->context_errno(ctx)));
            return;
        }
        self->m_api->threaded_mainloop_signal(self->m_loop, 0);
    }

    static void on_stream_state(pa::stream* s, void* user) {
        PinnedPulseStream* self = static_cast<PinnedPulseStream*>(user);
        const int state = self->m_api->stream_get_state(s);
        if (self->m_ready && (state == pa::stream_failed || state == pa::stream_terminated)) {
            const int code = self->m_api->context_errno(self->m_context);
            if (self->m_api->context_get_state(self->m_context) != pa::context_ready) self->lost("lost the connection to the sound server: " + self->m_api->error_text(code));
            else if (code == pa::error_killed || code == pa::error_noentity) self->lost("the device '" + self->m_device + "' was removed");
            else self->lost("the sound server stopped the stream on '" + self->m_device + "': " + self->m_api->error_text(code));
            return;
        }
        self->m_api->threaded_mainloop_signal(self->m_loop, 0);
    }

    static void on_moved(pa::stream* s, void* user) {
        PinnedPulseStream* self = static_cast<PinnedPulseStream*>(user);
        const char* now = self->m_api->stream_get_device_name(s);
        if (now && self->m_device == now) return;
        self->lost("the sound server moved the stream off '" + self->m_device + "' to '" + std::string(now ? now : "another device") + "'");
        pa::operation* op = self->m_stream ? self->m_api->stream_cork(self->m_stream, 1, nullptr, nullptr) : nullptr;
        if (op) self->m_api->operation_unref(op);
    }

    static void on_write(pa::stream*, std::size_t bytes, void* user) {
        PinnedPulseStream* self = static_cast<PinnedPulseStream*>(user);
        if (self->m_active) self->write_some(bytes);
    }

    static void on_read(pa::stream*, std::size_t, void* user) {
        static_cast<PinnedPulseStream*>(user)->read_all();
    }

    static void on_success(pa::stream*, int, void* user) {
        PinnedPulseStream* self = static_cast<PinnedPulseStream*>(user);
        self->m_api->threaded_mainloop_signal(self->m_loop, 0);
    }

    std::shared_ptr<const PulseApi> m_api;
    std::shared_ptr<StreamShared> m_shared;
    std::uint32_t m_period = 0;
    std::uint32_t m_latency = 0;
    std::string m_device;
    std::string m_shown;
    bool m_output = true;
    std::size_t m_frame_bytes = 0;
    std::vector<float> m_stage;
    std::uint32_t m_stage_pos = 0;
    pa::mainloop* m_loop = nullptr;
    pa::context* m_context = nullptr;
    pa::stream* m_stream = nullptr;
    bool m_loop_started = false;
    bool m_ready = false;
    bool m_active = false;
    bool m_closing = false;
};

class PulseBackend : public Backend {
public:
    bool init(std::string& why) {
        m_api = std::make_shared<PulseApi>();
        if (!m_api->load(why)) return false;
        const Snapshot s = snapshot(*m_api, "fizmo");
        if (!s.ok) {
            why = s.error;
            return false;
        }
        return true;
    }

    const char* name() const override { return "pulse"; }

    std::vector<DeviceInfo> devices() override {
        return snapshot(*m_api, "fizmo").list;
    }

    std::unique_ptr<BackendStream> open(const StreamRequest& request, std::shared_ptr<StreamShared> shared, std::string& error) override {
        std::string device = request.device;
        if (request.kind == DeviceKind::loopback && !device.empty()) {
            const std::string suffix = ".monitor";
            if (device.size() < suffix.size() || device.compare(device.size() - suffix.size(), suffix.size(), suffix) != 0) device += suffix;
        }
        std::uint32_t rate = request.sample_rate;
        std::uint32_t channels = request.channels;
        std::string shown = device;
        if (rate == 0 || channels == 0 || shown.empty()) {
            const Snapshot s = snapshot(*m_api, request.name.c_str());
            std::string target = device;
            if (target.empty()) {
                if (request.kind == DeviceKind::output) target = s.default_sink;
                else if (request.kind == DeviceKind::input) target = s.default_source;
                else target = s.default_sink + ".monitor";
            }
            for (const DeviceInfo& d : s.list) {
                if (d.id != target) continue;
                if (rate == 0) rate = d.sample_rate;
                if (channels == 0) channels = d.channels;
                if (shown.empty()) shown = d.name;
                break;
            }
        }
        if (rate == 0) rate = 48000;
        if (channels == 0) channels = request.kind == DeviceKind::input ? 1 : 2;
        if (shown.empty()) shown = "default";
        if (channels > 32) {
            error = "PulseAudio streams can have at most 32 channels";
            return nullptr;
        }
        pa::sample_spec spec;
        spec.format = pa::sample_float32le;
        spec.rate = rate;
        spec.channels = static_cast<std::uint8_t>(channels);
        const std::uint32_t frame_bytes = channels * 4;
        const std::uint32_t latency = std::max<std::uint32_t>(64, rate * request.latency_ms / 1000);
        const std::uint32_t period = std::max<std::uint32_t>(32, latency / 2);
        pa::buffer_attr attr;
        attr.maxlength = 0xFFFFFFFFu;
        attr.prebuf = 0xFFFFFFFFu;
        attr.minreq = 0xFFFFFFFFu;
        if (request.kind == DeviceKind::output) {
            attr.tlength = latency * frame_bytes;
            attr.fragsize = 0xFFFFFFFFu;
        } else {
            attr.tlength = 0xFFFFFFFFu;
            attr.fragsize = period * frame_bytes;
        }
        const char* dev = nullptr;
        std::string dev_name;
        if (!device.empty()) {
            dev_name = device;
            dev = dev_name.c_str();
        } else if (request.kind == DeviceKind::loopback) {
            dev = "@DEFAULT_MONITOR@";
        }
        if (request.pin_device && !device.empty()) {
            shared->sample_rate = rate;
            shared->channels = channels;
            auto pinned = std::make_unique<PinnedPulseStream>(m_api, shared, period, latency, device, shown);
            if (!pinned->connect(request.name, spec, attr, error)) {
                shared->sample_rate = 0;
                shared->channels = 0;
                return nullptr;
            }
            return pinned;
        }
        int err = 0;
        const int direction = request.kind == DeviceKind::output ? pa::stream_playback : pa::stream_record;
        pa::simple* handle = m_api->simple_new(nullptr, request.name.c_str(), direction, dev, request.name.c_str(), &spec, nullptr, &attr, &err);
        if (!handle) {
            error = "can't open " + (device.empty() ? std::string("the default device") : "'" + device + "'") + ": " + m_api->error_text(err);
            return nullptr;
        }
        shared->sample_rate = rate;
        shared->channels = channels;
        return std::make_unique<PulseStream>(m_api, std::move(shared), handle, period, latency, shown);
    }

private:
    std::shared_ptr<PulseApi> m_api;
};

} // namespace

std::unique_ptr<Backend> make_pulse_backend(std::string& why) {
    auto b = std::make_unique<PulseBackend>();
    if (!b->init(why)) return nullptr;
    return b;
}

} // namespace detail
} // namespace audio
} // namespace fizmo

#else

namespace fizmo {
namespace audio {
namespace detail {

std::unique_ptr<Backend> make_pulse_backend(std::string& why) {
    why = "PulseAudio is only used on Linux";
    return nullptr;
}

} // namespace detail
} // namespace audio
} // namespace fizmo

#endif // defined(__linux__) && !defined(__ANDROID__)
