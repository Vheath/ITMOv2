#include "village.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "terrain.h"

/* 96 m cluster lattice: 8x the flora cell, snapped like scenery */
#define VILLAGE_CELL_SIZE 96.0f
#define VILLAGE_CELL_RADIUS 3

/* ~12% of cells seed a cluster (10-15% band, research.md D1) */
#define VILLAGE_CLUSTER_THRESHOLD 12

/*
 * Flat-ground scarcity on this terrain: at the mandated 6x5 m
 * footprint and 1.5 m corner-spread bound, a single hash-derived
 * spot passes D2 only ~6% of the time, which cannot reach the
 * SC-001 density. The hub and each building therefore try a short
 * ladder of hash-derived candidate positions (all inside their
 * allowed near-cell-centre / +/-20 m discs); the first candidate
 * that passes validation is used. The ladder is a pure function of
 * the cell hash, so determinism (FR-007) is unaffected.
 */
#define VILLAGE_HUB_CANDIDATES 8
#define VILLAGE_BLD_CANDIDATES 24

/* Placement volume (research.md D2) */
#define VILLAGE_MIN_ALT 5.0f
#define VILLAGE_MAX_ALT 31.0f
#define VILLAGE_CORRIDOR_MARGIN 40.0f
#define VILLAGE_MAX_SPREAD 1.5f
#define VILLAGE_BASE_SINK 0.2f

/* Building geometry (research.md D4) */
#define VILLAGE_STORY_HEIGHT 3.2f
#define VILLAGE_ROOF_RISE 1.5f
#define VILLAGE_MAX_BUILDINGS 4

/* Field paths (research.md D5) */
#define VILLAGE_PATH_WIDTH 2.2f
#define VILLAGE_PATH_STEP 8.0f
#define VILLAGE_PATH_LIFT 0.08f

/*
 * All faces are wound with their cross-product normal pointing INTO
 * the solid: this pipeline culls triangles whose winding normal
 * faces the camera, so inward winding is what makes each face
 * visible from its true outside. Flat-shading then lights the face
 * as ambient-only, exactly like the terrain grid.
 */

struct village_bld {
	float x; /* world centre */
	float z;
	float hx;     /* footprint half-extent along local x */
	float hz;     /* footprint half-extent along local z */
	float yaw;    /* local +z (door face) looks at the hub */
	int stories;  /* 1 or 2 */
	int palette;  /* wood tone pair + roof colour */
	float base_y; /* min corner ground height - sink */
};

struct village_cluster {
	float hub_x; /* path convergence point */
	float hub_z;
	int nblds; /* 0 = empty cell; survives only >= 2 */
	struct village_bld blds[VILLAGE_MAX_BUILDINGS];
};

/* Local-to-world frame for one building */
struct village_frame {
	float x;
	float z;
	float y0; /* base height */
	float c;  /* cos(yaw) */
	float s;  /* sin(yaw) */
};

static const struct color_rgb village_wall[3] = {
	{0.52f, 0.38f, 0.24f, 1.0f}, /* weathered barn planks */
	{0.60f, 0.45f, 0.28f, 1.0f}, /* warm pine */
	{0.44f, 0.32f, 0.20f, 1.0f}, /* dark timber */
};

static const struct color_rgb village_gable[3] = {
	{0.62f, 0.47f, 0.30f, 1.0f},
	{0.70f, 0.55f, 0.35f, 1.0f},
	{0.54f, 0.41f, 0.26f, 1.0f},
};

static const struct color_rgb village_roof[3] = {
	{0.42f, 0.20f, 0.16f, 1.0f}, /* slate red */
	{0.34f, 0.30f, 0.32f, 1.0f}, /* grey slate */
	{0.50f, 0.25f, 0.18f, 1.0f}, /* brick red */
};

static const struct color_rgb village_door_col = {0.16f, 0.11f, 0.07f, 1.0f};
static const struct color_rgb village_window_col = {0.90f, 0.86f, 0.66f, 1.0f};
static const struct color_rgb village_path_col = {0.62f, 0.52f, 0.38f, 1.0f};

