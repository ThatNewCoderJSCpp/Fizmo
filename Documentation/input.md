# Keys: physical and logical

Every key in fizmo is a `fizmo::input::Key`, a `std::uint16_t` enum. Comparing keys, storing them in a `std::bitset`, and looking up their names are all plain integer operations, with no strings involved.

A key press carries two keys:

| | What it means | Use it for |
|---|---|---|
| **physical** | where the key sits on the keyboard, whatever the layout (`Key::W` is the key right of `Q` on a US board, even on AZERTY) | game controls like WASD |
| **logical** | what the layout says the key is (on AZERTY the key in the `Q` spot is logically `Key::A`) | shortcuts like Ctrl+Z, menus |

Text is separate from both. Typed characters come as `TextInput` events (`WindowEvent::text`), so Shift+2 can be `@` or `"` depending on the layout without changing either key.

## Names

```cpp
using namespace fizmo::input;

const char* a = key_name(Key::NumpadEnter);
std::string_view b = key_string(Key::NumpadEnter);
Key c = key_from_name("numpadenter");
Key d = key_from_string("Esc");
```

`key_name` and `key_string` index a `constexpr` table. `key_from_name` hashes the name, case-insensitively, and binary-searches an index that is sorted at compile time, so it is cheap enough to use every frame and can also run in a `constexpr` context. Aliases such as `Esc`, `Return`, `Ctrl`, `PgUp`, and the shifted symbols (`!` is `Num1`) are accepted.

## Saying which key you mean

```cpp
Key k = Key::W;
PhysicalKey p = physical(Key::W);
LogicalKey l = logical(Key::Z);
KeyKind kind = KeyKind::Physical;
```

`KeyKind` is `Any`, `Logical` or `Physical`. `Any` matches either key, which is what the plain `Key` overloads did before, so existing code behaves the same.

## Events

```cpp
window.add_event_listener(WindowEventType::KeyPress, [](const WindowEvent& e) {
    if (e.is_key(physical(Key::W))) move_forward();
    if (e.is_key(logical(Key::Z)) && e.has_modifiers(Modifiers::Control)) undo();
    if (e.is_key(Key::Escape)) close_menu();
    Key where = e.key_of(KeyKind::Physical);
    const char* label = e.logical_name();
});
```

| Member | Gives |
|---|---|
| `physical_key`, `logical_key` | the two keys |
| `key_of(kind)` | one of them (`Any` prefers logical) |
| `is_key(Key)`, `is_key(PhysicalKey)`, `is_key(LogicalKey)`, `is_key(Key, KeyKind)` | whether this event is that key |
| `is_physical_key(k)`, `is_logical_key(k)` | the same, by name |
| `physical_name()`, `logical_name()` | `key_name` of either key, with no allocation |
| `has_modifiers(m)`, `chord_modifiers()` | modifier checks (`chord_modifiers` drops Caps Lock and Num Lock) |
| `key_name` | the old label string, still filled in |

Each platform fills in both keys:

- **Windows:** the scancode gives the physical key and the virtual key gives the logical one.
- **X11:** the keycode gives the physical key and the keysym of the active layout group gives the logical one.
- **macOS:** the key code gives the physical key, and the character the layout gives (ignoring modifiers) gives the logical one.
- **Android:** the scan code gives the physical key and the Android key code gives the logical one.

When a layout has no Latin key for a position (Cyrillic or Greek, say), the logical key falls back to the physical one.

## InputManager

Every query exists for both kinds of key:

| Physical | Logical | Either |
|---|---|---|
| `is_physical_key_down(k)` | `is_logical_key_down(k)` | `is_key_down(k, KeyKind::Any)` |
| `is_physical_key_just_pressed(k)` | `is_logical_key_just_pressed(k)` | `is_key_just_pressed(k, KeyKind::Any)` |
| `is_physical_key_just_released(k)` | `is_logical_key_just_released(k)` | `is_key_just_released(k, KeyKind::Any)` |
| `is_physical_key_double_pressed(k)` | `is_logical_key_double_pressed(k)` | `is_key_double_pressed(k, KeyKind::Any)` |
| `physical_key_hold_duration(k)` | `logical_key_hold_duration(k)` | `key_hold_duration(k, KeyKind::Any)` |
| `is_physical_key_held(k, s)` | `is_logical_key_held(k, s)` | `is_key_held(k, KeyKind::Any, s)` |

The same queries also take `physical(k)` or `logical(k)`, as in `is_key_down(physical(Key::W))`, and the physical and logical versions take a name (`is_logical_key_down("z")`). The original `is_key_down(Key)` family is physical and the original `is_key_down(const std::string&)` family is logical, as before.

```cpp
InputManager input;
input.connect(window.event_handler());

if (input.is_key_down(physical(Key::W))) player.forward();
if (input.is_logical_key_just_pressed(Key::Z) && input.has_modifiers(Modifiers::Control)) undo();
```

## Keybinds

`ui::Keybind` has a `key_kind`. `Keybind::from_key(k)` is `Any`, as before. `from_physical_key(k)`, `from_logical_key(k)`, `from_key(physical(k))` and `from_key(k, kind, mods)` pick one. As text, a kind other than `Any` is written before the key:

```
Ctrl+Z
Physical:W
Ctrl+Shift+Logical:S
```

`Keybind::parse` reads all three forms, and `KeybindField::set_capture_kind(KeyKind::Physical)` makes the field record physical keys.

## Modifiers

`Modifiers` is a bit mask with `|`, `&`, `^`, `~`, `|=`, `&=`, `^=`, `has`, `has_all`, `has_any`, `without`, `modifier_of(Key)` and `modifiers_string`. `Key`, `PhysicalKey`, `LogicalKey`, `KeyKind` and `Modifiers` can all be written to a `std::ostream`.
