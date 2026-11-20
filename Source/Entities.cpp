#include "Globals.h"
#include "Entities.h"

#include <math.h>

static const PhysMaterial MAT_WALL = { 1.0f, 0.2f, 0.2f };
static const PhysMaterial MAT_CAR = { 1.0f, 0.2f, 0.25f };

static float Clamp(float value, float min, float max)
{
	return (value < min) ? min : (value > max) ? max : value;
}

// Wall -----------------------------------------------------------------------------------------

Wall::Wall(ModulePhysics* physics, const int* points, int size)
	: PhysicEntity(physics->CreateChain(points, size, PT_WALL, MAT_WALL))
{
}

// Barrel ---------------------------------------------------------------------------------------

Barrel::Barrel(ModulePhysics* physics, int x, int y, int _radius, float density)
	: PhysicEntity(physics->CreateCircle(x, y, _radius, b2_dynamicBody, PT_BARREL, PhysMaterial{ density, 0.2f, 0.35f }))
	, home_x(x)
	, home_y(y)
	, radius(_radius)
{
}

void Barrel::Reset()
{
	body->body->SetTransform(b2Vec2(PIXEL_TO_METERS(home_x), PIXEL_TO_METERS(home_y)), 0.0f);
	body->body->SetLinearVelocity(b2Vec2_zero);
	body->body->SetAngularVelocity(0.0f);
}

void Barrel::ApplyForces(ModulePhysics* physics, const Track& track)
{
	b2Vec2 pos = body->body->GetPosition();
	float x = pos.x * PIXELS_PER_METER;
	float y = pos.y * PIXELS_PER_METER;

	// The friction depends on the floor under the barrel
	int nearest = track.FindNearest(x, y);
	const Surface& floor = track.GetSurface(track.GetSurfaceType(x, y, nearest));

	// No other forces: the barrel only moves when something hits it. Lesson 5 friction for the movement and the spin
	physics->ApplyFriction(body, b2Vec2_zero, floor.static_friction, floor.dynamic_friction);
	physics->ApplyAngularFriction(body, floor.static_friction, floor.dynamic_friction, PIXEL_TO_METERS(radius));
}

void Barrel::Draw() const
{
	// Placeholder graphics: a flat circle and a line to see the rotation
	int x, y;
	body->GetPosition(x, y);

	float angle = body->GetRotation();
	DrawCircle(x, y, (float)radius, ORANGE);
	DrawLine(x, y, x + (int)(cosf(angle) * radius), y + (int)(sinf(angle) * radius), BLACK);
}

// Car ------------------------------------------------------------------------------------------

Car::Car(ModulePhysics* physics, Module* listener)
	: PhysicEntity(physics->CreateBox(0, 0, CAR_HALF_LENGTH, CAR_HALF_WIDTH, b2_dynamicBody, PT_CAR, MAT_CAR))
	, input{ 0.0f, 0.0f, 0.0f, false }
	, surface(SURF_ASPHALT)
	, track_index(0)
	, sliding(false)
{
	body->listener = listener;
}

void Car::Reset(const Track& track, int start_index, float lateral)
{
	Vector2 p = track.GetPoint(start_index);
	Vector2 t = track.GetTangent(start_index);
	Vector2 l = track.GetNormalLeft(start_index);

	float x = p.x + l.x * lateral;
	float y = p.y + l.y * lateral;

	body->body->SetTransform(b2Vec2(PIXEL_TO_METERS(x), PIXEL_TO_METERS(y)), atan2f(t.y, t.x));
	body->body->SetLinearVelocity(b2Vec2_zero);
	body->body->SetAngularVelocity(0.0f);
	body->body->SetAwake(true);

	input = CarInput{ 0.0f, 0.0f, 0.0f, false };
	surface = SURF_ASPHALT;
	track_index = start_index;
	sliding = false;
}

float Car::GetSpeed() const
{
	return body->body->GetLinearVelocity().Length();
}

float Car::GetForwardSpeed() const
{
	float a = body->body->GetAngle();
	return b2Dot(body->body->GetLinearVelocity(), b2Vec2(cosf(a), sinf(a)));
}

Vector2 Car::GetPosition() const
{
	b2Vec2 p = body->body->GetPosition();
	return Vector2{ p.x * PIXELS_PER_METER, p.y * PIXELS_PER_METER };
}

float Car::GetAngle() const
{
	return body->body->GetAngle();
}