/*
 * Integer hash of (ix, iz) in the same family as terrain's
 * hash_coord(), with the multiplies done in unsigned arithmetic.
 */
static inline uint32_t village_hash(int x, int z)
{
	uint32_t h = (uint32_t)x * 374761393u + (uint32_t)z * 668265263u;

	h = (h ^ (h >> 13)) * 1274126177u;
	return h ^ (h >> 16);
}

static float village_corridor_dist(float x, float z)
{
	return fabsf(x - sinf(z * 0.01f) * 35.0f);
}

/* Valley band + canyon/river corridor margin check (research.md D2);
 * the corridor test is cheap (one sinf), so it runs first. */
static bool village_ground_ok(float x, float z)
{
	if (village_corridor_dist(x, z) < VILLAGE_CORRIDOR_MARGIN)
		return false;

	float h = terrain_get_height(x, z);

	return h >= VILLAGE_MIN_ALT && h <= VILLAGE_MAX_ALT;
}

static struct vec3 village_pt(const struct village_frame *f, float lx, float ly,
			      float lz)
{
	return vec3_create(f->x + lx * f->c - lz * f->s, f->y0 + ly,
			   f->z + lx * f->s + lz * f->c);
}

static void village_tri(struct tri3d *tris, int *count, struct vec3 a,
			struct vec3 b, struct vec3 c, struct color_rgb col)
{
	tris[*count].v[0] = a;
	tris[*count].v[1] = b;
	tris[*count].v[2] = c;
	tris[*count].color = col;
	(*count)++;
}

static void village_quad(struct tri3d *tris, int *count, struct vec3 q0,
			 struct vec3 q1, struct vec3 q2, struct vec3 q3,
			 struct color_rgb col)
{
	village_tri(tris, count, q0, q1, q2, col);
	village_tri(tris, count, q0, q2, q3, col);
}

/*
 * Derives one building from hash bits around the cluster hub and
 * validates its whole footprint (research.md D2): all four corners
 * in the vegetated valley band, outside the corridor margin, and a
 * corner-to-corner height spread <= 1.5 m. Failing buildings are
 * dropped individually.
 */
static bool village_derive_building(uint32_t hb, float hub_x, float hub_z,
				    float ang, struct village_bld *b)
{
	static const float corner_x[4] = {-1.0f, 1.0f, 1.0f, -1.0f};
	static const float corner_z[4] = {-1.0f, -1.0f, 1.0f, 1.0f};

	/* Hub-relative offset within +/-20 m of the hub */
	float radius = 8.0f + ((float)((hb >> 8) % 100) / 100.0f) * 12.0f;

	b->x = hub_x + cosf(ang) * radius;
	b->z = hub_z + sinf(ang) * radius;
	b->hx = 2.8f + 0.1f * (float)((hb >> 16) % 5);
	b->hz = 2.3f + 0.1f * (float)((hb >> 19) % 5);
	b->stories = 1 + (int)((hb >> 21) % 2);
	b->palette = (int)((hb >> 23) % 3);

	/* Orient the door face (+z local) toward the path hub */
	b->yaw = atan2f(b->x - hub_x, hub_z - b->z);

	float c = cosf(b->yaw);
	float s = sinf(b->yaw);
	float ymin = 1e9f;
	float ymax = -1e9f;

	for (int i = 0; i < 4; i++) {
		float lx = b->hx * corner_x[i];
		float lz = b->hz * corner_z[i];
		float wx = b->x + lx * c - lz * s;
		float wz = b->z + lx * s + lz * c;

		if (village_corridor_dist(wx, wz) < VILLAGE_CORRIDOR_MARGIN)
			return false;

		float h = terrain_get_height(wx, wz);

		if (h < VILLAGE_MIN_ALT || h > VILLAGE_MAX_ALT)
			return false;
		if (h < ymin)
			ymin = h;
		if (h > ymax)
			ymax = h;
	}

	if (ymax - ymin > VILLAGE_MAX_SPREAD)
		return false;

	/* Flush seating: anchor at the lowest corner (research.md D3) */
	b->base_y = ymin - VILLAGE_BASE_SINK;
	return true;
}

