#include "Globals.h"
#include "Application.h"
#include "ModuleRender.h"
#include "ModulePhysics.h"

#include <math.h>
#include <algorithm>

static void FillFixture(b2FixtureDef& fixture, const b2Shape* shape, const PhysMaterial& material)
{
	fixture.shape = shape;
	fixture.density = material.density;
	fixture.friction = material.friction;
	fixture.restitution = material.restitution;
}

ModulePhysics::ModulePhysics(Application* app, bool start_enabled) : Module(app, start_enabled)
{
	world = NULL;
	ground = NULL;
	mouse_joint = NULL;
	step_listener = NULL;
	debug = false;
	accumulator = 0.0f;
}

ModulePhysics::~ModulePhysics()
{
}

bool ModulePhysics::Start()
{
	LOG("Creating Physics 2D environment");

	// Top view: no gravity on the screen plane
	world = new b2World(b2Vec2(GRAVITY_X, GRAVITY_Y));
	world->SetContactListener(this);

	b2BodyDef bd;
	ground = world->CreateBody(&bd);

	return true;
}

update_status ModulePhysics::PreUpdate()
{
	// Fixed time step with an accumulator: real time at any FPS. The forces are applied BEFORE every step
	// because Box2D clears them at the end of each Step().
	float frame_time = GetFrameTime();
	if (frame_time > PHYSICS_DT * MAX_STEPS_PER_FRAME)
		frame_time = PHYSICS_DT * MAX_STEPS_PER_FRAME;

	accumulator += frame_time;

	while (accumulator >= PHYSICS_DT)
	{
		if (step_listener != NULL)
			step_listener->OnPhysicsStep();

		world->Step(PHYSICS_DT, VELOCITY_ITERATIONS, POSITION_ITERATIONS);
		accumulator -= PHYSICS_DT;
	}

	return UPDATE_CONTINUE;
}

