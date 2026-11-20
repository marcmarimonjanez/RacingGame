#pragma once

#include "Globals.h"
#include "ModulePhysics.h"
#include "Track.h"

#include "raylib.h"

// Base class for everything in the game that has a physics body.
// The ModuleGame owns the entities and destroys their bodies (PhysBody) using ModulePhysics.
class PhysicEntity
{
protected:

	PhysicEntity(PhysBody* _body) : body(_body)
	{
		body->owner = this;
	}

public:
	virtual ~PhysicEntity() = default;

	virtual void Draw() const
	{}

public:
	PhysBody* body;
};

// Static walls of the circuit (one sided chain loop)
class Wall : public PhysicEntity
{
public:
	Wall(ModulePhysics* physics, const int* points, int size);
};

// Loose barrel: it is pushed by the car and stopped by the friction of the floor (Lesson 5)
class Barrel : public PhysicEntity
{
public:
	Barrel(ModulePhysics* physics, int x, int y, int radius, float density);

	void Reset();
	// Called before every step of the world
	void ApplyForces(ModulePhysics* physics, const Track& track);
	void Draw() const override;

private:
	int home_x, home_y;
	int radius;
};

// What the driver asks the car to do
struct CarInput
{
	float throttle; // 0 .. 1
	float brake; // 0 .. 1 (reverse when the car is almost stopped)
	float steer; // -1 (left) .. 1 (right)
	bool handbrake;
};

// Car tuning. Accelerations are per unit of mass (m/s^2), so they do not depend on the mass of the car
#define CAR_HALF_LENGTH 28 // pixels
#define CAR_HALF_WIDTH 14
#define CAR_ENGINE_ACCEL 8.0f
#define CAR_BRAKE_ACCEL 12.0f
#define CAR_REVERSE_ACCEL 4.0f
#define CAR_REVERSE_SPEED 2.0f
#define CAR_HANDBRAKE_ACCEL 5.0f
#define CAR_DRAG 0.23f // air drag: a = -drag * |v| * v, it limits the top speed (about 5.6 m/s on asphalt)
#define CAR_TOP_SPEED 5.6f
#define CAR_TURN_RATE 2.4f // rad/s
#define CAR_STEER_RESPONSE 14.0f // rad/s^2
#define CAR_LATERAL_GAIN 0.85f

class Car : public PhysicEntity
{
public:
	Car(ModulePhysics* physics, Module* listener);

	// Puts the car in the start grid (start_index is a point of the centerline)
	void Reset(const Track& track, int start_index, float lateral);

	// Called before every step of the world: engine, brakes, drag and friction
	void ApplyForces(ModulePhysics* physics, const Track& track);

	void Draw() const override;

	float GetSpeed() const; // m/s
	float GetForwardSpeed() const; // m/s, negative when reversing
	Vector2 GetPosition() const; // pixels
	float GetAngle() const; // radians

public:
	CarInput input;

	SurfaceType surface;
	int track_index; // closest point of the centerline
	bool sliding; // the tyres are sliding (dynamic friction)
};
