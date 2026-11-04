/* SPDX-License-Identifier: GPL-3.0-only */
/* Single producer (decoder task), single consumer (audio ISR), signed Q15.
 * Resampling and ring-fill clock correction run only in the producer. */
#define A2DP_RING_FRAMES 4096u
#define A2DP_PREBUFFER 2048u
#define A2DP_PUBLISH() __asm__ volatile("" ::: "memory")
extern int16_t bt_buffer_setting;
static uint32_t a2dp_target(void) { return A2DP_PREBUFFER + (uint32_t)bt_buffer_setting * 512u; }
static int16_t a2dp_pcm[A2DP_RING_FRAMES][2] A2DP_POOL;
static volatile uint32_t a2dp_head, a2dp_tail, a2dp_playing;
static volatile uint32_t a2dp_pcm_generation;
volatile uint32_t a2dp_underruns, a2dp_blocks;   /* (the audio ISR's: dropouts heard, blocks played; console memr) */
static uint32_t a2dp_phase, a2dp_have_prev;
static int32_t a2dp_prev[2], a2dp_correction;

static void a2dp_pcm_reset(void)
{
    local_irq_disable();
    a2dp_head = a2dp_tail = a2dp_playing = 0;
    a2dp_pcm_generation = a2dp_generation;
    local_irq_enable();
    a2dp_phase = a2dp_have_prev = 0;
    a2dp_correction = 0;
}

void a2dp_mix(int32_t *left, int32_t *right, uint32_t frames, int32_t gain_q7)
{
    uint32_t t = a2dp_tail, h = a2dp_head, i;
    A2DP_PUBLISH();
    if (!a2dp_active || a2dp_pcm_generation != a2dp_generation)
        return;
    if (!a2dp_playing) {
        if (h - t < a2dp_target())
            return;
        a2dp_playing = 1;
    }
    a2dp_blocks++;
    for (i = 0; i < frames && t != h; ++i, ++t) {
        left[i] += (a2dp_pcm[t & (A2DP_RING_FRAMES - 1u)][0] * gain_q7) >> 7;
        right[i] += (a2dp_pcm[t & (A2DP_RING_FRAMES - 1u)][1] * gain_q7) >> 7;
    }
    A2DP_PUBLISH();
    a2dp_tail = t;
    if (i < frames) {
        a2dp_playing = 0; /* Underrun: leave the musical mix untouched. */
        a2dp_underruns++;
    }
}

static void a2dp_pcm_write(const int16_t *pcm, uint32_t frames, uint32_t channels,
                           uint32_t rate, uint32_t generation)
{
    uint32_t i, h = a2dp_head;
    int32_t fill = (int32_t)(h - a2dp_tail), step;
    /* A dead band avoids following packet jitter. Limit correction to 100 Hz. */
    if (a2dp_playing) {
        if (fill < (int32_t)a2dp_target() - 512 && a2dp_correction > -100)
            --a2dp_correction;
        else if (fill > (int32_t)a2dp_target() + 512 && a2dp_correction < 100)
            ++a2dp_correction;
    }
    step = (int32_t)rate + a2dp_correction;
    for (i = 0; i < frames; ++i) {
        int32_t l = pcm[i * channels], r = pcm[i * channels + channels - 1u];
        if (a2dp_have_prev) {
            while (a2dp_phase < 44100u) {
                int32_t fraction = (int32_t)((a2dp_phase * 48694u) >> 16);   /* phase * 32768 / 44100, no divide
                                                                               * (phase < 44100 + 100: fits) */
                while (h - a2dp_tail == A2DP_RING_FRAMES) {
                    if (generation != a2dp_generation || !a2dp_active)
                        return;
                    os_time_dly(1);
                }
                if (generation != a2dp_generation || !a2dp_active)
                    return;
                /* Exact 147/160 at 48 kHz with zero drift correction. */
                a2dp_pcm[h & (A2DP_RING_FRAMES - 1u)][0] =
                    (int16_t)(a2dp_prev[0] + (((l - a2dp_prev[0]) * fraction) >> 15));
                a2dp_pcm[h & (A2DP_RING_FRAMES - 1u)][1] =
                    (int16_t)(a2dp_prev[1] + (((r - a2dp_prev[1]) * fraction) >> 15));
                ++h;
                A2DP_PUBLISH();
                a2dp_head = h;
                a2dp_phase += (uint32_t)step;
            }
            a2dp_phase -= 44100u;
        }
        a2dp_have_prev = 1;
        a2dp_prev[0] = l;
        a2dp_prev[1] = r;
    }
}
