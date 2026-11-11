#include "../Source/Input/input_manager.hpp"
#include "../Source/Input/keys.hpp"

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

using namespace fizmo;
using namespace fizmo::input;

namespace {

int g_passed = 0;
int g_failed = 0;

void check(bool ok, const std::string& what) {
    if (ok) {
        ++g_passed;
    } else {
        ++g_failed;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

static_assert(key_from_name("a") == Key::A, "key_from_name is case-insensitive");
static_assert(key_from_name("LeftArrow") == Key::Left, "primary names resolve");
static_assert(key_from_name("left") == Key::Left, "aliases resolve");
static_assert(key_from_name("nope") == Key::Unknown, "unknown names give Unknown");
static_assert(key_string(Key::NumpadEnter) == "NumpadEnter", "key_string reads the table");
static_assert(physical(Key::W) == Key::W, "PhysicalKey compares with Key");
static_assert(modifier_of(Key::RightControl) == Modifiers::Control, "modifier_of maps both sides");

windows::WindowEvent key_event(bool down, Key phys, Key logi) {
    windows::WindowEvent e;
    e.type = down ? windows::WindowEventType::KeyPress : windows::WindowEventType::KeyRelease;
    e.physical_key = phys;
    e.logical_key = logi;
    return e;
}

void test_names() {
    for (std::size_t i = 1; i < kKeyCount; ++i) {
        const Key k = static_cast<Key>(i);
        check(key_from_name(key_name(k)) == k, std::string("round trip ") + key_name(k));
        check(key_from_name(std::string(key_name(k))) == k, std::string("round trip std::string ") + key_name(k));
    }
    for (const detail::KeyName& a : detail::kKeyAliases) check(key_from_name(a.name) == a.key, std::string("alias ") + a.name);
    check(modifiers_string(Modifiers::Control | Modifiers::Shift) == "Ctrl+Shift", "modifiers_string");
}

void test_events() {
    const windows::WindowEvent e = key_event(true, Key::Q, Key::A);
    check(e.is_key(physical(Key::Q)) && !e.is_key(physical(Key::A)), "event physical match");
    check(e.is_key(logical(Key::A)) && !e.is_key(logical(Key::Q)), "event logical match");
    check(e.is_key(Key::Q) && e.is_key(Key::A), "event any match");
    check(e.key_of(KeyKind::Any) == Key::A, "any prefers logical");
    check(std::string(e.physical_name()) == "Q" && std::string(e.logical_name()) == "A", "event names");
    check(!windows::WindowEvent().is_key(Key::Unknown), "Unknown never matches");
}

void test_manager() {
    windows::WindowEventHandler h;
    InputManager im;
    im.connect(h);

    h.dispatch_event(key_event(true, Key::Q, Key::A));
    check(im.is_physical_key_down(Key::Q) && !im.is_physical_key_down(Key::A), "physical down");
    check(im.is_logical_key_down(Key::A) && !im.is_logical_key_down(Key::Q), "logical down");
    check(im.is_key_down(physical(Key::Q)) && im.is_key_down(logical(Key::A)), "tagged down");
    check(im.is_key_down(Key::A, KeyKind::Any) && im.is_key_down(Key::Q, KeyKind::Any), "any down");
    check(!im.is_key_down(Key::Q, KeyKind::Logical), "kind logical rejects physical");
    check(im.is_logical_key_just_pressed("a") && im.is_physical_key_just_pressed("Q"), "named just pressed");
    check(im.is_key_down(std::string("A")), "legacy string lookup is logical");

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    check(im.logical_key_hold_duration(Key::A) > 0.01, "logical hold duration");
    check(im.is_logical_key_held(Key::A, 0.01), "logical held");
    check(im.key_hold_duration(Key::A, KeyKind::Any) > 0.01, "any hold duration");

    im.end_frame();
    h.dispatch_event(key_event(false, Key::Q, Key::A));
    check(im.is_logical_key_just_released(Key::A) && im.is_physical_key_just_released(Key::Q), "just released");

    im.end_frame();
    h.dispatch_event(key_event(true, Key::Q, Key::A));
    check(im.is_logical_key_double_pressed(Key::A), "logical double press");
    check(im.is_physical_key_double_pressed(Key::Q), "physical double press");
    check(im.is_key_double_pressed(logical(Key::A)), "tagged double press");

    im.end_frame();
    check(!im.is_logical_key_double_pressed(Key::A), "double press clears each frame");

    im.reset();
    check(!im.is_logical_key_down(Key::A), "reset releases logical keys");
    h.dispatch_event(key_event(true, Key::Q, Key::A));
    check(im.is_logical_key_just_pressed(Key::A), "logical keys press again after reset");
}

} // namespace

int main() {
    test_names();
    test_events();
    test_manager();
    std::printf("input tests: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
