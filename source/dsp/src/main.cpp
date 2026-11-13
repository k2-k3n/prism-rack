// MIT. A mono cell: transparent, soft drive/tone, or PSRAM echo.
#include <resona/math.h>
#include <resona/module.h>
#include <resona/parameters.h>
namespace
{
constexpr unsigned N = 32768;
const resona_buffer_request requests[] = {{1, RESONA_MEMORY_PSRAM, N * sizeof(float), 128}};
const resona_memory_profile memory = {RESONA_MEMORY_VERSION, sizeof(memory), 1, requests};
const resona_param params[] = {{"MODE", 0, 2, 1, ""},          {"DRIVE", 0, 1, .25f, ""},
                               {"TONE", 0, 1, .6f, ""},        {"TIME", 40, 500, 180, "ms"},
                               {"FEEDBACK", 0, .7f, .25f, ""}, {"WET", 0, 1, .45f, ""},
                               {"LEVEL", 0, 1, 1, ""}};
struct State
{
    float *delay;
    uint32_t write;
    float lp, v[7];
    resona_parameter_inbox inbox;
};
void init(void *p, const resona_host *h)
{
    State &s = *(State *)p;
    s.write = 0;
    s.lp = 0;
    s.inbox = {};
    const resona_buffer *b = resona_buffer_find(h->memory, 1);
    s.delay = b && b->bytes >= N * sizeof(float) ? (float *)b->data : nullptr;
    for (unsigned i = 0; i < 7; i++)
        s.v[i] = params[i].def;
    if (h->log)
        h->log("Prism Cell: user DSP initialized");
}
void reset(void *p)
{
    State &s = *(State *)p;
    s.write = 0;
    s.lp = 0;
    if (s.delay)
        for (unsigned i = 0; i < N; i++)
            s.delay[i] = 0;
}
void set(void *p, uint32_t i, float x)
{
    if (i < 7)
        resona_parameter_publish(&((State *)p)->inbox, i, x);
}
float get(const void *p, uint32_t i)
{
    return i < 7 ? ((const State *)p)->v[i] : 0;
}
void process(void *p, const float *const *in, float *const *out, uint32_t frames)
{
    State &s = *(State *)p;
    float next[12];
    uint32_t mask = resona_parameter_consume(&s.inbox, next);
    for (unsigned j = 0; j < 7; j++)
        if (mask & (1u << j))
            s.v[j] = next[j];
    unsigned mode = (unsigned)(s.v[0] + .5f), distance = (unsigned)(s.v[3] * 48);
    if (distance >= N)
        distance = N - 1;
    float drive = 1 + 12 * s.v[1], alpha = .025f + .42f * s.v[2];
    for (unsigned i = 0; i < frames; i++)
    {
        float dry = in[0][i], y = dry;
        if (mode == 1)
        {
            float z = dry * drive;
            z = z / (1 + resona_fabsf(z));
            s.lp += alpha * (z - s.lp);
            y = s.lp;
        }
        else if (mode == 2 && s.delay)
        {
            float wet = s.delay[(s.write + N - distance) & (N - 1)];
            s.delay[s.write] = dry + wet * s.v[4];
            s.write = (s.write + 1) & (N - 1);
            y = dry * (1 - s.v[5]) + wet * s.v[5];
        }
        out[0][i] = y * s.v[6];
    }
}
const resona_module module = {RESONA_MODULE_ABI,
                              sizeof(resona_module),
                              "org.resona.demo.prism-cell",
                              "Prism Cell",
                              "0.1.0",
                              RESONA_LAYOUT_MONO,
                              RESONA_RATE_48000,
                              sizeof(State),
                              7,
                              params,
                              nullptr,
                              init,
                              reset,
                              set,
                              process,
                              nullptr,
                              nullptr,
                              get,
                              nullptr,
                              &memory};
} // namespace
RESONA_EXPORT const resona_module *resona_module_get()
{
    return &module;
}
