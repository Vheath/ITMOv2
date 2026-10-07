#include "audio.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/*
 * Quick uniform pseudo-random generator for procedural white noise
 */
static inline float audio_white_noise(uint32_t *state)
{
	*state = *state * 1664525u + 1013904223u;
	return ((float)(*state & 0x00ffffff) / (float)0x007fffff) - 1.0f;
}

/*
 * Dynamic audio synthesis stream callback
 */
static void SDLCALL audio_stream_callback(void *userdata,
					  SDL_AudioStream *stream,
					  int additional_amount,
					  int total_amount)
{
	struct audio_engine *ae = (struct audio_engine *)userdata;
	(void)total_amount;

	if (additional_amount <= 0)
		return;

	int samples_needed = additional_amount / (int)sizeof(float);
	int frames_needed = samples_needed / AUDIO_CHANNELS;
	if (frames_needed <= 0)
		return;

	float *buf = malloc(sizeof(float) * samples_needed);
	if (!buf)
		return;

	SDL_LockMutex(ae->lock);

	float dt = 1.0f / (float)AUDIO_SAMPLE_RATE;

	for (int i = 0; i < frames_needed; i++) {
		/* Smooth wind intensity tracking */
		ae->wind_intensity +=
			(ae->wind_target_intensity - ae->wind_intensity) *
			(12.0f * dt);

		/* 1. Procedural Wind Rush (dual low-pass filtered noise) */
		float raw_noise = audio_white_noise(&ae->rng_state);

		/* Filter cutoff tracks airspeed dynamically */
		float cutoff = 0.04f + ae->wind_intensity * 0.22f;
		ae->noise_lpf1 += (raw_noise - ae->noise_lpf1) * cutoff;
		ae->noise_lpf2 += (ae->noise_lpf1 - ae->noise_lpf2) * cutoff;

		float wind_sample =
			ae->noise_lpf2 * (0.05f + ae->wind_intensity * 0.35f);

		/* 2. Wing Flap Whoosh Thud */
		float flap_sample = 0.0f;
		if (ae->flap_env > 0.001f) {
			ae->flap_env -= dt * 4.8f;
			if (ae->flap_env < 0.0f)
				ae->flap_env = 0.0f;
			float flap_noise = audio_white_noise(&ae->rng_state);
			flap_sample = flap_noise * ae->flap_env * 0.18f;
		}

		/* 3. Ring Collect Harmonic Pentatonic Chimes */
		float chime_sample = 0.0f;
		if (ae->chime_timer > 0.0f) {
			ae->chime_timer -= dt * 2.2f;
			float env = ae->chime_timer;
			if (env < 0.0f)
				env = 0.0f;

			for (int c = 0; c < 3; c++) {
				ae->chime_phase[c] +=
					2.0f * M_PI * ae->chime_freq[c] * dt;
				if (ae->chime_phase[c] > 2.0f * M_PI)
					ae->chime_phase[c] -= 2.0f * M_PI;
				chime_sample += sinf(ae->chime_phase[c]);
			}
			chime_sample = chime_sample * (env * env) *
				       ae->chime_gain * 0.12f;
		}

		/* 4. Crash Thud Envelope */
		float crash_sample = 0.0f;
		if (ae->crash_env > 0.001f) {
			ae->crash_env -= dt * 1.8f;
			if (ae->crash_env < 0.0f)
				ae->crash_env = 0.0f;
			float cn = audio_white_noise(&ae->rng_state);
			ae->crash_lpf += (cn - ae->crash_lpf) * 0.08f;
			crash_sample = ae->crash_lpf * ae->crash_env * 0.45f;
		}

		/* Mix & soft saturate */
		float mono =
			wind_sample + flap_sample + chime_sample + crash_sample;

		/* Soft clipping tanh approximation */
		if (mono > 1.0f)
			mono = 1.0f;
		else if (mono < -1.0f)
			mono = -1.0f;

		buf[i * 2 + 0] = mono;
		buf[i * 2 + 1] = mono;
	}

	SDL_UnlockMutex(ae->lock);

	SDL_PutAudioStreamData(stream, buf, samples_needed * sizeof(float));
	free(buf);
}

int audio_engine_init(struct audio_engine *ae)
{
	memset(ae, 0, sizeof(*ae));
	ae->rng_state = 0x1337c0de;

	ae->lock = SDL_CreateMutex();
	if (!ae->lock)
		return -1;

	SDL_AudioSpec spec;
	spec.format = SDL_AUDIO_F32;
	spec.channels = AUDIO_CHANNELS;
	spec.freq = AUDIO_SAMPLE_RATE;

	ae->stream =
		SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
					  &spec, audio_stream_callback, ae);
	if (!ae->stream) {
		SDL_DestroyMutex(ae->lock);
		return -1;
	}

	SDL_ResumeAudioStreamDevice(ae->stream);
	return 0;
}

void audio_engine_destroy(struct audio_engine *ae)
{
	if (ae->stream) {
		SDL_DestroyAudioStream(ae->stream);
		ae->stream = NULL;
	}
	if (ae->lock) {
		SDL_DestroyMutex(ae->lock);
		ae->lock = NULL;
	}
}

void audio_engine_set_speed(struct audio_engine *ae, float speed,
			    bool is_boosting)
{
	SDL_LockMutex(ae->lock);
	float norm = (speed - 12.0f) / (36.0f - 12.0f);
	if (norm < 0.0f)
		norm = 0.0f;
	if (norm > 1.0f)
		norm = 1.0f;

	if (is_boosting)
		norm = 1.25f;

	ae->wind_target_intensity = norm;
	SDL_UnlockMutex(ae->lock);
}

void audio_engine_trigger_flap(struct audio_engine *ae, float strength)
{
	SDL_LockMutex(ae->lock);
	ae->flap_env = strength;
	SDL_UnlockMutex(ae->lock);
}

void audio_engine_trigger_ring(struct audio_engine *ae, int combo)
{
	/* Pentatonic scale frequency table (Hz) */
	static const float notes[] = {
		523.25f,  /* C5 */
		587.33f,  /* D5 */
		659.25f,  /* E5 */
		783.99f,  /* G5 */
		880.00f,  /* A5 */
		1046.50f, /* C6 */
		1174.66f, /* D6 */
		1318.51f, /* E6 */
	};

	int base_idx = (combo - 1) % 6;
	if (base_idx < 0)
		base_idx = 0;

	SDL_LockMutex(ae->lock);
	ae->chime_timer = 1.0f;
	ae->chime_freq[0] = notes[base_idx];
	ae->chime_freq[1] = notes[base_idx + 1];
	ae->chime_freq[2] = notes[base_idx + 2];
	ae->chime_gain = 0.8f + (float)combo * 0.15f;
	SDL_UnlockMutex(ae->lock);
}

void audio_engine_trigger_crash(struct audio_engine *ae)
{
	SDL_LockMutex(ae->lock);
	ae->crash_env = 1.0f;
	ae->wind_target_intensity = 0.0f;
	ae->wind_intensity = 0.0f;
	SDL_UnlockMutex(ae->lock);
}
