// MIT. Prism Rack: a normal user App, public SDK only, no firmware UART.
//
// The Web, Mobile and MIDI controllers (controller/, CCs on channel 1, see README.md) reach every setting the
// knobs reach, plus the input, each effect's fixed values (drive tone and level, echo feedback, gate threshold,
// input trim), the four main processors' on/off, SAVE, and four hold pads played across the independent effects:
// FIRE (Cell A screams), DUB (Cell B feeds back), KILL (the trim cuts the input; echoes ring on) and DRY.
#include <resona/effects.hpp>
#include <resona/startup_helpers.h>
#include <resona/libc.h>
#include "gfx.hpp"
#ifdef RESONA_DEMO_BENCH   // the SDK LAB page and its probes (allocation exhaustion, timers, routing, lifecycle, reboot)
#include "storage_contract.h"
#include "sdk_probes.h"
#include "sdk_round2.h"
#include "event_contract.h"
#include "sdk_round3.h"
#include "format_eq_contract.h"
#include "sdk_round4.h"
#include "ui_fixed_probe.h"
#include "sdk_round5.h"
#include "sdk_round6.h"
#endif
#include "demo_capture.h"
#include "trip_guard.h"
#include "controller_link.h"
#include "generated/splash_data.h"
namespace
{
const resona_platform *P;
resona::Effects fx;
const char *scenes[] = {"GLASS", "EMBER", "HALO"};
const char *nodes[] = {"FACTORY GATE", "FACTORY COMP", "CELL A DRIVE", "CELL B ECHO", "INPUT TRIM", "EXTRA TRIM"};
#ifdef RESONA_DEMO_BENCH
constexpr unsigned kPages = 3;   // LIVE, RACK, SDK LAB
const char *labItems[] = {"INPUT GTR / USB",    "ADD TRIM INSTANCE", "REMOVE LAST TRIM",
                          "GROW UNTIL REFUSAL", "FREE EXTRA TRIMS",  "APP HEAP PROBE",
                          "TIMER PROBE",        "REJECT A CYCLE",    "STORAGE BOUNDS",
                          "SAVE APP SETTINGS",  "APPLY SPLASH CODE", "APPLY VARIANT",
                          "REBOOT VIA SDK"};
#else
constexpr unsigned kPages = 2;   // LIVE, RACK; the SDK LAB page is bench-only
#endif
struct Heap
{
    uint16_t pixels[320 * 172];
    uint8_t splash[RESONA_SPLASH_MAX_BYTES];
};
struct PrefsV1
{
    uint32_t magic, version, scene, mix, drive, delay, input, bypass, crc;
};
struct Prefs  // version 2: the controllers' settings follow the 0.1 fields
{
    uint32_t magic, version, scene, mix, drive, delay, input, bypass, tone, level, feedback, gateThr,
        trimGain, crc;
};
// The controllers' MIDI map: CC on channel 1 (controller/shared/profile.js says the same).
enum
{
    CC_SCENE = 20, CC_BLEND, CC_INPUT, CC_BYPASS, CC_DRIVE, CC_TONE, CC_LEVEL, CC_TIME, CC_FEEDBACK,
    CC_GATE, CC_FIRE = 30, CC_DUB, CC_KILL, CC_DRY, CC_SAVE, CC_TRIM = 40, CC_GATE_ON, CC_COMP_ON,
    CC_DRIVE_ON, CC_ECHO_ON, CC_CPU
};
struct State
{
    Heap *h;
    uint32_t gate, comp, a, b, trim, extra[16], count;
    unsigned page, selected, scene, mix, drive, delay, input, bypass, activeScene, activeMix,
        activeBypass;
    uint32_t start, lastDraw, lastStatus, drawMax, frames, resetToken, heapBlocks, heapBig,
        timerCount, initMs;
    int timerRc;
    int ready, rc, grow, preview;
    uint8_t inLevel, outLevel, cpu;
    unsigned uiIn, uiOut;
    resona_effects_status status;
    resona_program_resources chain;
    char notice[52];
    Prefs saved;
    int waiting;                // the OS bypassed one of our effects for its CPU budget: audio retries at retryAt
    uint32_t retryAt, tripped;  // tripped: the instance index (0 gate .. 4 trim, 5+ extra), or ~0u if unknown
    uint32_t faultScan;         // when our instances were last checked for an OS bypass
    trip_guard::Guard trip;
    unsigned tone, level, feedback, gateThr, trimGain;  // percent; feedback 0..70
    int gateOn, compOn, aOn, bOn, cpuRead;  // processors on (not saved) and CPU, for the controllers
    int fire, dub, kill, dry;               // pads held on a controller
    ctl::Link link;
};
State *S;
constexpr int kBinds = 21;
ctl::Bind kBind[kBinds];
int *I(unsigned &v) { return reinterpret_cast<int *>(&v); }
void bind()
{
    const ctl::Bind b[kBinds] = {
        {CC_SCENE, ctl::STEPS, 0, 2, I(S->scene)},       {CC_BLEND, ctl::RANGE, 0, 100, I(S->mix)},
        {CC_INPUT, ctl::STEPS, 0, 1, I(S->input)},       {CC_BYPASS, ctl::STEPS, 0, 1, I(S->bypass)},
        {CC_DRIVE, ctl::RANGE, 0, 100, I(S->drive)},     {CC_TONE, ctl::RANGE, 0, 100, I(S->tone)},
        {CC_LEVEL, ctl::RANGE, 0, 100, I(S->level)},     {CC_TIME, ctl::RANGE, 40, 500, I(S->delay)},
        {CC_FEEDBACK, ctl::RANGE, 0, 70, I(S->feedback)}, {CC_GATE, ctl::RANGE, 0, 100, I(S->gateThr)},
        {CC_TRIM, ctl::RANGE, 0, 100, I(S->trimGain)},   {CC_GATE_ON, ctl::STEPS, 0, 1, &S->gateOn},
        {CC_COMP_ON, ctl::STEPS, 0, 1, &S->compOn},      {CC_DRIVE_ON, ctl::STEPS, 0, 1, &S->aOn},
        {CC_ECHO_ON, ctl::STEPS, 0, 1, &S->bOn},         {CC_CPU, ctl::READ_RANGE, 0, 100, &S->cpuRead},
        {CC_FIRE, ctl::TRIGGER, 0, 1, nullptr},          {CC_DUB, ctl::TRIGGER, 0, 1, nullptr},
        {CC_KILL, ctl::TRIGGER, 0, 1, nullptr},          {CC_DRY, ctl::TRIGGER, 0, 1, nullptr},
        {CC_SAVE, ctl::TRIGGER, 0, 1, nullptr},
    };
    for (int i = 0; i < kBinds; i++)
        kBind[i] = b[i];
}
// The pads override while held: FIRE and DUB play GLASS as EMBER so their cells are heard; DRY bypasses.
unsigned sceneNow() { return S->scene == 0 && (S->fire || S->dub) ? 1 : S->scene; }
unsigned bypassNow() { return S->bypass || S->dry ? 1 : 0; }
int clamp(int v, int a, int b) { return v < a ? a : v > b ? b : v; }
void log(const char *name, int rc)
{
    char s[128];
    P->format(s, sizeof s, "PRISM %s rc=%d scene=%u mix=%u extra=%u instances=%u faults=%u", name,
              rc, S->scene, S->mix, S->count, S->status.instances, S->status.faults);
    P->log(s);
}
void note(const char *s, int rc)
{
    S->rc = rc;
    unsigned i = 0;
    for (; i < sizeof S->notice - 1 && s[i]; i++)
        S->notice[i] = s[i];
    S->notice[i] = 0;
    log(s, rc);
    S->lastDraw = 0;
}
void metrics()
{
    P->meters(&S->inLevel, &S->outLevel, &S->cpu);
    P->program_resources(&S->chain);
    (void)fx.status(&S->status);
    S->lastStatus = P->millis();
    S->cpuRead = clamp(S->cpu, 0, 100);
    const uint32_t h[4] = {S->gate, S->comp, S->a, S->b};
    int *on[4] = {&S->gateOn, &S->compOn, &S->aOn, &S->bOn};
    for (unsigned i = 0; i < 4 && S->ready; i++)
    {
        resona_effect_info n{};
        if (!fx.info(h[i], &n))
            *on[i] = n.enabled ? 1 : 0;
    }
}
int graph()
{
    if (!S->ready)
    {
        if (S->preview)
        {
            S->activeScene = sceneNow();
            S->activeMix = S->mix;
            S->activeBypass = bypassNow();
            return 0;
        }
        return S->rc;
    }
    fx.clear_routes();
    uint32_t input = S->input ? fx.Usb : fx.Guitar;
    const unsigned scene = sceneNow();
    if (bypassNow())
        fx.output(input, .8f);
    else
    {
        fx.connect(input, S->gate);
        fx.connect(S->gate, S->trim);
        uint32_t tail = S->trim;
        for (unsigned i = 0; i < S->count; i++)
        {
            fx.connect(tail, S->extra[i]);
            tail = S->extra[i];
        }
        if (scene == 0)
            fx.output(tail);
        else
        {
            fx.connect(tail, S->comp);
            fx.connect(S->comp, S->a);
            if (scene == 1)
            {
                fx.connect(S->a, S->b);
                fx.output(S->b);
            }
            else
            {
                fx.connect(tail, S->b);
                fx.output(S->a, 1 - S->mix * .01f);
                fx.output(S->b, S->mix * .01f);
            }
        }
    }
    // Device playback: USB (unless it is the rack's input) and Bluetooth pass straight to the output.
    if (input != fx.Usb)
    {
        fx.connect(fx.Usb, fx.Output, 1, 0, 0);
        fx.connect(fx.Usb, fx.Output, 1, 1, 1);
    }
    fx.connect(fx.Bluetooth, fx.Output, 1, 0, 0);
    fx.connect(fx.Bluetooth, fx.Output, 1, 1, 1);
    fx.connect(fx.Main, fx.Capture, 1, 0, 0);
    fx.connect(fx.Main, fx.Capture, 1, 1, 1);
    int rc = fx.commit();
    if (!rc)
    {
        S->activeScene = scene;
        S->activeMix = S->mix;
        S->activeBypass = bypassNow();
    }
    metrics();
    return rc;
}
// Every App-set effect value; FIRE, DUB and KILL override theirs while held.
int controls()
{
    fx.clear_error();
    int rc = fx.param(S->a, 1, (S->fire ? 100 : S->drive) * .01f);
    if (!rc)
        rc = fx.param(S->a, 2, (S->fire && S->tone < 90 ? 90 : S->tone) * .01f);
    if (!rc)
        rc = fx.param(S->a, 6, S->level * .01f);
    if (!rc)
        rc = fx.param(S->b, 3, (float)S->delay);
    if (!rc)
        rc = fx.param(S->b, 4, (S->dub ? 68 : S->feedback) * .01f);
    if (!rc)
        rc = fx.param(S->b, 5, sceneNow() == 1 ? (S->dub ? .6f : .35f) : 1.f);
    if (!rc)
        rc = fx.param(S->gate, 0, S->gateThr * .01f);
    if (!rc)
        rc = fx.param(S->trim, 0, S->kill ? 0.f : S->trimGain * .01f);
    return rc;
}
#ifdef RESONA_DEMO_BENCH
int add()
{
    if (S->count == 16)
        return RESONA_ERR_FULL;
    fx.clear_error();
    uint32_t h = fx.pack("org.resona.demo.prism-trim", "0.1.0");
    int rc = fx.error();
    if (!h)
        return rc;
    S->extra[S->count++] = h;
    rc = graph();
    if (rc)
    {
        S->count--;
        fx.clear_error();
        int destroy = fx.destroy(h);
        if (destroy)
            return destroy;
    }
    metrics();
    return rc;
}
int remove()
{
    if (!S->count)
        return RESONA_ERR_STATE;
    uint32_t h = S->extra[--S->count];
    int rc = graph();
    if (rc)
    {
        S->count++;
        return rc;
    }
    fx.clear_error();
    rc = fx.destroy(h);
    metrics();
    return rc;
}
#endif
Prefs current()
{
    Prefs c = {0x384d5250u, 2,       S->scene,    S->mix,      S->drive,    S->delay,    S->input,
               S->bypass,   S->tone, S->level,    S->feedback, S->gateThr,  S->trimGain, 0};
    c.crc = resona_splash_crc32(&c, offsetof(Prefs, crc));
    return c;
}
void save()
{
    Prefs c = current();
    if (!memcmp(&c, &S->saved, sizeof c))
    {
        const int audio = S->rc;
        note("SETTINGS ALREADY SAVED", 0);
        if (!S->ready)
            S->rc = audio;
        return;
    }
    int rc = P->app_store_write(0, &c, sizeof c);
    if (!rc)
        S->saved = c;
    const int audio = S->rc;
    note(rc ? "SETTINGS NOT SAVED" : "APP SETTINGS SAVED", rc);
    if (!S->ready)
        S->rc = audio;  // a storage result is not an audio error: keep why audio is off
}
#ifdef RESONA_DEMO_BENCH
void heapProbe()
{
    void *blocks[128] = {};
    unsigned n = 0;
    for (; n < 128; n++)
    {
        blocks[n] = P->alloc(256, 16);
        if (!blocks[n])
            break;
    }
    S->heapBlocks = n;
    S->heapBig = 0;
    if (n)
    {
        P->free(blocks[--n]);
        blocks[n] = nullptr;
        void *large = P->alloc(32768, 16);
        S->heapBig = large ? 1 : 0;
        if (large)
            P->free(large);
    }
    for (unsigned i = 0; i < n; i++)
        P->free(blocks[i]);
    char text[100];
    P->format(text, sizeof text, "PRISM HEAP small_blocks=%u bytes=%u large_after_free_one=%u",
              S->heapBlocks, S->heapBlocks * 256, S->heapBig);
    P->log(text);
    note("HEAP PROBE RELEASED ALL", 0);
}
void timerProbe()
{
    unsigned n = 0;
    int rc = 0;
    for (; n < 9; n++)
    {
        rc = P->timer_start(50 + n, 60000, 0);
        if (rc)
            break;
    }
    S->timerCount = n;
    S->timerRc = rc;
    for (unsigned i = 0; i < n; i++)
        P->timer_stop(50 + i);
    char text[100];
    P->format(text, sizeof text, "PRISM TIMERS accepted=%u next_rc=%d all_stopped=1", n, rc);
    P->log(text);
    note("TIMER PROBE COMPLETE", n == 8 && rc < 0 ? 0 : rc);
}
void cycle()
{
    if (!S->ready)
        return;
    resona_effects_status before{}, after{};
    fx.status(&before);
    fx.clear_routes();
    fx.connect(S->trim, S->trim);
    int rc = fx.commit();
    fx.status(&after);
    bool kept = before.generation == after.generation && after.active;
    note(kept && rc == RESONA_ERR_ARG ? "CYCLE REJECTED GRAPH KEPT" : "CYCLE CHECK FAILED",
         kept && rc == RESONA_ERR_ARG ? 0 : RESONA_ERR_STATE);
}
void splash(bool variant)
{
    const void *data = kSplash;
    uint32_t bytes = sizeof kSplash;
    if (variant)
    {
        memcpy(S->h->splash, kSplash, bytes);
        resona_splash_put(S->h->splash + 12, 1800);
        data = S->h->splash;
    }
    int rc = resona_startup_splash_apply(P, data, bytes);
    note(rc == 1 ? "SPLASH SAVED REBOOT TO SEE" : "SPLASH ALREADY MATCHES", rc);
}
#endif
void command(unsigned cc, unsigned value)
{
    int rc = 0;
    if (cc == 20)
    {
        S->scene = value % 3;
        rc = S->ready ? controls() : (S->preview ? 0 : S->rc);
        if (!rc)
            rc = graph();
        note("SCENE", rc);
    }
    else if (cc == 21)
    {
        S->mix = clamp(value, 0, 100);
        rc = graph();
        note("BLEND", rc);
    }
    else if (cc == 22)
    {
        S->input = value ? 1 : 0;
        rc = graph();
        note("INPUT", rc);
    }
    else if (cc == 23)
    {
        S->bypass = value ? 1 : 0;
        rc = graph();
        note("BYPASS", rc);
    }
    else if (cc == 25)
    {
        S->drive = clamp(value, 0, 100);
        rc = S->ready ? controls() : (S->preview ? 0 : S->rc);
        note("DRIVE", rc);
    }
    else if (cc == 26)
    {
        S->delay = 40 + clamp(value, 0, 127) * 460 / 127;
        rc = S->ready ? controls() : (S->preview ? 0 : S->rc);
        note("DELAY TIME", rc);
    }
#ifdef RESONA_DEMO_BENCH  // (25/26 above serve the RACK page's bottom encoder in every build)
    else if (cc == 24)
    {
        if (value == 0)
            rc = add();
        else if (value == 1)
            rc = remove();
        else if (value == 2)
        {
            S->grow = 1;
            note("GROW STARTED", 0);
            return;
        }
        else if (value == 3)
        {
            S->grow = -1;
            note("FREE EXTRAS STARTED", 0);
            return;
        }
        note("INSTANCE", rc);
    }
    else if (cc == 27)
    {
        save();
    }
    else if (cc == 28)
    {
        if (value == 0)
            heapProbe();
        else if (value == 1)
            timerProbe();
        else if (value == 2)
            cycle();
        else if (value == 3)
        {
            uint8_t x = 0;
            uint32_t capacity = P->app_store_size(), offset = capacity, len = 1;
            rc = P->app_store_read(offset, &x, len);
            note(rc == RESONA_ERR_ARG ? "STORE RANGE REJECTED" : "STORE RANGE CHECK FAILED",
                 rc == RESONA_ERR_ARG ? 0 : RESONA_ERR_STATE);
        }
        else if (value == 6)
        {
            rc = resona_storage_contract(P);
            metrics();
            note("STORAGE CONTRACT COMPLETE", rc);
        }
        else if (value == 7)
        {
            rc = sdk_probe::lifecycle(P); metrics(); note("LIFECYCLE PROBE", rc);
        }
        else if (value == 8)
        {
            rc = sdk_probe::timer_start(P); note("TIMER DELIVERY STARTED", rc);
        }
        else if (value == 9)
        {
            rc = sdk_probe::allocation(P); note("ALLOCATION PROBE", rc);
        }
        else if (value == 10)
        {
            sdk_probe::P=P; note("HEAP SLOT CHECK", 0); sdk_probe::message("HEAP-SLOTS",0,sdk_probe::heap_slots());
        }
        else if (value == 11)
        {
            rc=sdk_probe::builder(P); metrics(); note("BUILDER SCOPE PROBE",rc);
        }
        else if (value >= 60 && value <= 62)
        {rc=round2::post(P,value-60);note("POST DELIVERY PROBE",rc);}
        else if (value == 65)
        {event_contract_begin(P);note("EVENT CONTRACT PROBE",0);}
        else if (value == 63 || value == 64)
        {rc=round2::timers(P,value-63);note("EXTENDED TIMER PROBE",rc);}
        else if (value >= 70 && value <= 79)
        {rc=round2::routing(P,value-70,S->trim);metrics();note("AUDIO ROUTING PROBE",rc);}
        else if (value == 80 || value == 84)
        {rc=round3::formatting(P,value==84);note("ROUND3 FORMAT PROBE",rc);}
        else if (value == 81)
        {rc=round3::metadata(P);note("ROUND3 METADATA PROBE",rc);}
        else if (value == 82)
        {rc=round3::rgb_begin(P);note("ROUND3 RGB PROBE",rc);}
        else if (value == 83)
        {rc=round3::rgb_invalid(P);note("ROUND3 RGB VALIDATION",rc);}
        else if (value == 85)
        {rc=round3::large_begin(P);note("ROUND3 LARGE FORMAT",rc);}
        else if (value == 86)
        {rc=round3::eq_roundtrip(P);note("ROUND3 EQ PROBE",rc);}
        else if (value == 87)
        {rc=resona_format_eq_contract(P);metrics();note("FORMAT EQ CONTRACT",rc);}
        else if (value == 90)
        {rc=round4::memory(P);metrics();note("ROUND4 MEMORY PROBE",rc);}
        else if (value == 91)
        {rc=round4::ui_begin(P);note("ROUND4 UI PROBE",rc);}
        else if (value == 92)
        {rc=round4::ui_begin(P,1);note("ROUND4 FOCUS PROBE",rc);}
        else if (value == 93)
        {rc=ui_fixed::begin(P);note("UTF8 FONT CIRCLE PROBE",rc);}
        else if (value == 94)
        {rc=ui_fixed::show(P);note("LIVE FONT SCREEN",rc);}
        else if (value == 100)
        {rc=round5::begin(P);note("ROUND5 CONSTRUCTOR PROBE",rc);}
        else if (value == 101)
        {rc=round5::metadata(P,S->gate,0);if(!rc)rc=round5::metadata(P,S->comp,1);note("ROUND5 PARAMETER METADATA",rc);}
        else if (value == 102)
        {rc=round5::setters(P);metrics();note("ROUND5 PARAMETER SETTERS",rc);}
        else if (value == 104)
        {rc=round6::names(P);note("NAME HELPER PROBE",rc);}
        else if (value == 105)
        {rc=round6::splash(P);note("SPLASH HELPER PROBE",rc);}
        else if (value == 89)
        {ui_fixed::stop();rc=P->effect_param_set(S->trim,0,.8f);if(!rc)rc=graph();metrics();note("NORMAL GRAPH RESTORED",rc);}
        else if (value >= 50 && value <= 54)
        {
            const unsigned lengths[]={128,255,256,512,513};
            rc=sdk_probe::midi_send(P,lengths[value-50]); note("MIDI TX PROBE",rc);
        }
        else if (value >= 20 && value <= 39)
        {
            rc=sdk_probe::rejection(P,value,S->gate,S->trim); metrics(); note("REJECTION PROBE",rc);
        }
        else if (value == 4)
        {
            metrics();
            log("STATUS", 0);
            char t[128];
            P->format(t, sizeof t,
                      "PRISM UI frames=%u draw_max_ms=%u init_elapsed_ms=%u heap_budget=196608",
                      S->frames, S->drawMax, S->initMs);
            P->log(t);
        }
    }
    else if (cc == 29)
    {
        S->page = value % 3;
        S->selected = 0;
        S->lastDraw = 0;
    }
    else if (cc == 30)
    {
        splash(value != 0);
    }
    else if (cc == 31)
    {
        log("SDK REBOOT", 0);
        P->reboot();
    }
#endif
}
#ifdef RESONA_DEMO_BENCH
void labAction(unsigned k)
{
    switch (k)
    {
    case 0:
        command(22, !S->input);
        break;
    case 1:
        command(24, 0);
        break;
    case 2:
        command(24, 1);
        break;
    case 3:
        command(24, 2);
        break;
    case 4:
        command(24, 3);
        break;
    case 5:
        command(28, 0);
        break;
    case 6:
        command(28, 1);
        break;
    case 7:
        command(28, 2);
        break;
    case 8:
        command(28, 3);
        break;
    case 9:
        command(27, 0);
        break;
    case 10:
        command(30, 0);
        break;
    case 11:
        command(30, 1);
        break;
    case 12:
        command(31, 0);
        break;
    }
}
#endif
uint32_t handleAt(unsigned i)
{
    if (i == 0)
        return S->gate;
    if (i == 1)
        return S->comp;
    if (i == 2)
        return S->a;
    if (i == 3)
        return S->b;
    if (i == 4)
        return S->trim;
    return S->extra[i - 5];
}
uint32_t selectedHandle() { return handleAt(S->selected); }
// A controller changed a value or pressed a pad: do what the knobs do (command() is their path).
void controller(const ctl::Msg &m)
{
    static const char *names[] = {"DRIVE", "DRIVE TONE", "DRIVE LEVEL", "DELAY TIME", "ECHO FEEDBACK",
                                  "GATE THRESHOLD"};
    S->lastDraw = 0;
    if (m.cc <= CC_BYPASS)
        command(m.cc, m.cc == CC_SCENE ? S->scene : m.cc == CC_BLEND ? S->mix : m.cc == CC_INPUT ? S->input : S->bypass);
    else if (m.cc <= CC_GATE || m.cc == CC_TRIM)
        note(m.cc == CC_TRIM ? "INPUT TRIM" : names[m.cc - CC_DRIVE],
             S->ready ? controls() : (S->preview ? 0 : S->rc));
    else if (m.cc == CC_SAVE)
    {
        if (m.down)
            save();
    }
    else if (m.cc <= CC_DRY)
    {
        (m.cc == CC_FIRE ? S->fire : m.cc == CC_DUB ? S->dub : m.cc == CC_KILL ? S->kill : S->dry) = m.down;
        if (!S->ready && !S->preview)
            return;  // audio is off: keep the reason on screen
        int rc = S->ready ? controls() : 0;
        if (!rc && (sceneNow() != S->activeScene || bypassNow() != S->activeBypass))
            rc = graph();
        if (rc)
            note("PAD", rc);
    }
    else if (m.cc >= CC_GATE_ON && m.cc <= CC_ECHO_ON && S->ready)
    {
        const uint32_t h[4] = {S->gate, S->comp, S->a, S->b};
        const int *on[4] = {&S->gateOn, &S->compOn, &S->aOn, &S->bOn};
        note("NODE ENABLE", fx.enable(h[m.cc - CC_GATE_ON], *on[m.cc - CC_GATE_ON] != 0));
    }
}
void gesture(unsigned input, unsigned kind, int value)
{
    if (kind == RESONA_EVENT_LONG)
    {
        save();
        return;
    }
    if (input == RESONA_INPUT_SELECTOR)
    {
        if (kind == RESONA_EVENT_CLICK)
        {
            S->page = (S->page + 1) % kPages;
            S->selected = 0;
        }
        else if (kind == RESONA_EVENT_TURN)
        {
            if (S->page == 0)
                command(20, (S->scene + value % 3 + 3) % 3);
            else
                S->selected =
                    clamp((int)S->selected + value, 0, S->page == 1 ? (int)S->count + 4 : 12);
        }
    }
    else if (kind == RESONA_EVENT_TURN)
    {
        if (S->page == 0)
            command(21, clamp((int)S->mix + value * 2, 0, 100));
        else if (S->page == 1 && (S->selected == 2 || S->selected == 3))
            command(S->selected == 3 ? 26 : 25,
                    S->selected == 3 ? clamp((int)((S->delay - 40) * 127 / 460) + value, 0, 127)
                                     : clamp((int)S->drive + value * 2, 0, 100));
    }
    else if (kind == RESONA_EVENT_CLICK)
    {
        if (S->page == 0)
            command(23, !S->bypass);
        else if (S->page == 1 && S->ready)
        {
            resona_effect_info n{};
            int rc = fx.info(selectedHandle(), &n);
            if (!rc)
                rc = fx.enable(n.handle, !n.enabled);
            note("NODE ENABLE", rc);
        }
#ifdef RESONA_DEMO_BENCH
        else if (S->page == 2)
            labAction(S->selected);
#endif
    }
    S->lastDraw = 0;
}
int init(void *state, const resona_platform *p)
{
    P = p;
    S = (State *)state;
    S->h = (Heap *)P->alloc(sizeof(Heap), 16);
    if (!S->h)
        return RESONA_ERR_NO_MEMORY;
    S->scene = 2;
    S->mix = 35;
    S->drive = 25;
    S->delay = 180;
    S->tone = 60;
    S->level = 80;
    S->feedback = 25;
    S->trimGain = 80;
    S->gateOn = S->compOn = S->aOn = S->bOn = 1;
    S->start = P->millis();
    Prefs c{};
    int read = P->app_store_read(0, &c, sizeof c);
    bool ok = !read && c.magic == 0x384d5250u && c.version == 2 &&
              c.crc == resona_splash_crc32(&c, offsetof(Prefs, crc));
    if (ok)
        S->saved = c;
    else
    {  // 0.1 settings: the controllers' values take their defaults
        PrefsV1 v{};
        ok = !P->app_store_read(0, &v, sizeof v) && v.magic == 0x384d5250u && v.version == 1 &&
             v.crc == resona_splash_crc32(&v, offsetof(PrefsV1, crc));
        c = {v.magic, 2, v.scene, v.mix, v.drive, v.delay, v.input, v.bypass, 60, 80, 25, 0, 80, 0};
    }
    if (ok && c.scene < 3 && c.mix <= 100 && c.drive <= 100 && c.delay >= 40 && c.delay <= 500 &&
        c.input < 2 && c.bypass < 2)
    {
        S->scene = c.scene;
        S->mix = c.mix;
        S->drive = c.drive;
        S->delay = c.delay;
        S->input = c.input;
        S->bypass = c.bypass;
        S->tone = (unsigned)clamp(c.tone, 0, 100);
        S->level = (unsigned)clamp(c.level, 0, 100);
        S->feedback = (unsigned)clamp(c.feedback, 0, 70);
        S->gateThr = (unsigned)clamp(c.gateThr, 0, 100);
        S->trimGain = (unsigned)clamp(c.trimGain, 0, 100);
    }
    bind();
    P->input_config(0, 0);
    P->input_config(1, 0);
    resona_rgb_config rgb = resona_rgb_solid(40, 226, 194, 70);
    (void)P->rgb_set(&rgb);
    resona_device_info device{};
    P->device_info(&device);
    S->preview = !strcmp(device.serial, "LOCAL-PREVIEW");
    if (S->preview)
    {
        S->activeScene = S->scene;
        S->activeMix = S->mix;
        note("UI PREVIEW / DEVICE DSP ONLY", 0);
        return 0;
    }
    const int startup = resona_startup_splash_apply(P, kSplash, sizeof kSplash);
    if (startup < 0 && startup != RESONA_ERR_UNSUPPORTED)
        P->log("Prism Rack: startup animation could not be saved");
    int rc = fx.begin(p);
    if (rc)
    {
        note("EFFECT GRAPH UNAVAILABLE", rc);
        return 0;
    }
    S->gate = fx.factory(RESONA_ENGINE_GATE);
    S->comp = fx.factory(RESONA_ENGINE_COMP);
    S->a = fx.pack("org.resona.demo.prism-cell", "0.1.0");
    S->b = fx.pack("org.resona.demo.prism-cell", "0.1.0");
    S->trim = fx.pack("org.resona.demo.prism-trim", "0.1.0");
    if (!S->gate || !S->comp || !S->a || !S->b || !S->trim)
    {
        note("INSTALL CELL AND TRIM PACKS", fx.error());
        return 0;
    }
    S->ready = 1;
    rc = fx.param(S->a, 0, 1);
    if (!rc)
        rc = fx.param(S->b, 0, 2);
    if (!rc)
        rc = controls();  // gate threshold, trim, drive, tone, level, time, feedback, wet
    if (!rc)
        rc = graph();
    note("READY", rc);
    S->initMs = P->millis() - S->start;
    return 0;
}
// The OS bypassed one of our effects for running over the CPU budget. The instance is the
// role, counted in creation order (gate, comp, cell A, cell B, trim, extras); audio waits,
// longer on each trip, before that instance is enabled again.
void tripped(int role)
{
    if (!S->ready && !S->waiting)
        return;  // nothing of ours is playing (packs missing, preview)
    const uint32_t now = P->millis();
    S->ready = 0;
    S->waiting = 1;
    S->tripped = role < (int)(5 + S->count) ? (uint32_t)role : ~0u;
    S->retryAt = now + trip_guard::tripped(S->trip, now);
    note("CPU LIMIT", RESONA_ERR_BUDGET);
}
void retry(uint32_t now)
{
    const uint32_t h = S->tripped != ~0u ? handleAt(S->tripped) : 0;
    int rc = h ? fx.enable(h, 1) : 0;
    if (!h)  // unknown instance: enable whichever reports a fault
        for (unsigned i = 0; i < 5 + S->count && !rc; i++)
        {
            resona_effect_info n{};
            if (!fx.info(handleAt(i), &n) && n.fault)
                rc = fx.enable(n.handle, 1);
        }
    S->ready = 1;
    if (!rc)
        rc = controls();
    if (!rc)
        rc = graph();
    if (rc)
    {
        S->ready = 0;
        S->retryAt = now + 2000;
        note("CPU LIMIT", RESONA_ERR_BUDGET);
        return;
    }
    S->waiting = 0;
    note("AUDIO BACK ON", 0);
}
void event(void *, const resona_app_event *e)
{
#ifdef RESONA_DEMO_BENCH
    if(event_contract_event(e))return;
    if(round2::event(e))return;
    sdk_probe::event(e);
    if (sdk_probe::midi_event(P,e))
        return;
#endif
    if (demo_capture::event(P, e))
        return;
    ctl::Msg m;
    if (ctl::receive(P, S->link, kBind, kBinds, e, m))
    {
        if (m.changed)
            controller(m);
        return;
    }
    if (e->kind == RESONA_APP_EV_INPUT)
        gesture(e->u.input.input, e->u.input.kind, e->u.input.value);
    else if (e->kind == RESONA_APP_EV_AUDIO && e->u.audio.tripped_role >= 0)
        tripped(e->u.audio.tripped_role);
#ifdef RESONA_DEMO_BENCH
    else if (e->kind == RESONA_APP_EV_MIDI && e->u.midi.len == 3 && e->u.midi.data[0] == 0xbf &&
             e->u.midi.data[1] >= 20 && e->u.midi.data[1] <= 31)
    {
        unsigned cc = e->u.midi.data[1], v = e->u.midi.data[2];
        command(cc, v);
        uint8_t msg[] = {0xf0,
                         0x7d,
                         0x50,
                         0x52,
                         0x38,
                         (uint8_t)cc,
                         (uint8_t)v,
                         (uint8_t)(S->rc > 0   ? 64 + S->rc
                                   : S->rc < 0 ? -S->rc
                                               : 0),
                         (uint8_t)S->page,
                         (uint8_t)S->scene,
                         (uint8_t)S->mix,
                         (uint8_t)S->count,
                         (uint8_t)S->status.instances,
                         0xf7};
        P->midi_send(RESONA_MIDI_USB, msg, sizeof msg);
    }
#endif
}
// Animate a signal marker along a cable only while its route has signal.
void cable(gfx::Canvas &c, int x, int y, int endX, int endY, bool powered,
           uint16_t tint, unsigned phase)
{
    c.line(x, y, endX, endY, powered ? tint : gfx::EDGE);
    if (!powered)
        return;
    const int progress = (int)(phase % 100);
    const int px = x + (endX - x) * progress / 100;
    const int py = y + (endY - y) * progress / 100;
    c.rect(px - 1, py - 1, 3, 3, gfx::WHITE);
}

void meter(gfx::Canvas &c, int x, unsigned level, uint16_t tint)
{
    for (unsigned i = 0; i < 12; ++i)
        c.rect(x + (int)i * 8, 138, 6, 7,
               level * 12 > i * 90 ? (i >= 10 ? gfx::AMBER : tint) : gfx::EDGE);
}

void frame(void *, uint32_t)
{
#ifdef RESONA_DEMO_BENCH
    round5::frame(P);
    if(ui_fixed::frame(P))return;
    if(round4::frame(P))return;
    round3::frame(P);
    event_contract_frame();
    round2::frame(P);
    sdk_probe::frame(P);
#endif
    uint32_t now = P->millis();
    if (demo_capture::paused(P))
        return;
    if (S->waiting && (int32_t)(now - S->retryAt) >= 0)
        retry(now);
    // platform API 1 reports no role with RESONA_APP_EV_AUDIO: look for a bypassed instance once a second
    if (S->ready && !S->waiting && now - S->faultScan >= 1000)
    {
        S->faultScan = now;
        for (unsigned i = 0; i < 5 + S->count; i++)
            if (trip_guard::faulted(P, handleAt(i)))
            {
                tripped((int)i);
                break;
            }
    }
#ifdef RESONA_DEMO_BENCH
    if (S->grow && now - S->lastStatus >= 250)
    {
        int rc = S->grow > 0 ? add() : remove();
        if (rc || (!S->count && S->grow < 0) || S->count == 16)
        {
            S->grow = 0;
            note("GROW STOP / GRAPH KEPT", rc);
        }
        S->lastStatus = now;
    }
#endif
    if (now - S->lastStatus >= 500)
        metrics();
    ctl::sync(P, S->link, kBind, kBinds, now);
    if (S->lastDraw && now - S->lastDraw < 32)
        return;
    S->lastDraw = now;
    if (S->page == 0 && S->ready)
    {
        P->meters(&S->inLevel, &S->outLevel, nullptr);
        const unsigned in = (unsigned)clamp(S->inLevel, 0, 9) * 10u;
        const unsigned out = (unsigned)clamp(S->outLevel, 0, 9) * 10u;
        S->uiIn = in > S->uiIn ? in : (S->uiIn * 3 + in) / 4;
        S->uiOut = out > S->uiOut ? out : (S->uiOut * 3 + out) / 4;
    }
    uint32_t start = P->millis();
    gfx::Canvas c{S->h->pixels, 320};
    c.clear(gfx::BG);
    char t[80];
    if (now - S->start < 700)
        gfx::splash(c, (now - S->start) / 175);
    else
    {
        const uint16_t navy = RESONA_RGB(9, 23, 36);
        const uint16_t panel = RESONA_RGB(15, 35, 49);
        const uint16_t lavender = RESONA_RGB(175, 158, 245);
        const uint16_t theme = S->activeScene == 0 ? gfx::CYAN :
                               S->activeScene == 1 ? gfx::AMBER : lavender;
        c.clear(navy);
        c.rect(0, 0, 320, 2, theme);
        c.text(12, 11, 2, gfx::WHITE, "PRISM RACK");
        c.rect(259, 8, 49, 19, panel);
        c.text(271, 14, 1, theme, S->page == 0 ? "LIVE" : S->page == 1 ? "RACK" : "LAB");
        c.rect(12, 33, 296, 1, gfx::EDGE);
        if (S->waiting)
        {
            c.text(14, 58, 2, gfx::AMBER, "CPU LIMIT");
            if (S->tripped != ~0u)
                c.text(14, 90, 1, gfx::MUTED, nodes[S->tripped < 5 ? S->tripped : 5]);
        }
        else if (!S->ready && !S->preview)
        {
            c.text(14, 58, 2, gfx::AMBER, "DEVICE DSP ONLY");
            c.text(14, 90, 1, gfx::MUTED, "INSTALL PRISM CELL + TRIM");
            P->format(t, sizeof t, "SDK RETURN %d", S->rc);
            c.text(14, 115, 1, gfx::RED, t);
        }
        else if (S->page == 0)
        {
            const bool bypass = S->activeBypass != 0;
            const bool input = S->uiIn >= 5;
            const bool output = S->uiOut >= 5;
            const bool signal = input && output;
            const unsigned phase = now / 5u;

            c.text(13, 43, 3, theme, scenes[S->activeScene]);
            c.text(14, 69, 1, gfx::MUTED, bypass ? "BYPASS" :
                   S->activeScene == 0 ? "CLEAN" : S->activeScene == 1 ? "SERIES" : "PARALLEL");
            c.text(209, 44, 1, gfx::MUTED, "BLEND");
            P->format(t, sizeof t, "%u%%", S->activeMix);
            c.text(241, 43, 2, gfx::WHITE, t);
            c.bar(209, 62, 98, 4, S->activeMix, 100, theme);
            if (S->fire || S->dub || S->kill || S->dry)
            {
                P->format(t, sizeof t, "%s%s%s%s", S->fire ? " FIRE" : "", S->dub ? " DUB" : "",
                          S->kill ? " KILL" : "", S->dry ? " DRY" : "");
                c.text(307 - 6 * (int)strlen(t), 73, 1, gfx::AMBER, t);
            }

            c.rect(12, 84, 296, 46, panel);
            c.text(20, 102, 1, gfx::WHITE, S->input ? "USB" : "GTR");
            c.text(280, 102, 1, gfx::WHITE, "OUT");
            if (bypass)
            {
                cable(c, 47, 105, 274, 105, signal, gfx::MUTED, phase);
            }
            else
            {
                c.frame(64, 96, 38, 19, gfx::EDGE);
                c.text(71, 102, 1, gfx::WHITE, "GATE");
                cable(c, 47, 105, 63, 105, input, gfx::CYAN, phase);
                if (S->activeScene == 2)
                {
                    const bool drive = signal && S->aOn, echo = signal && S->bOn;
                    cable(c, 102, 105, 133, 94, drive, gfx::CYAN, phase);
                    cable(c, 102, 105, 133, 119, echo, gfx::AMBER, phase + 40);
                    c.text(142, 91, 1, S->aOn ? gfx::CYAN : gfx::MUTED, "DRIVE");
                    c.text(142, 116, 1, S->bOn ? gfx::AMBER : gfx::MUTED, "ECHO");
                    cable(c, 179, 94, 244, 105, drive, gfx::CYAN, phase);
                    cable(c, 179, 119, 244, 105, echo, gfx::AMBER, phase + 40);
                }
                else
                {
                    cable(c, 102, 105, 125, 105, signal, theme, phase);
                    if (S->activeScene == 0)
                        c.text(151, 102, 1, theme, "TRIM");
                    else
                    {
                        c.text(129, 102, 1, S->aOn ? theme : gfx::MUTED, "DRIVE");
                        cable(c, 162, 105, 176, 105, signal, theme, phase);
                        c.text(182, 102, 1, S->bOn ? theme : gfx::MUTED, "ECHO");
                    }
                    cable(c, 213, 105, 244, 105, signal, theme, phase);
                }
                cable(c, 244, 105, 274, 105, output, theme, phase);
            }
            c.text(14, 138, 1, gfx::MUTED, "IN");
            meter(c, 34, S->uiIn, gfx::CYAN);
            c.text(181, 138, 1, gfx::MUTED, "OUT");
            meter(c, 211, S->uiOut, theme);
        }
        else if (S->page == 1)
        {
            const unsigned first = S->selected / 5 * 5;
            for (unsigned i = first; i < first + 5 && i < 5 + S->count; ++i)
            {
                const int y = 39 + (int)(i - first) * 19;
                const bool selected = i == S->selected;
                resona_effect_info n{};
                const bool known = S->ready && fx.info(handleAt(i), &n) == 0;
                const uint16_t state = !known ? gfx::MUTED : n.enabled ? gfx::CYAN : gfx::AMBER;
                c.rect(12, y, 296, 17, selected ? panel : gfx::BG);
                if (selected)
                    c.rect(12, y, 3, 17, theme);
                P->format(t, sizeof t, "%02u %s", i + 1, nodes[i < 5 ? i : 5]);
                c.text(20, y + 5, 1, selected ? gfx::WHITE : gfx::MUTED, t);
                c.rect(184, y + 6, 5, 5, state);
                c.text(195, y + 5, 1, state, !known ? "--" : n.enabled ? "ON" : "OFF");
                const unsigned cpu = S->chain.period_ticks ? n.last_ticks * 100 / S->chain.period_ticks : 0;
                if (known)
                {
                    P->format(t, sizeof t, "%u%%", cpu);
                    c.text(228, y + 5, 1, gfx::MUTED, t);
                    c.bar(266, y + 6, 33, 5, cpu, 25, state);
                }
            }
            if (S->selected == 2)
                P->format(t, sizeof t, "TURN: DRIVE %u%%", S->drive);
            else if (S->selected == 3)
                P->format(t, sizeof t, "TURN: TIME %uMS", S->delay);
            else
                t[0] = 0;
            c.text(14, 138, 1, theme, t);
            c.text(208, 138, 1, gfx::MUTED, "PRESS: ON/OFF");
        }
#ifdef RESONA_DEMO_BENCH
        else
        {
            c.text(14, 44, 1, gfx::CYAN, labItems[S->selected]);
            P->format(t, sizeof t, "INST %u   ROUTES %u   FAULTS %u", S->status.instances,
                      S->status.routes, S->status.faults);
            c.text(14, 65, 1, gfx::WHITE, t);
            P->format(t, sizeof t, "CPU %u%%  RESERVED %u%%", S->cpu,
                      S->chain.period_ticks ? S->status.reserved_ticks * 100 / S->chain.period_ticks
                                            : 0);
            c.text(14, 83, 1, gfx::MUTED, t);
            c.bar(14, 96, 140, 3, S->cpu, 100, gfx::CYAN);
            P->format(t, sizeof t, "FAST %uK  PSRAM %uK FREE", S->chain.fast_free / 1024,
                      S->chain.psram_free / 1024);
            c.text(14, 108, 1, gfx::MUTED, t);
            P->format(t, sizeof t, "HEAP %u BLOCKS BIG %u TIMER %u", S->heapBlocks, S->heapBig,
                      S->timerCount);
            c.text(14, 126, 1, gfx::MUTED, t);
        }
#endif
        c.rect(12, 151, 296, 1, gfx::EDGE);
        P->format(t, sizeof t, "%.40s", S->preview ? "" : S->notice);
        c.text(12, 159, 1, S->rc < 0 ? gfx::RED : gfx::MUTED, t);
        P->format(t, sizeof t, "CPU%u", S->cpu);
        c.text(266, 159, 1, gfx::CYAN, t);
    }
    uint16_t *px = nullptr;
    uint32_t stride = 0;
    resona_ui *u = P->screen_begin(&px, &stride);
    if (!u || !px)
        return;
    for (unsigned y = 0; y < 172; y++)
        memcpy(px + y * stride, S->h->pixels + y * 320, 640);
    demo_capture::remember(px, stride);
    P->screen_present(0, 0, 320, 172);
    S->frames++;
    unsigned took = P->millis() - start;
    if (took > S->drawMax)
        S->drawMax = took;
}
const resona_app app = {RESONA_PLATFORM_VERSION,
                        sizeof(resona_app),
                        RESONA_APP_ID,
                        RESONA_APP_NAME,
                        RESONA_APP_VERSION,
                        sizeof(State),
                        196608,
                        init,
                        event,
                        frame};
} // namespace
RESONA_EXPORT const resona_app *resona_app_get() { return &app; }
