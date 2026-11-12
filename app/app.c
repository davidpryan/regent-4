/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 David Ryan */
/* REGENT-4: the unity build, after fm1-base's platform.
 *   core/   shared with the other five firmwares      seq.c  the chord and the parts      ui.c  controls and screen */
#include "fastmath.h"
#include "core/util.c"
#include "core/synth.c"
#include "core/clock.c"
#include "core/slots.c"
#include "core/kit.c"
#include "seq.c"
#include "ui.c"

void app_init(void)
{
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) != (int)sizeof set || set.magic != SET_MAGIC || set.slot >= NSLOTS) {
        set.magic = SET_MAGIC;
        set.slot = 0;
        set.ble_off = 0;
    }
    if (!slot_load(set.slot, PROJ_MAGIC, &P, sizeof P))
        project_new();
    project_apply();
    H.seed = 0x5EED;
    H.follow = P.prog_n > 0u;
    H.now.deg = 0xFFu;
    U.held_white = -1;
    U.page = PG_COND;
    K.card = NB;
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    clock_render(lr, n, (float)master_q12 * (1.0f / 4096.0f) * 7000000.0f);
}

void app_panic(void)
{
    T.playing = 0;
    voices_silence();
}

void app_ui_init(void)
{
    touch_on(1);
    lcd_fill(0, 0, 240, 240, K_BG);
    K.redraw = 1;
}

void app_input_only(void) { input(); }

void app_frame(void)
{
    static uint32_t last;
    uint32_t now = plat_ms(), i;
    input();
    if (ann_on() && ann_where()) {
        char t[48];
        s_cpy(t, APP_NAME ". ");
        s_cat(t, U.page == PG_COND ? "CONDUCTOR" : U.page == PG_PROG ? "PROGRESSION" : "SETUP");
        ann_say(t, 0, ANN_NOW);
    }
    ann_tick(now);
    if (U.save_wait && !T.playing) {
        U.save_wait = 0;
        project_save_now();
    }
    if (K.msg_until && now >= K.msg_until) {
        K.msg_until = 0;
        K.redraw = 1;
    }
    if (T.playing && now - last >= 40u)
        K.redraw = 1;
    for (i = 0; i < 16u; i++)
        if (heard[i])
            heard[i]--;
    if (kit_hint_due(HINTS))
        K.redraw = 1;
    lights();
    if (K.redraw) {
        K.redraw = 0;
        last = now;
        draw();
    }
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "playing")) *got = T.playing;
    else if (!strcmp(what, "page")) *got = U.page;
    else if (!strcmp(what, "part")) *got = U.part;
    else if (!strcmp(what, "deg")) *got = H.now.deg == 0xFFu ? -1 : H.now.deg;
    else if (!strcmp(what, "mods")) *got = H.now.mods;
    else if (!strcmp(what, "nnotes")) *got = H.n;
    else if (!strcmp(what, "root")) *got = H.n ? H.notes[0] : -1;
    else if (!strcmp(what, "third")) *got = H.n > 1u ? H.notes[1] - H.notes[0] : -1;
    else if (!strcmp(what, "padn")) *got = H.pad_n;
    else if (!strcmp(what, "progn")) *got = P.prog_n;
    else if (!strcmp(what, "slot")) *got = H.slot;
    else if (!strcmp(what, "follow")) *got = H.follow;
    else if (!strcmp(what, "rec")) *got = H.rec;
    else if (!strcmp(what, "proglen0")) *got = P.prog[0].beats;
    else if (!strcmp(what, "prog1")) *got = P.prog[1].deg == 0xFFu ? -1 : P.prog[1].deg;
    else if (!strcmp(what, "muted")) *got = P.part[U.part].muted;
    else if (!strcmp(what, "bpm10")) *got = T.bpm10;
    else if (!strcmp(what, "bpm_near120")) *got = T.bpm10 >= 1150 && T.bpm10 <= 1250;
    else if (!strcmp(what, "card")) *got = K.card == NB ? -1 : K.card;
    else if (!strcmp(what, "dirty")) *got = U.dirty;
    else return 0;
    return 1;
}
#endif
