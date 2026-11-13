// MIT. Separate independent trim instances make graph growth audible/measurable.
#include <resona/module.h>
struct State
{
    float gain;
};
static void init(void *p, const resona_host *h)
{
    (void)h;
    ((State *)p)->gain = 1;
}
static void reset(void *)
{
}
static void set(void *p, uint32_t i, float v)
{
    if (i == 0)
        ((State *)p)->gain = v;
}
static float get(const void *p, uint32_t i)
{
    return i == 0 ? ((const State *)p)->gain : 0;
}
static void process(void *p, const float *const *in, float *const *out, uint32_t n)
{
    float g = ((State *)p)->gain;
    for (uint32_t i = 0; i < n; i++)
        out[0][i] = in[0][i] * g;
}
static const resona_param params[] = {{"GAIN", 0, 1, 1, ""}};
static const resona_module module = {RESONA_MODULE_ABI,
                                     sizeof(resona_module),
                                     "org.resona.demo.prism-trim",
                                     "Prism Trim",
                                     "0.1.0",
                                     RESONA_LAYOUT_MONO,
                                     RESONA_RATE_48000,
                                     sizeof(State),
                                     1,
                                     params,
                                     nullptr,
                                     init,
                                     reset,
                                     set,
                                     process,
                                     nullptr,
                                     nullptr,
                                     get};
RESONA_EXPORT const resona_module *resona_module_get()
{
    return &module;
}