update_status ModulePhysics::PostUpdate()
{
	if (IsKeyPressed(KEY_F1))
		debug = !debug;

	if (!debug)
	{
		if (mouse_joint != NULL)
		{
			world->DestroyJoint(mouse_joint);
			mouse_joint = NULL;
		}
		return UPDATE_CONTINUE;
	}

	b2Vec2 mouse_position(PIXEL_TO_METERS(GetMouseX()), PIXEL_TO_METERS(GetMouseY()));
	b2Body* body_clicked = NULL;

	// Click on a dynamic body: create a mouse joint
	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouse_joint == NULL)
	{
		for (b2Body* b = world->GetBodyList(); b && body_clicked == NULL; b = b->GetNext())
		{
			if (b->GetType() != b2_dynamicBody)
				continue;

			for (b2Fixture* f = b->GetFixtureList(); f; f = f->GetNext())
			{
				if (f->TestPoint(mouse_position))
				{
					body_clicked = b;
					break;
				}
			}
		}
	}

	if (body_clicked != NULL)
	{
		b2MouseJointDef def;
		def.bodyA = ground;
		def.bodyB = body_clicked;
		def.target = mouse_position;
		def.maxForce = 500.0f * body_clicked->GetMass();
		b2LinearStiffness(def.stiffness, def.damping, 5.0f, 0.7f, def.bodyA, def.bodyB);

		mouse_joint = (b2MouseJoint*)world->CreateJoint(&def);
		body_clicked->SetAwake(true);
	}

	if (mouse_joint != NULL && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
		mouse_joint->SetTarget(mouse_position);

	if (mouse_joint != NULL && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
	{
		world->DestroyJoint(mouse_joint);
		mouse_joint = NULL;
	}

	return UPDATE_CONTINUE;
}

bool ModulePhysics::CleanUp()
{
	LOG("Destroying physics world");

	for (PhysBody* pbody : pbodies)
		delete pbody;
	pbodies.clear();

	// Frees every body, fixture and joint of the world
	delete world;
	world = NULL;
	ground = NULL;
	mouse_joint = NULL;

	return true;
}

void ModulePhysics::SetStepListener(Module* module)
{
	step_listener = module;
}

// Bodies ---------------------------------------------------------------------------------------

PhysBody* ModulePhysics::CreateBody(int x, int y, b2BodyType type, PhysType ptype, float angle_rad)
{
	PhysBody* pbody = new PhysBody();
	pbody->type = ptype;

	b2BodyDef body;
	body.type = type;
	body.position.Set(PIXEL_TO_METERS(x), PIXEL_TO_METERS(y));
	body.angle = angle_rad;
	body.userData.pointer = reinterpret_cast<uintptr_t>(pbody); // PhysBody pointer as UserData, BEFORE CreateBody

	pbody->body = world->CreateBody(&body);
	pbodies.push_back(pbody);

	return pbody;
}

PhysBody* ModulePhysics::CreateCircle(int x, int y, int radius, b2BodyType type, PhysType ptype, const PhysMaterial& material)
{
	PhysBody* pbody = CreateBody(x, y, type, ptype, 0.0f);

	b2CircleShape shape;
	shape.m_radius = PIXEL_TO_METERS(radius);

	b2FixtureDef fixture;
	FillFixture(fixture, &shape, material);
	pbody->body->CreateFixture(&fixture);

	return pbody;
}

PhysBody* ModulePhysics::CreateBox(int x, int y, int half_width, int half_height, b2BodyType type, PhysType ptype, const PhysMaterial& material, float angle_rad)
{
	PhysBody* pbody = CreateBody(x, y, type, ptype, angle_rad);

	b2PolygonShape box;
	box.SetAsBox(PIXEL_TO_METERS(half_width), PIXEL_TO_METERS(half_height));

	b2FixtureDef fixture;
	FillFixture(fixture, &box, material);
	pbody->body->CreateFixture(&fixture);

	return pbody;
}

// size is the number of ints, not vertices
PhysBody* ModulePhysics::CreateChain(const int* points, int size, PhysType ptype, const PhysMaterial& material)
{
	PhysBody* pbody = CreateBody(0, 0, b2_staticBody, ptype, 0.0f);

	int count = size / 2;
	b2Vec2* vertices = new b2Vec2[count];
	for (int i = 0; i < count; ++i)
	{
		vertices[i].x = PIXEL_TO_METERS(points[i * 2 + 0]);
		vertices[i].y = PIXEL_TO_METERS(points[i * 2 + 1]);
	}

	b2ChainShape shape;
	shape.CreateLoop(vertices, count);
	delete[] vertices; // the shape copies them

	b2FixtureDef fixture;
	FillFixture(fixture, &shape, material);
	pbody->body->CreateFixture(&fixture);

	return pbody;
}

void ModulePhysics::DestroyBody(PhysBody* pbody)
{
	if (pbody == NULL)
		return;

	if (mouse_joint != NULL && mouse_joint->GetBodyB() == pbody->body)
		mouse_joint = NULL;

	world->DestroyBody(pbody->body);
	pbodies.erase(std::remove(pbodies.begin(), pbodies.end(), pbody), pbodies.end());
	delete pbody;
}

// Friction (Lesson 5) --------------------------------------------------------------------------

void ModulePhysics::ApplyFriction(PhysBody* pbody, const b2Vec2& applied_force, float static_friction, float dynamic_friction)
{
	b2Body* b = pbody->body;

	float mass = b->GetMass();
	float normal = mass * GRAVITY_Z; // N = m * g

	b2Vec2 velocity = b->GetLinearVelocity();
	b2Vec2 direction; // direction of the motion

	// At rest: static friction
	if (velocity.Length() < 0.01f)
	{
		// It holds while the applied force is not too big
		if (applied_force.Length() <= static_friction * normal)
		{
			b->SetLinearVelocity(b2Vec2_zero);
			b->ApplyForceToCenter(-applied_force, true); // cancels the applied force: nothing moves
			return;
		}

		// Too big: it starts sliding in the direction of the force (we only add the dynamic friction)
		direction = applied_force;
	}
	else
	{
		direction = velocity;
	}
	direction.Normalize();

	// Dynamic friction: constant force opposite to the motion
	b2Vec2 friction = -dynamic_friction * normal * direction;

	// Friction stops a body, it never pushes it backwards: if the next velocity points to the other side, stop
	b2Vec2 next_velocity = velocity + (PHYSICS_DT / mass) * (applied_force + friction);
	if (b2Dot(next_velocity, direction) <= 0.0f)
	{
		b->SetLinearVelocity(b2Vec2_zero);
		b->ApplyForceToCenter(-applied_force, true);
		return;
	}

	b->ApplyForceToCenter(friction, true);
}

void ModulePhysics::ApplyAngularFriction(PhysBody* pbody, float static_friction, float dynamic_friction, float radius)
{
	b2Body* b = pbody->body;

	float angular_velocity = b->GetAngularVelocity();

	// At rest (static): nothing makes it spin
	if (fabsf(angular_velocity) < 0.02f)
	{
		b->SetAngularVelocity(0.0f);
		return;
	}

	// Torque of the friction of a disc: (2/3) * mu * N * R. The friction opposes the spin
	float normal = b->GetMass() * GRAVITY_Z;
	float torque = (2.0f / 3.0f) * dynamic_friction * normal * radius;
	float sign = (angular_velocity > 0.0f) ? 1.0f : -1.0f;

	// It never makes the body spin the other way
	float next_angular_velocity = angular_velocity - sign * (PHYSICS_DT / b->GetInertia()) * torque;
	if (next_angular_velocity * angular_velocity <= 0.0f)
	{
		b->SetAngularVelocity(0.0f);
		return;
	}

	b->ApplyTorque(-sign * torque, true);
}

// Debug ----------------------------------------------------------------------------------------

bool ModulePhysics::IsDebug() const
{
	return debug;
}

void ModulePhysics::DrawDebug()
{
	if (!debug)
		return;

	for (b2Body* b = world->GetBodyList(); b; b = b->GetNext())
	{
		Color color = (b->GetType() == b2_dynamicBody) ? RED : Color{ 255, 120, 120, 255 };

		for (b2Fixture* f = b->GetFixtureList(); f; f = f->GetNext())
		{
			switch (f->GetType())
			{
				case b2Shape::e_circle:
				{
					b2CircleShape* shape = (b2CircleShape*)f->GetShape();
					b2Vec2 pos = b->GetWorldPoint(shape->m_p);
					DrawCircleLines(METERS_TO_PIXELS(pos.x), METERS_TO_PIXELS(pos.y), (float)METERS_TO_PIXELS(shape->m_radius), color);

					b2Vec2 edge = b->GetWorldPoint(shape->m_p + b2Vec2(shape->m_radius, 0.0f));
					DrawLine(METERS_TO_PIXELS(pos.x), METERS_TO_PIXELS(pos.y), METERS_TO_PIXELS(edge.x), METERS_TO_PIXELS(edge.y), WHITE);
				}
				break;

				case b2Shape::e_polygon:
				{
					b2PolygonShape* shape = (b2PolygonShape*)f->GetShape();
					for (int32 i = 0; i < shape->m_count; ++i)
					{
						b2Vec2 a = b->GetWorldPoint(shape->m_vertices[i]);
						b2Vec2 c = b->GetWorldPoint(shape->m_vertices[(i + 1) % shape->m_count]);
						DrawLine(METERS_TO_PIXELS(a.x), METERS_TO_PIXELS(a.y), METERS_TO_PIXELS(c.x), METERS_TO_PIXELS(c.y), color);
					}
				}
				break;

				case b2Shape::e_chain:
				{
					b2ChainShape* shape = (b2ChainShape*)f->GetShape();
					for (int32 i = 0; i < shape->m_count; ++i)
					{
						b2Vec2 a = b->GetWorldPoint(shape->m_vertices[i]);
						b2Vec2 c = b->GetWorldPoint(shape->m_vertices[(i + 1) % shape->m_count]);
						DrawLine(METERS_TO_PIXELS(a.x), METERS_TO_PIXELS(a.y), METERS_TO_PIXELS(c.x), METERS_TO_PIXELS(c.y), GREEN);
					}
				}
				break;

				default:
				break;
			}
		}
	}

	if (mouse_joint != NULL)
	{
		b2Vec2 a = mouse_joint->GetAnchorA();
		b2Vec2 b = mouse_joint->GetAnchorB();
		DrawLine(METERS_TO_PIXELS(a.x), METERS_TO_PIXELS(a.y), METERS_TO_PIXELS(b.x), METERS_TO_PIXELS(b.y), RED);
	}
}

// Contacts -------------------------------------------------------------------------------------

void ModulePhysics::BeginContact(b2Contact* contact)
{
	PhysBody* a = (PhysBody*)contact->GetFixtureA()->GetBody()->GetUserData().pointer;
	PhysBody* b = (PhysBody*)contact->GetFixtureB()->GetBody()->GetUserData().pointer;

	if (a != NULL && a->listener != NULL)
		a->listener->OnCollision(a, b);

	if (b != NULL && b->listener != NULL)
		b->listener->OnCollision(b, a);
}

// PhysBody ------------------------------------
void PhysBody::GetPosition(int& x, int& y) const
{
	b2Vec2 pos = body->GetPosition();
	x = METERS_TO_PIXELS(pos.x);
	y = METERS_TO_PIXELS(pos.y);
}

float PhysBody::GetRotation() const
{
	return body->GetAngle();
}

bool PhysBody::Contains(int x, int y) const
{
	b2Vec2 p(PIXEL_TO_METERS(x), PIXEL_TO_METERS(y));

	for (b2Fixture* fixture = body->GetFixtureList(); fixture; fixture = fixture->GetNext())
		if (fixture->GetShape()->TestPoint(body->GetTransform(), p))
			return true;

	return false;
}

int PhysBody::RayCast(int x1, int y1, int x2, int y2, float& normal_x, float& normal_y) const
{
	int ret = -1;

	b2RayCastInput input;
	input.p1.Set(PIXEL_TO_METERS(x1), PIXEL_TO_METERS(y1));
	input.p2.Set(PIXEL_TO_METERS(x2), PIXEL_TO_METERS(y2));
	input.maxFraction = 1.0f;

	float closest = 1.0f;

	for (b2Fixture* fixture = body->GetFixtureList(); fixture; fixture = fixture->GetNext())
	{
		for (int32 i = 0; i < fixture->GetShape()->GetChildCount(); ++i) // a chain has one child per edge
		{
			b2RayCastOutput output;
			if (fixture->GetShape()->RayCast(&output, input, body->GetTransform(), i) && output.fraction <= closest)
			{
				closest = output.fraction;

				float dx = (float)(x2 - x1);
				float dy = (float)(y2 - y1);
				ret = (int)(output.fraction * sqrtf(dx * dx + dy * dy));

				normal_x = output.normal.x;
				normal_y = output.normal.y;
			}
		}
	}

	return ret;
}
