// MIT. Small native RGB565 canvas; shared by App UI and the splash asset tool.
#pragma once
#include <stdint.h>
#include <resona/module.h>
namespace gfx
{
constexpr int W = 320, H = 172;
constexpr uint16_t BG = RESONA_RGB(7, 14, 23), CARD = RESONA_RGB(14, 28, 39),
                   EDGE = RESONA_RGB(33, 60, 72), WHITE = RESONA_RGB(225, 241, 241),
                   MUTED = RESONA_RGB(111, 144, 158), CYAN = RESONA_RGB(40, 226, 194),
                   AMBER = RESONA_RGB(255, 165, 77), RED = RESONA_RGB(255, 88, 99);
static const uint8_t glyphs[36][5] = {{0x7e, 0x11, 0x11, 0x11, 0x7e},
                                      {0x7f, 0x49, 0x49, 0x49, 0x36},
                                      {0x3e, 0x41, 0x41, 0x41, 0x22},
                                      {0x7f, 0x41, 0x41, 0x22, 0x1c},
                                      {0x7f, 0x49, 0x49, 0x49, 0x41},
                                      {0x7f, 9, 9, 9, 1},
                                      {0x3e, 0x41, 0x49, 0x49, 0x7a},
                                      {0x7f, 8, 8, 8, 0x7f},
                                      {0, 0x41, 0x7f, 0x41, 0},
                                      {0x20, 0x40, 0x41, 0x3f, 1},
                                      {0x7f, 8, 0x14, 0x22, 0x41},
                                      {0x7f, 0x40, 0x40, 0x40, 0x40},
                                      {0x7f, 2, 0x0c, 2, 0x7f},
                                      {0x7f, 4, 8, 0x10, 0x7f},
                                      {0x3e, 0x41, 0x41, 0x41, 0x3e},
                                      {0x7f, 9, 9, 9, 6},
                                      {0x3e, 0x41, 0x51, 0x21, 0x5e},
                                      {0x7f, 9, 0x19, 0x29, 0x46},
                                      {0x46, 0x49, 0x49, 0x49, 0x31},
                                      {1, 1, 0x7f, 1, 1},
                                      {0x3f, 0x40, 0x40, 0x40, 0x3f},
                                      {0x1f, 0x20, 0x40, 0x20, 0x1f},
                                      {0x3f, 0x40, 0x38, 0x40, 0x3f},
                                      {0x63, 0x14, 8, 0x14, 0x63},
                                      {7, 8, 0x70, 8, 7},
                                      {0x61, 0x51, 0x49, 0x45, 0x43},
                                      {0x3e, 0x51, 0x49, 0x45, 0x3e},
                                      {0, 0x42, 0x7f, 0x40, 0},
                                      {0x42, 0x61, 0x51, 0x49, 0x46},
                                      {0x21, 0x41, 0x45, 0x4b, 0x31},
                                      {0x18, 0x14, 0x12, 0x7f, 0x10},
                                      {0x27, 0x45, 0x45, 0x45, 0x39},
                                      {0x3c, 0x4a, 0x49, 0x49, 0x30},
                                      {1, 0x71, 9, 5, 3},
                                      {0x36, 0x49, 0x49, 0x49, 0x36},
                                      {6, 0x49, 0x49, 0x29, 0x1e}};
struct Canvas
{
    uint16_t *p;
    uint32_t stride;
    void rect(int x, int y, int w, int h, uint16_t c)
    {
        int x1 = x + w, y1 = y + h;
        if (x < 0)
            x = 0;
        if (y < 0)
            y = 0;
        if (x1 > W)
            x1 = W;
        if (y1 > H)
            y1 = H;
        for (int j = y; j < y1; j++)
            for (int i = x; i < x1; i++)
                p[j * stride + i] = c;
    }
    void clear(uint16_t c) { rect(0, 0, W, H, c); }
    void frame(int x, int y, int w, int h, uint16_t c)
    {
        rect(x, y, w, 1, c);
        rect(x, y + h - 1, w, 1, c);
        rect(x, y, 1, h, c);
        rect(x + w - 1, y, 1, h, c);
    }
    void line(int x, int y, int xx, int yy, uint16_t c)
    {
        int dx = xx > x ? xx - x : x - xx, sx = x < xx ? 1 : -1, dy = yy > y ? y - yy : yy - y,
            sy = y < yy ? 1 : -1, e = dx + dy;
        for (;;)
        {
            rect(x, y, 1, 1, c);
            if (x == xx && y == yy)
                break;
            int a = 2 * e;
            if (a >= dy)
            {
                e += dy;
                x += sx;
            }
            if (a <= dx)
            {
                e += dx;
                y += sy;
            }
        }
    }
    void text(int x, int y, int scale, uint16_t c, const char *s)
    {
        for (; *s; s++, x += 6 * scale)
        {
            unsigned char k = *s;
            if (k >= 'a' && k <= 'z')
                k -= 32;
            const uint8_t *g = nullptr;
            if (k >= 'A' && k <= 'Z')
                g = glyphs[k - 'A'];
            else if (k >= '0' && k <= '9')
                g = glyphs[26 + k - '0'];
            if (g)
            {
                for (int col = 0; col < 5; col++)
                    for (int row = 0; row < 7; row++)
                        if (g[col] & (1 << row))
                            rect(x + col * scale, y + row * scale, scale, scale, c);
            }
            else if (k == '-')
                rect(x, y + 3 * scale, 5 * scale, scale, c);
            else if (k == '.')
                rect(x + 2 * scale, y + 6 * scale, scale, scale, c);
            else if (k == ':')
            {
                rect(x + 2 * scale, y + 2 * scale, scale, scale, c);
                rect(x + 2 * scale, y + 5 * scale, scale, scale, c);
            }
            else if (k == '/')
                line(x, y + 6 * scale, x + 4 * scale, y, c);
            else if (k == '%')
            {
                rect(x, y, 2 * scale, 2 * scale, c);
                rect(x + 3 * scale, y + 5 * scale, 2 * scale, 2 * scale, c);
                line(x, y + 6 * scale, x + 4 * scale, y, c);
            }
            else if (k == '+')
            {
                rect(x, y + 3 * scale, 5 * scale, scale, c);
                rect(x + 2 * scale, y + scale, scale, 5 * scale, c);
            }
        }
    }
    void bar(int x, int y, int w, int h, int value, int max, uint16_t c)
    {
        rect(x, y, w, h, EDGE);
        int fill = max > 0 ? w * value / max : 0;
        if (fill < 0)
            fill = 0;
        if (fill > w)
            fill = w;
        rect(x, y, fill, h, c);
    }
};
inline void splash(Canvas &c, unsigned phase)
{
    c.clear(BG);
    c.line(25, 86, 60, 43, CYAN);
    c.line(60, 43, 78, 113, CYAN);
    c.line(78, 113, 25, 86, CYAN);
    c.line(8, 88, 45, 82, WHITE);
    for (unsigned i = 0; i <= phase && i < 4; i++)
    {
        int y = 69 + 10 * i;
        c.line(66, y, 91, y - 3 * (int)i, i & 1 ? AMBER : CYAN);
    }
    c.text(104, 65, 2, WHITE, "PRISM RACK");
    c.text(105, 94, 1, MUTED, "CODE / SHAPE / MIX");
    c.text(28, 128, 1, MUTED, "A USER APP. SDK 8.");
    c.bar(28, 145, 264, 4, (int)phase + 1, 4, CYAN);
}
} // namespace gfx
