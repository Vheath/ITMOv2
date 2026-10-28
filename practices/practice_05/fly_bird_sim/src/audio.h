#ifndef AUDIO_H
#define AUDIO_H

#include <SDL3/SDL.h>
#include <stdbool.h>

#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_CHANNELS 2

struct audio_engine {
	SDL_AudioStream *stream;
	SDL_Mutex *lock;

	/* Wind rush synth state */
	float wind_intensity;
	float wind_target_intensity;
	float noise_lpf1;
	float noise_lpf2;
	uint32_t rng_state;

	/* Wing flap whoosh thud */
	float flap_env;
	float flap_freq;

	/* Ring chime chords */
	float chime_timer;
	float chime_freq[3];
	float chime_phase[3];
	float chime_gain;

	/* Crash impact thud */
	float crash_env;
	float crash_lpf;
};

int audio_engine_init(struct audio_engine *ae);
void audio_engine_destroy(struct audio_engine *ae);

void audio_engine_set_speed(struct audio_engine *ae, float speed,
			    bool is_boosting);
void audio_engine_trigger_flap(struct audio_engine *ae, float strength);
void audio_engine_trigger_ring(struct audio_engine *ae, int combo);
void audio_engine_trigger_crash(struct audio_engine *ae);

#endif /* AUDIO_H */