/* Door point: centre of the door face, pushed off the wall */
static void village_door_point(const struct village_bld *b, float *dx,
			       float *dz)
{
	float off = b->hz + 0.6f;

	*dx = b->x - sinf(b->yaw) * off;
	*dz = b->z + cosf(b->yaw) * off;
}

/*
 * Validates the door-to-hub path (research.md D5): straight line,
 * tessellated every ~8 m, no vertex inside the corridor margin
 * (FR-006). Uses the same tessellation the emitter applies, so
 * what is checked here is exactly what gets drawn.
 */
static bool village_path_ok(const struct village_bld *b, float hub_x,
			    float hub_z)
{
	float dx, dz;

	village_door_point(b, &dx, &dz);
	float vx = hub_x - dx;
	float vz = hub_z - dz;
	float dist = sqrtf(vx * vx + vz * vz);
	int segs = (int)(dist / VILLAGE_PATH_STEP) + 1;

	for (int i = 0; i <= segs; i++) {
		float t = (float)i / (float)segs;

		if (village_corridor_dist(dx + vx * t, dz + vz * t) <
		    VILLAGE_CORRIDOR_MARGIN)
			return false;
	}
	return true;
}

/*
 * Derives the cluster of one lattice cell: threshold check, hub
 * seed validation (candidate ladder), then 2-4 hash-derived
 * buildings (each with its own candidate ladder). A cell stays
 * wilderness unless at least 2 buildings survive (data-model.md).
 */
static void village_derive_cluster(int ix, int iz, struct village_cluster *cl)
{
	cl->nblds = 0;

	uint32_t h = village_hash(ix, iz);

	if ((h % 100) >= VILLAGE_CLUSTER_THRESHOLD)
		return;

	/* Hub: first hash-derived ground point near the cell centre
	 * that satisfies the placement volume; when no candidate
	 * passes, the seed failed and the cluster is dropped (D2).
	 */
	float cell_cx =
		(float)ix * VILLAGE_CELL_SIZE + VILLAGE_CELL_SIZE * 0.5f;
	float cell_cz =
		(float)iz * VILLAGE_CELL_SIZE + VILLAGE_CELL_SIZE * 0.5f;
	bool hub_ok = false;

	for (int k = 0; k < VILLAGE_HUB_CANDIDATES && !hub_ok; k++) {
		uint32_t hk = village_hash(ix + k * 1013, iz - k * 331);

		cl->hub_x = cell_cx + (((float)((hk >> 4) % 100) / 100.0f) -
				       0.5f) * 28.0f;
		cl->hub_z = cell_cz + (((float)((hk >> 14) % 100) / 100.0f) -
				       0.5f) * 28.0f;
		hub_ok = village_ground_ok(cl->hub_x, cl->hub_z);
	}
	if (!hub_ok)
		return;

	int want = 2 + (int)((h >> 2) % 3);

	for (int b = 0; b < want; b++) {
		struct village_bld bl;
		bool ok = false;

		/* Candidate ladder: hash-derived spots in the +/-20 m
		 * disc around the hub; the first that passes D2 wins,
		 * otherwise the building is dropped individually.
		 */
		for (int k = 0; k < VILLAGE_BLD_CANDIDATES && !ok; k++) {
			uint32_t hk = village_hash(ix * 4 + b, iz + k * 1024);
			float ang = (float)(hk % 360) * ((float)M_PI / 180.0f);

			ok = village_derive_building(hk, cl->hub_x, cl->hub_z,
						     ang, &bl);
			/* A building survives only with a legally
			 * placeable path: every surviving building is
			 * connected (FR-004) and no geometry sits in
			 * the corridor (FR-006).
			 */
			if (ok)
				ok = village_path_ok(&bl, cl->hub_x, cl->hub_z);
		}
		if (ok)
			cl->blds[cl->nblds++] = bl;
	}
}

