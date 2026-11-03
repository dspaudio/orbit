/* SPDX-License-Identifier: GPL-3.0-only */
/* Native object: the pinned LTO linker cannot resolve --wrap=memcpy when the
 * replacement itself is bitcode. Calibration needs this implementation in RAM. */
__attribute__((section(".ram_text.ble"), noinline))
void *__wrap_memcpy(void *d, const void *s, unsigned n)
{
    unsigned char *o = d;
    const unsigned char *i = s;
    while (n--)
        *o++ = *i++;
    return d;
}
