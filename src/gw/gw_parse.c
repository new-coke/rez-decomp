/* Debug GUI number text: integer and float parsing for the variable editor, bit strings. */

#include "gw_parse.h"

static int gw_parse_int_hex(char *s);

extern char str_gw_bits_ones[];
extern char str_gw_bits_zeros[];

/*
 * Low nbits, MSB first; each bit's character from that position of ones or zeros ('1'/'0' default).
 */
/* 0x001966a0 */
char *gw_format_bits(int val, int nbits, char *buf, char *ones, char *zeros)
{
    char *out;

    if (ones == 0)
        ones = str_gw_bits_ones;
    if (zeros == 0)
        zeros = str_gw_bits_zeros;
    val <<= -nbits;
    out = buf;
    while (--nbits != -1) {
        if (val < 0)
            *out = *ones;
        else
            *out = *zeros;
        out++;
        ones++;
        zeros++;
        val <<= 1;
    }
    *out = 0;
    return buf;
}

/*
 * Optional '-', digits, '.' fraction; stops at other characters. "0x" goes to the hex parser (0).
 */
/* 0x00196710 */
float gw_parse_float(char *s)
{
    int c;
    char ch;
    int dot;
    float val;
    float scale;
    float sign;
    int bits;

    while (c = *(unsigned char *)s, *s == ' ')
        s++;
    if (*(char *)&c == '0' && (s[1] == 'x' || s[1] == 'X')) {
        bits = gw_parse_int_hex(s);
        return *(float *)&bits;
    }
    scale = 1.0f;
    dot = 0;
    val = 0.0f;
    sign = 1.0f;
    if (*(char *)&c == '-') {
        s++;
        sign = -1.0f;
        c = *(unsigned char *)s;
    }
    for (;;) {
        ch = *(char *)&c;
        s++;
        if (ch == 0)
            goto done;
        c = ch - '0';
        if ((unsigned)c < 10) {
            if (dot) {
                scale *= 0.1f;
                val += c * scale;
                c = *(unsigned char *)s;
            } else {
                val *= 10.0f;
                val += c;
                c = *(unsigned char *)s;
            }
        } else if (ch == '.') {
            dot = 1;
            c = *(unsigned char *)s;
        } else {
            goto done;
        }
    }
done:
    return val * sign;
}

/* Bug: the digit loop tests a bit no character has, so always 0. */
/* 0x00196840 */
static int gw_parse_int_hex(char *s)
{
    int val;
    unsigned char c;

    val = 0;
    c = *s;
    while (*s == ' ')
        c = *++s;
    c = *s;
    if ((char)c == '0' && (s[1] == 'x' || s[1] == 'X'))
        c = s[2];
    while (c & 0x100) {
        val = val * 16 + c;
        c = *++s;
    }
    if (c == 0) {
        val <<= 4;
        val >>= 4;
    }
    return val;
}

/* Optional '-', then every decimal digit in the rest (others skipped). "0x" -> hex parser (0). */
/* 0x001968b8 */
int gw_parse_int(char *s)
{
    int c;
    int n;
    float sign;

    while (*s == ' ')
        s++;
    if (*s == '0' && (s[1] == 'x' || s[1] == 'X'))
        return gw_parse_int_hex(s);
    n = 0;
    sign = 1.0f;
    if (*s == '-') {
        s++;
        sign = -1.0f;
    }
    while ((c = *s++)) {
        if ((unsigned)(c - '0') < 10) {
            n = n * 10;
            n += c - '0';
        }
    }
    return (int)((float)n * sign);
}