void Car::ApplyForces(ModulePhysics* physics, const Track& track)
{
	b2Body* b = body->body;
	float mass = b->GetMass();
	float normal = mass * GRAVITY_Z; // N = m * g

	// Where are we and what is under the tyres ------------------------------------------------
	Vector2 position = GetPosition();
	track_index = track.FindNearest(position.x, position.y, track_index);
	surface = track.GetSurfaceType(position.x, position.y, track_index);
	const Surface& floor = track.GetSurface(surface);

	// Directions of the car: heading and right (Y grows downwards, a positive angle is clockwise on screen)
	float angle = b->GetAngle();
	b2Vec2 heading(cosf(angle), sinf(angle));
	b2Vec2 right(-heading.y, heading.x);

	b2Vec2 velocity = b->GetLinearVelocity();
	float forward_speed = b2Dot(velocity, heading);
	float side_speed = b2Dot(velocity, right);
	float speed = velocity.Length();

	// 1. Engine and brakes. A tyre can not push more than the friction allows: mu_s * g (ice: almost nothing)
	float max_accel = floor.static_friction * GRAVITY_Z;
	float accel = CAR_ENGINE_ACCEL * input.throttle;

	if (input.brake > 0.0f)
	{
		if (forward_speed > 0.3f)
			accel -= CAR_BRAKE_ACCEL * input.brake;
		else if (forward_speed > -CAR_REVERSE_SPEED)
			accel -= CAR_REVERSE_ACCEL * input.brake;
	}

	if (input.handbrake && forward_speed > 0.0f)
		accel -= CAR_HANDBRAKE_ACCEL;

	accel = Clamp(accel, -max_accel, max_accel);
	b2Vec2 drive = mass * accel * heading;

	// 2. Air drag (quadratic): it gives the car a top speed
	b2Vec2 drag = -CAR_DRAG * mass * speed * velocity;

	b->ApplyForceToCenter(drive, true);
	b->ApplyForceToCenter(drag, true);

	// 3. Rolling resistance of the floor: friction of Lesson 5 (static when the car is stopped, dynamic when it moves)
	physics->ApplyFriction(body, drive + drag, floor.rolling * 1.2f, floor.rolling);

	// 4. Lateral friction of the tyres: they do not let the car slide sideways.
	//    - Static (grip): if the force needed to cancel the lateral velocity in one step is below mu_s * N, they hold
	//    - Dynamic (slide): otherwise the tyres slide and the force is only mu_k * N (the car drifts)
	float grip = input.handbrake ? 0.30f : 1.0f; // the handbrake locks the rear wheels
	float static_limit = floor.static_friction * grip * normal;
	float dynamic_force = floor.dynamic_friction * grip * normal;

	float needed = CAR_LATERAL_GAIN * mass * fabsf(side_speed) / PHYSICS_DT;
	float side_sign = (side_speed > 0.0f) ? 1.0f : -1.0f;

	if (needed <= static_limit)
	{
		sliding = false;
		b->ApplyForceToCenter(-needed * side_sign * right, true);
	}
	else
	{
		sliding = true;
		b->ApplyForceToCenter(-dynamic_force * side_sign * right, true);
	}

	// 5. Steering. The turn rate depends on the speed and on the grip of the surface (on ice the front wheels
	//    can not turn the car). When the driver does not steer, the angular friction stops the spin.
	if (input.steer != 0.0f)
	{
		float speed_factor = Clamp(fabsf(forward_speed) / 2.0f, 0.3f, 1.0f) * (1.0f - 0.35f * Clamp(speed / CAR_TOP_SPEED, 0.0f, 1.0f));
		float grip_factor = Clamp(floor.static_friction / 0.8f, 0.0f, 1.0f);
		float target = input.steer * CAR_TURN_RATE * speed_factor * grip_factor;
		if (forward_speed < 0.0f)
			target = -target; // reversing inverts the steering

		float omega = b->GetAngularVelocity();
		omega += Clamp(target - omega, -CAR_STEER_RESPONSE * PHYSICS_DT, CAR_STEER_RESPONSE * PHYSICS_DT);
		b->SetAngularVelocity(omega);
	}
	else
	{
		physics->ApplyAngularFriction(body, floor.static_friction, floor.dynamic_friction, PIXEL_TO_METERS(CAR_HALF_LENGTH));
	}
}

void Car::Draw() const
{
	// Placeholder graphics: a flat rectangle with a "windshield" at the front
	Vector2 p = GetPosition();
	float angle = GetAngle();

	Rectangle body_rect = { p.x, p.y, (float)(CAR_HALF_LENGTH * 2), (float)(CAR_HALF_WIDTH * 2) };
	DrawRectanglePro(body_rect, Vector2{ (float)CAR_HALF_LENGTH, (float)CAR_HALF_WIDTH }, angle * RAD2DEG, sliding ? ORANGE : RED);

	Rectangle front_rect = { p.x + cosf(angle) * 14.0f, p.y + sinf(angle) * 14.0f, 10.0f, (float)(CAR_HALF_WIDTH * 2 - 6) };
	DrawRectanglePro(front_rect, Vector2{ 5.0f, (float)(CAR_HALF_WIDTH - 3) }, angle * RAD2DEG, DARKGRAY);
}