/* Four wall faces: 2 triangles each; lighter gable-end short walls */
static void village_emit_walls(const struct village_frame *f,
			       const struct village_bld *b, struct tri3d *tris,
			       int *count)
{
	float wh = (float)b->stories * VILLAGE_STORY_HEIGHT;
	float hx = b->hx;
	float hz = b->hz;
	struct color_rgb wall = village_wall[b->palette];
	struct color_rgb gable = village_gable[b->palette];

	/* Long walls (door side + back): wood-plank browns */
	village_quad(tris, count, village_pt(f, hx, 0.0f, hz),
		     village_pt(f, -hx, 0.0f, hz), village_pt(f, -hx, wh, hz),
		     village_pt(f, hx, wh, hz), wall);
	village_quad(tris, count, village_pt(f, -hx, 0.0f, -hz),
		     village_pt(f, hx, 0.0f, -hz), village_pt(f, hx, wh, -hz),
		     village_pt(f, -hx, wh, -hz), wall);

	/* Short walls (gable ends): lighter wood */
	village_quad(tris, count, village_pt(f, hx, 0.0f, -hz),
		     village_pt(f, hx, 0.0f, hz), village_pt(f, hx, wh, hz),
		     village_pt(f, hx, wh, -hz), gable);
	village_quad(tris, count, village_pt(f, -hx, 0.0f, hz),
		     village_pt(f, -hx, 0.0f, -hz), village_pt(f, -hx, wh, -hz),
		     village_pt(f, -hx, wh, hz), gable);
}

/* Pitched roof: 2 rectangular slopes + 2 triangular gable ends */
static void village_emit_roof(const struct village_frame *f,
			      const struct village_bld *b, struct tri3d *tris,
			      int *count)
{
	float wh = (float)b->stories * VILLAGE_STORY_HEIGHT;
	float rg = wh + VILLAGE_ROOF_RISE;
	float hx = b->hx;
	float hz = b->hz;
	struct color_rgb roof = village_roof[b->palette];
	struct color_rgb gable = village_gable[b->palette];

	village_quad(tris, count, village_pt(f, -hx, wh, hz),
		     village_pt(f, -hx, rg, 0.0f), village_pt(f, hx, rg, 0.0f),
		     village_pt(f, hx, wh, hz), roof);
	village_quad(tris, count, village_pt(f, -hx, wh, -hz),
		     village_pt(f, hx, wh, -hz), village_pt(f, hx, rg, 0.0f),
		     village_pt(f, -hx, rg, 0.0f), roof);

	village_tri(tris, count, village_pt(f, hx, wh, -hz),
		    village_pt(f, hx, wh, hz), village_pt(f, hx, rg, 0.0f),
		    gable);
	village_tri(tris, count, village_pt(f, -hx, wh, hz),
		    village_pt(f, -hx, wh, -hz), village_pt(f, -hx, rg, 0.0f),
		    gable);
}

/* Door quad on one long face; windows only on two-storey buildings */
static void village_emit_decals(const struct village_frame *f,
				const struct village_bld *b, struct tri3d *tris,
				int *count)
{
	float zd = b->hz + 0.06f;

	village_quad(tris, count, village_pt(f, 0.55f, 0.1f, zd),
		     village_pt(f, -0.55f, 0.1f, zd),
		     village_pt(f, -0.55f, 2.1f, zd),
		     village_pt(f, 0.55f, 2.1f, zd), village_door_col);

	if (b->stories < 2)
		return;

	float y0 = VILLAGE_STORY_HEIGHT + 0.6f;
	float y1 = VILLAGE_STORY_HEIGHT + 2.1f;

	village_quad(tris, count, village_pt(f, 1.8f, y0, zd),
		     village_pt(f, 0.8f, y0, zd), village_pt(f, 0.8f, y1, zd),
		     village_pt(f, 1.8f, y1, zd), village_window_col);
	village_quad(tris, count, village_pt(f, -0.8f, y0, -zd),
		     village_pt(f, -1.8f, y0, -zd),
		     village_pt(f, -1.8f, y1, -zd),
		     village_pt(f, -0.8f, y1, -zd), village_window_col);
}

/*
 * Emits one whole building or nothing (FR-011): 16 triangles for a
 * one-storey house, 20 with the second-storey windows.
 */
