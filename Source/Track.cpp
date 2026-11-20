#include "Track.h"

#include <math.h>

#define SEARCH_WINDOW 14

static float DistanceToSegment(float px, float py, Vector2 a, Vector2 b)
{
	float abx = b.x - a.x;
	float aby = b.y - a.y;
	float length_sq = abx * abx + aby * aby;

	float t = 0.0f;
	if (length_sq > 0.0f)
		t = ((px - a.x) * abx + (py - a.y) * aby) / length_sq;
	t = (t < 0.0f) ? 0.0f : (t > 1.0f) ? 1.0f : t;

	float dx = px - (a.x + abx * t);
	float dy = py - (a.y + aby * t);
	return sqrtf(dx * dx + dy * dy);
}

Track::Track()
{
	for (int i = 0; i < TRACK_POINTS; ++i)
		points[i] = Vector2{ (float)TRACK_CENTER[i * 2], (float)TRACK_CENTER[i * 2 + 1] };

	for (int i = 0; i < TRACK_POINTS; ++i)
	{
		Vector2 next = points[(i + 1) % TRACK_POINTS];
		Vector2 prev = points[(i + TRACK_POINTS - 1) % TRACK_POINTS];

		float dx = next.x - prev.x;
		float dy = next.y - prev.y;
		float length = sqrtf(dx * dx + dy * dy);

		tangents[i] = Vector2{ dx / length, dy / length };
		normals[i] = Vector2{ tangents[i].y, -tangents[i].x }; // visual left (Y grows downwards)
	}

	// name, static (grip), dynamic (sliding), rolling.  Asphalt and ice: tables of the Lesson 5
	surfaces[SURF_ASPHALT] = Surface{ "ASPHALT", 1.00f, 0.80f, 0.08f };
	surfaces[SURF_GRASS] = Surface{ "GRASS", 0.60f, 0.45f, 0.30f };
	surfaces[SURF_SAND] = Surface{ "SAND", 0.50f, 0.40f, 0.35f };
	surfaces[SURF_ICE] = Surface{ "ICE", 0.10f, 0.03f, 0.02f };
}

int Track::GetPointCount() const
{
	return TRACK_POINTS;
}

Vector2 Track::GetPoint(int index) const
{
	return points[((index % TRACK_POINTS) + TRACK_POINTS) % TRACK_POINTS];
}

Vector2 Track::GetTangent(int index) const
{
	return tangents[((index % TRACK_POINTS) + TRACK_POINTS) % TRACK_POINTS];
}

Vector2 Track::GetNormalLeft(int index) const
{
	return normals[((index % TRACK_POINTS) + TRACK_POINTS) % TRACK_POINTS];
}

int Track::FindNearest(float x, float y, int hint) const
{
	int first = 0;
	int count = TRACK_POINTS;

	if (hint >= 0)
	{
		first = hint - SEARCH_WINDOW;
		count = SEARCH_WINDOW * 2 + 1;
	}

	int best = 0;
	float best_distance = 1.0e20f;

	for (int k = 0; k < count; ++k)
	{
		int i = (((first + k) % TRACK_POINTS) + TRACK_POINTS) % TRACK_POINTS;
		float dx = x - points[i].x;
		float dy = y - points[i].y;
		float d = dx * dx + dy * dy;

		if (d < best_distance)
		{
			best_distance = d;
			best = i;
		}
	}

	return best;
}

float Track::DistanceToCenter(float x, float y, int nearest) const
{
	int prev = (nearest + TRACK_POINTS - 1) % TRACK_POINTS;
	int next = (nearest + 1) % TRACK_POINTS;

	float d1 = DistanceToSegment(x, y, points[prev], points[nearest]);
	float d2 = DistanceToSegment(x, y, points[nearest], points[next]);
	return (d1 < d2) ? d1 : d2;
}

SurfaceType Track::GetSurfaceType(float x, float y, int nearest) const
{
	// Phase 2: patches of sand and ice (TRACK_SURFACES)

	// Road (curbs included) or grass
	if (DistanceToCenter(x, y, nearest) <= TRACK_ROAD_HALF_WIDTH + 9.0f)
		return SURF_ASPHALT;

	return SURF_GRASS;
}

Surface& Track::GetSurface(SurfaceType type)
{
	return surfaces[type];
}

const Surface& Track::GetSurface(SurfaceType type) const
{
	return surfaces[type];
}
