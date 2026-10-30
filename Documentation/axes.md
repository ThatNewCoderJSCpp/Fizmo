# Axes

`#include <axes.hpp>` (also pulled in by `basic_vectors.hpp`, physics and the cameras).

One place decides which way is up, which way is forward and which way a positive turn goes. Physics gravity, `RigidBody3D::set_yaw_pitch_roll`, `Camera3D` and `Camera2D` all read it, so a world built in Blender's Z-up or Unreal's left-handed Z-up looks and falls the same way it does in the default.

## 3D presets

| `AxesPreset` | `name()` | Up | Forward | Right | Also parses | Matches |
|---|---|---|---|---|---|---|
| `YUpRight` (default) | `y-up` | +Y | -Z | +X | `opengl`, `godot` | Fizmo, OpenGL, Godot, Sapling |
| `YUpLeft` | `y-up-left` | +Y | +Z | +X | `unity`, `directx` | Unity |
| `ZUpRight` | `z-up` | +Z | +Y | +X | `blender` | Blender, Arbor |
| `ZUpLeft` | `z-up-left` | +Z | +X | +Y | `unreal` | Unreal |

Angles follow the preset. Positive yaw turns counter-clockwise seen from above in right-handed presets and clockwise in left-handed ones. Positive pitch always looks up.

```cpp
fizmo::set_default_axes(fizmo::AxesPreset::ZUpRight);

fizmo::physics::Simulation3D sim;                 // gravity is now (0, 0, -g)
sim.set_axes(fizmo::Axes3D::y_up());              // or switch one simulation back

fizmo::graphics::Camera3D cam;                    // picks up the default axes
cam.set_axes(fizmo::Axes3D::z_up());
cam.orbit({0, 0, 0}, 30.0, 20.0, 8.0);            // target, yaw, pitch, distance
cam.look_at({0, 0, 1});                           // up hint is the axes' up

body.set_yaw_pitch_roll(fizmo::Axes3D::z_up(), 90.0, 0.0, 0.0, false);
```

`Axes3D` helpers: `up()`, `down()`, `forward()`, `back()`, `right()`, `left()`, `gravity(g)`, `height(p)`, `flatten(v)`, `from_yaw_pitch_roll`, `to_yaw_pitch_roll`, `direction(yaw, pitch)`, `orbit_view`, `orbit_unview`, `orbit_eye`, `to_render`, `from_render`, `name()`, `parse(text)`.

`orbit_view(offset, yaw, pitch)` returns a view-space point (x right, y up, z toward the viewer). It is the same maths Sapling's and Arbor's software cameras used, so they can share it.

Left-handed presets are drawn mirrored: the renderer flips face culling for the frame and keeps textured quads upright, so meshes look the same as in the tool they came from.

## 2D

| `YAxis2D` | `name()` | Up on screen | Gravity |
|---|---|---|---|
| `Up` (default) | `y-up` | +Y | (0, -g) |
| `Down` | `y-down` | -Y | (0, +g) |

`Camera2D` flips its view for `y-up`, so physics positions and drawing positions agree. Use `set_default_axes(fizmo::YAxis2D::Down)` or `camera.set_axes(fizmo::Axes2D::y_down())` for the old screen-style coordinates.