static void village_emit_building(const struct village_bld *b,
				  struct tri3d *tris, int max_tris, int *count)
{
	int need = 16 + (b->stories == 2 ? 4 : 0);

	if (*count + need > max_tris)
		return;

	struct village_frame f = {b->x, b->z, b->base_y, cosf(b->yaw),
				  sinf(b->yaw)};

	village_emit_walls(&f, b, tris, count);
	village_emit_roof(&f, b, tris, count);
	village_emit_decals(&f, b, tris, count);
}

/*
 * Emits the dirt path from the building's door to the cluster hub
 * (research.md D5): 2.2 m wide flat strips, straight line,
 * tessellated every ~8 m, every vertex seated on the terrain plus
 * a 0.08 m anti-z-fighting lift. Two opaque triangles per segment,
 * emitted whole or not at all (FR-011).
 */
static void village_emit_path(const struct village_bld *b, float hub_x,
			      float hub_z, struct tri3d *tris, int max_tris,
			      int *count)
{
	float dx, dz;

	village_door_point(b, &dx, &dz);
	float vx = hub_x - dx;
	float vz = hub_z - dz;
	float dist = sqrtf(vx * vx + vz * vz);

	if (dist < 0.001f)
		return;

	int segs = (int)(dist / VILLAGE_PATH_STEP) + 1;
	float hw = VILLAGE_PATH_WIDTH * 0.5f;

	for (int i = 0; i < segs; i++) {
		if (*count + 2 > max_tris)
			return;

		float t0 = (float)i / (float)segs;
		float t1 = (float)(i + 1) / (float)segs;
		float ax = dx + vx * t0;
		float az = dz + vz * t0;
		float bx = dx + vx * t1;
		float bz = dz + vz * t1;

		/* Strip direction and its perpendicular */
		float ddx = bx - ax;
		float ddz = bz - az;
		float dl = sqrtf(ddx * ddx + ddz * ddz);

		if (dl < 0.0001f)
			continue;

		float px = (ddz / dl) * hw;
		float pz = -(ddx / dl) * hw;

		/* Every quad corner samples the terrain beneath it,
		 * lifted 0.08 m: the strip stays glued to rolling
		 * ground with no clipping or hovering (V3).
		 */
		float ay0 = terrain_get_height(ax - px, az - pz) +
			    VILLAGE_PATH_LIFT;
		float ay1 = terrain_get_height(ax + px, az + pz) +
			    VILLAGE_PATH_LIFT;
		float by1 = terrain_get_height(bx + px, bz + pz) +
			    VILLAGE_PATH_LIFT;
		float by0 = terrain_get_height(bx - px, bz - pz) +
			    VILLAGE_PATH_LIFT;

		/* Terrain-style downward winding: visible from above */
		village_quad(tris, count, vec3_create(ax - px, ay0, az - pz),
			     vec3_create(ax + px, ay1, az + pz),
			     vec3_create(bx + px, by1, bz + pz),
			     vec3_create(bx - px, by0, bz - pz),
			     village_path_col);
	}
}

int village_generate_mesh(struct vec3 center_pos, struct tri3d *tris,
			  int max_tris)
{
	int count = 0;
	float step = VILLAGE_CELL_SIZE;
	int radius = VILLAGE_CELL_RADIUS;

	float snapped_x = floorf(center_pos.x / step) * step;
	float snapped_z = floorf(center_pos.z / step) * step;

	for (int gz = -radius; gz <= radius; gz++) {
		for (int gx = -radius; gx <= radius; gx++) {
			int ix = (int)(snapped_x / step) + gx;
			int iz = (int)(snapped_z / step) + gz;
			struct village_cluster cl;

			village_derive_cluster(ix, iz, &cl);
			if (cl.nblds < 2)
				continue;

			for (int b = 0; b < cl.nblds; b++) {
				village_emit_building(&cl.blds[b], tris,
						      max_tris, &count);
				village_emit_path(&cl.blds[b], cl.hub_x,
						  cl.hub_z, tris, max_tris,
						  &count);
			}
		}
	}

	return count;
}
