# Numerics

`fizmo::numerics` solves ODEs and PDEs, sparse and dense linear systems and nonlinear equations, and turns sampled functions into geometry for drawing (contours, isosurfaces, meshes, curves and color maps). It is the `numerics` component (`fizmo::numerics` in CMake), which only needs `fizmo::core`.

```cpp
#define FIZMO
#define FIZMO_NUMERICS
#include <fizmo/includes.hpp>

namespace num = fizmo::numerics;
```

`ALL_FIZMO` turns it on along with everything else.

## Dense linear algebra (`dense.hpp`)

Templates on the number type `T` (`float`, `double`, `long double`).

| | |
|---|---|
| `DenseMatrix<T>(rows, cols)` | row-major matrix with `m(r, c)`, `identity(n)` and `m * vector` |
| `DenseLU<T>` | `factor(a)` with partial pivoting, `solve(b)` in place, `determinant()` |
| `solve_dense(a, b)` | factor and solve in one call |
| `numeric_jacobian(f, x, fx, J)` | forward-difference Jacobian of a `VectorFunction<T>` |
| `newton_dense(f, x, jacobian = nullptr, tolerance, max_iterations)` | Newton's method with a backtracking line search; numeric Jacobian when none is given. Returns `NewtonResult<T>{ converged, iterations, residual }` |

## Sparse linear algebra (`sparse.hpp`)

Square matrices in compressed sparse row form, in `double`.

| | |
|---|---|
| `CsrMatrix::from_triplets(n, entries)` | builds from `Triplet{ row, col, value }`s (duplicates are added, and a zero diagonal is kept by default) |
| `multiply`, `operator*`, `at`, `bandwidth`, `scale_rows` | |
| `BandedLU` | direct solver with partial pivoting inside the band; `BandedLU::cost(a)` estimates the work |
| `Ilu0` | incomplete LU with no fill, as a preconditioner |
| `gmres(a, &ilu, b, x, tol, max_iter, restart)` | restarted GMRES, right preconditioned |
| `bicgstab(a, &ilu, b, x, tol, max_iter)` | BiCGSTAB |
| `conjugate_gradient(a, b, x, tol, max_iter)` | Jacobi-preconditioned CG for symmetric positive definite systems |
| `SparseSolver` | `factor` then `solve`; `SparseMethod::Automatic` picks banded LU when `cost <= band_budget` and GMRES with ILU(0) otherwise. `name()` and `iterations()` report what it did |

`newton.hpp` adds `newton_sparse(residual, jacobian, x, options)` for large nonlinear systems, where the Jacobian callback fills a `CsrMatrix`.

## ODEs (`ode.hpp`)

Systems are `OdeSystem<T>`: `void(const T& t, const std::vector<T>& y, std::vector<T>& dydt)`. Every solver returns an `OdeSolution<T>` with the times `t`, states `y` and derivatives `dydt` of every step, `at(time)` for cubic Hermite interpolation between them, `component(k)`, `events` that were hit, and `ok`/`message`.

| | |
|---|---|
| `solve_fixed(f, t0, y0, t1, steps, method)` | `Euler`, `Midpoint`, `Heun`, `Rk4`, and the implicit `BackwardEuler`, `Trapezoid`, `ImplicitMidpoint` and `Bdf2` (Newton on each step) |
| `solve_adaptive(f, t0, y0, t1, options)` | Dormand–Prince 5(4) with step size control; integrates backward when `t1 < t0` |
| `solve_stiff(f, t0, y0, t1, options)` | adaptive implicit trapezoid with step doubling, for stiff systems |
| `solve_symplectic(acceleration, t0, q0, v0, t1, steps, method)` | `Euler`, `Verlet` or `Yoshida4` for mechanics, where energy must not drift; `y` holds positions then velocities |
| `implicit_order_system(F, order)` | turns `F(x, { y, y', ..., y⁽ⁿ⁾ }) = 0` into a first-order system by solving for the highest derivative at each evaluation |
| `rk4_step(f, t, y, h, out)` | one RK4 step |

`OdeOptions<T>` holds the tolerances, initial, smallest and largest step, a step limit, `events` and a `cancelled` callback. An `OdeEvent<T>` is a function `g(t, y)`; the solver finds where it crosses zero (in a `direction` of -1, +1 or either), records it, and stops there if it's `terminal`.

Builtin floating types work out of the box. Another number type needs `OdeScalar<T>` specialized with `to_double` and `from_double` (used for step size control).

## PDEs (`pde.hpp`)

`solve_pde(problem, solution, error)` solves time-dependent and steady PDEs on a grid in one, two or three space dimensions, with any number of unknown functions. Every time step is implicit: Newton's method solves the whole grid at once, with a banded LU or GMRES with ILU(0) for the linear systems.

A problem lists its `terms`, each a derivative `PdeTerm(function, order_x, order_y, order_z, order_t)` of one of the functions (up to fourth order in each space variable), and one `PdeEquation` per function. An equation's `residual` is evaluated on many points at once from `PdeInputs` (`x()`, `y()`, `z()`, `t()` and `term(k)`) and should be zero at the solution; it lists the `terms` it uses, exact `partials` where it has them (the rest are found by finite differences), and whether it's `linear` (its partials don't depend on any term) and `constant` (they don't depend on `t` either; then the matrix is factored once).

Each function's highest time derivative in the equations sets how many starting values it needs (`initial`); a function with no time derivative is solved along with the others at every step. The highest time derivative can appear inside space derivatives (`u_xxt`) or nonlinearly.

Each edge (`edges[axis][side]`) is `Periodic`, `Insulated`, `Free` or a list of `PdeCondition`s, each an equation for one function. A condition without a derivative across the edge fixes the value there; the others (like `u_x = 0` or `u_x + u = 0`) set the ghost points beyond it, so fourth-order equations take two conditions per edge.

`PdeMethod` is `Trapezoid` (Crank–Nicolson, keeps waves and probability), `Bdf2` (damps stiff noise) or `BackwardEuler`. `steps` and `frames` set the time step and how many snapshots are kept; `poll` lets the caller abort (`Abort`) or stop early and keep what's done (`Finish`).

`PdeSolution` holds the grid (`n`, `lo`, `hi`, `periodic`, `coord`, `spacing`), the snapshot `times` and `frames`, `value(frame, function, i, j, k)`, `at_time(t, function)` (interpolated between snapshots), and what the solver did (`steps`, `newton`, `linear`, `solver`, `dt`, `blew_up`, `problem`).

## Geometry (`geometry.hpp`)

| | |
|---|---|
| `colormap(name, t)`, `colormap_range(name, value, lo, hi)` | `viridis`, `magma`, `turbo`, `coolwarm`, `ocean`, `gray` |
| `sample_grid(f, ...)` | samples a function of two or three variables into a `ScalarGrid2` or `ScalarGrid3` |
| `marching_squares(grid, level, refine)` | contour segments, two points each; `refine` sharpens them by bisecting the real function |
| `join_segments(segments)` | joins contour segments into polylines |
| `marching_tetrahedra(grid, level)` | isosurface triangles; `mesh_from_soup` indexes them and adds normals |
| `surface_mesh(f, u0, u1, v0, v1, nu, nv)`, `height_mesh(grid)` | `TriangleMesh` with positions, normals, indices and optional colors (`color_by`, `color_by_height`) |
| `mesh_grid_lines(...)` | the parameter lines of a surface |
| `sample_curve(f, t0, t1, tolerance)` | adaptive sampling of 2D and 3D curves, refined where they bend |
