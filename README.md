# Racing Game - Phase 1 (physics prototype)

Top view racing game for the **Physics II** subject (Lesson 5: custom friction), CITM - UPC. C++, raylib and Box2D.
It starts from the code of our Pinball project (same modules and setup).

The project is planned in 3 phases. **This is phase 1: the physics prototype.**

| Phase | Goal |
|---|---|
| **1 (this one)** | A car that drives on a circuit with walls. Custom friction (static / dynamic / angular), fixed time step, loose barrels. Placeholder graphics (flat colors), no sound |
| 2 | The race: laps and timer, AI opponents, sand and ice, sounds, collision effects |
| 3 | Polish: art (track, cars), skid marks and dust, HUD, title / countdown / results screens, music |

- **GitHub:** https://github.com/<user>/<repository>

## Developers

- Marc Marimon Janez

## Controls

| Key | Action |
|---|---|
| W / UP | Accelerate |
| S / DOWN | Brake, and reverse when the car is almost stopped |
| A, D / LEFT, RIGHT | Steer |
| SPACE | Handbrake (the rear tyres lose grip: drift) |
| R | Put the car and the barrels back in place |
| Z / X | Asphalt static friction - / + |
| C / V | Asphalt dynamic friction - / + |
| F4 / F5 | Target FPS - / + |
| F1 | Debug mode: draws every shape, drag bodies with the mouse (mouse joint) |

The screen only shows the circuit and the car (no HUD). The keys below still work.

## What is in phase 1

There is no gravity on the screen (top view). `GRAVITY_Z` (9.8) only gives the normal force `N = m * g` used by the friction.

- **Static and dynamic friction (Lesson 5):** `ModulePhysics::ApplyFriction`. At rest the body holds the applied force up to
  `mu_s * N`; once it moves a constant force `mu_k * N` opposes the velocity and it never pushes the body backwards.
  The barrels use it with the coefficients of the floor under them.
- **Angular friction:** `ModulePhysics::ApplyAngularFriction`.
- **Car** (`Car::ApplyForces`): engine and brakes limited by the grip, air drag (top speed), rolling resistance and a **lateral tyre friction**
  (grip while the needed force is below `mu_s * N`, otherwise the tyres slide with `mu_k * N` and the car drifts). The handbrake reduces the grip.
- **Surfaces:** asphalt and grass (`Track`), each with static, dynamic and rolling coefficients.
- **Fixed time step** (1/60 s) with an accumulator. The forces are applied before EVERY step through `Module::OnPhysicsStep`.
- **Walls:** two one sided chain loops generated from the centerline of the circuit (`Tools/generate_track.py` writes `Source/TrackData.h`).
- No memory leaks: `ModulePhysics` creates and destroys every `PhysBody`.

## Project structure

- `Source/ModulePhysics.*`: Box2D world, bodies, friction functions, fixed step, debug draw, mouse joint.
- `Source/Entities.*`: `Wall`, `Barrel`, `Car`.
- `Source/Track.*`, `Source/TrackData.h`: circuit and surfaces (`TrackData.h` is generated).
- `Source/ModuleGame.*`: input, world and placeholder drawing.

## Build

Open `PhysicsGame.sln` with Visual Studio 2022 and build the **Release** configuration.

## License

MIT, see `LICENSE`.
