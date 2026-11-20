#pragma once

#include "Module.h"
#include "Globals.h"

#include "box2d/box2d.h"

#include <vector>

// TOP VIEW game: there is no gravity pulling bodies on the screen (the world gravity is zero).
// GRAVITY_Z is the gravity that pushes the bodies against the floor: it gives the normal force N = m * g
// used by the friction (Lesson 5).
#define GRAVITY_X 0.0f
#define GRAVITY_Y 0.0f
#define GRAVITY_Z 9.8f

// Units: Box2D works in meters (float), raylib draws in pixels (int)
#define PIXELS_PER_METER 50.0f // if touched change METER_PER_PIXEL too
#define METER_PER_PIXEL 0.02f  // this is 1 / PIXELS_PER_METER !

#define METERS_TO_PIXELS(m) ((int) floor(PIXELS_PER_METER * (m)))
#define PIXEL_TO_METERS(p)  ((float) METER_PER_PIXEL * (p))

// Fixed time step. The forces are applied before EVERY step (see Module::OnPhysicsStep) so the simulation
// does not depend on the FPS. The friction needs to know this dt to predict the next velocity.
#define PHYSICS_DT (1.0f / 60.0f)
#define VELOCITY_ITERATIONS 8
#define POSITION_ITERATIONS 3
#define MAX_STEPS_PER_FRAME 8 // avoids the "spiral of death" with a very low FPS

enum PhysType
{
	PT_NONE,
	PT_CAR,
	PT_WALL,
	PT_BARREL
};

struct PhysMaterial
{
	float density;
	float friction; // Box2D contact friction (bodies against bodies). The floor friction is ours.
	float restitution;
};

class PhysicEntity;

// Small class to return to other modules to track position and rotation of physics bodies
class PhysBody
{
public:
	PhysBody() : body(NULL), listener(NULL), type(PT_NONE), owner(NULL)
	{}

	void GetPosition(int& x, int& y) const;
	float GetRotation() const;
	bool Contains(int x, int y) const;
	int RayCast(int x1, int y1, int x2, int y2, float& normal_x, float& normal_y) const;

public:
	b2Body* body;
	Module* listener; // module that listens to the collisions of this body
	PhysType type;
	PhysicEntity* owner; // game object that owns this body (can be NULL)
};

// Module --------------------------------------
class ModulePhysics : public Module, public b2ContactListener
{
public:
	ModulePhysics(Application* app, bool start_enabled = true);
	~ModulePhysics();

	bool Start();
	update_status PreUpdate();
	update_status PostUpdate();
	bool CleanUp();

	// Bodies (positions and sizes in PIXELS) -------------------------------------------------
	PhysBody* CreateCircle(int x, int y, int radius, b2BodyType type, PhysType ptype, const PhysMaterial& material);
	PhysBody* CreateBox(int x, int y, int half_width, int half_height, b2BodyType type, PhysType ptype, const PhysMaterial& material, float angle_rad = 0.0f);
	// points: x0, y0, x1, y1, ... (size = number of ints). One sided loop: it collides with what is on the LEFT
	// of the direction of travel, so counter-clockwise ON SCREEN = collides from the inside
	PhysBody* CreateChain(const int* points, int size, PhysType ptype, const PhysMaterial& material);
	void DestroyBody(PhysBody* pbody);

	// Friction (Lesson 5) ---------------------------------------------------------------------
	// Top view friction of a body sliding on the floor.
	//  - Static: the body is at rest, it holds the applied force up to mu_s * N
	//  - Dynamic: the body slides, constant force mu_k * N opposite to the velocity (it never pushes backwards)
	// applied_force: sum of the other forces that push the body in the plane. IMPORTANT: they have already been
	// applied to the body (ApplyForceToCenter); the friction cancels them when the body stays still.
	void ApplyFriction(PhysBody* pbody, const b2Vec2& applied_force, float static_friction, float dynamic_friction);
	// The same for the rotation (a barrel that spins after a hit, a car spinning on the ice). radius: size of the body
	void ApplyAngularFriction(PhysBody* pbody, float static_friction, float dynamic_friction, float radius);

	// The module that is called before every step of the world
	void SetStepListener(Module* module);

	// Debug mode (F1): draws every shape and lets the player drag bodies with a mouse joint
	bool IsDebug() const;
	void DrawDebug();

	// b2ContactListener
	void BeginContact(b2Contact* contact) override;

private:
	PhysBody* CreateBody(int x, int y, b2BodyType type, PhysType ptype, float angle_rad);

private:
	bool debug;
	b2World* world;
	float accumulator;
	Module* step_listener;

	// Every wrapper we create, so nothing leaks even if a module forgets to destroy one
	std::vector<PhysBody*> pbodies;

	b2Body* ground; // needed by the mouse joint
	b2MouseJoint* mouse_joint;
};
