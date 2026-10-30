# Friction and bounce mixing

When two colliders touch, each has its own friction and restitution (bounce), and the solver needs one value for the contact. `fizmo::physics::CombineMode` says how the two are mixed. Every material picks a mode for friction and one for restitution, and the simulation has a default for materials that don't pick one.

| `CombineMode` | Result | Used by |
|---|---|---|
| `Default` | the simulation's mode | |
| `Average` | (a + b) / 2 | Unity, Unreal and PhysX's default |
| `GeometricMean` | sqrt(a · b) | Box2D friction; Fizmo's default for friction |
| `HarmonicMean` | 2ab / (a + b) | |
| `QuadraticMean` | sqrt((a² + b²) / 2) | |
| `Minimum` | min(a, b) | |
| `Multiply` | a · b | Bullet's default |
| `Maximum` | max(a, b) | Box2D restitution; Fizmo's default for restitution |
| `Sum` | a + b | |
| `Custom` | whatever your callback returns | Bullet's contact-added callback |

When the two colliders pick different modes, the one with the higher priority wins, in the order of the table (`Average` lowest, `Custom` highest). That is the rule Unity, Unreal and PhysX use for their four modes (Average < Minimum < Multiply < Maximum), with the extra modes slotted in.

```cpp
fizmo::physics::Simulation3D::Config cfg;
cfg.friction_combine = fizmo::physics::CombineMode::Average;
cfg.restitution_combine = fizmo::physics::CombineMode::Maximum;
fizmo::physics::Simulation3D sim(cfg);

fizmo::physics::Material3D ice(900.0);
ice.friction = 0.02;
ice.friction_combine = fizmo::physics::CombineMode::Multiply;

sim.set_friction_combiner([](const fizmo::physics::Collider3D& a, const fizmo::physics::Collider3D& b) {
    return std::min(a.material.friction, b.material.friction) * 0.5;
});
```

`combine_values(mode, a, b)`, `resolve_combine(a_mode, b_mode, fallback)`, `combined_friction(a, b)` and `combined_restitution(a, b)` give the same numbers the solver uses, and `to_string` / `parse_combine_mode` convert names like `geometric-mean` and `rms`. The 2D simulation has the same settings with `Material` and `Collider2D`.
