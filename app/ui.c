/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 David Ryan */
/* REGENT-4's controls and screen.
 *   white keys 1-14   chords: the degrees I..VII, then again an octave up      white 15  no chord   white 16  tap tempo
 *   black keys 1-7    held with a white key (or pressed while one is held): 7, sus2, sus4, add9, 6, flip, b5
 *   black keys 8-11   mute PAD, BASS, MOTIF A, MOTIF B
 *   ALGO the part (its four knobs)   PRESETS the key's root (SEL held: the scale)   OCT the part's octave
 *   REC record a progression by playing it   SEQ the progression plays   EDIT the progression page
 *   ARP dice a motif   LFO mute the part   FX no chord   HOME the conductor   GLO setup   SAVE save */

#define ACCENT RGB(120, 180, 255)
enum { PG_COND, PG_PROG, PG_SETUP };
static const uint16_t PART_C[NSOUND] = {RGB(190, 140, 255), RGB(255, 140, 90), RGB(120, 220, 160), RGB(250, 210, 90),
                                        RGB(200, 200, 200)};

#define SET_MAGIC 0x31474552u
typedef struct {
    uint32_t magic;
    uint8_t slot, ble_off, rsv[2];
} settings_t;
static settings_t set;

static struct {
    uint8_t page, part, grow, pslot;
    uint8_t dirty, save_wait;
    int8_t held_white;                                  /* the chord key held (-1 none) */
    int8_t live_pad[6];                                 /* stopped: the pad notes sounding */
    uint8_t live_n;
    uint32_t tap_t[4];
    uint8_t tap_n;
} U;

static const char *const CARD_TEXT[NB] = {
    "FX: NO CHORD. The parts fall silent until the next chord.",
    "SEL held + PRESETS: the scale. Every part follows at once.",
    0,
    "LFO mutes or unmutes the part shown (ALGO picks it).",
    ("EDIT: the PROGRESSION page. SELECT a slot, a white key sets its chord, black keys its colours, "
     "K1 its beats, FX clears it."),
    "GLO: SETUP. Tempo, when a chord changes (at once, beat, bar), clock, echo and the four project slots.",
    "HOME: the CONDUCTOR. The chord, its notes, and what each part plays from it.",
    "SAVE writes the project to its slot. While playing it saves at the stop.",
    "ARP: DICE. A new figure for the motif shown (MOTIF A or B).",
    ("SEQ: the PROGRESSION plays its chords, each for its beats, round and round. "
     "A chord you play takes over until the next slot."),
    "PLAY starts and stops.",
    "REC while playing: the chords you play become the progression, each as long as you held it.",
    "OCT-: the part shown an octave down.",
    "OCT+: the part shown an octave up.",
#if PLAT_SMK37
    0, 0, 0, 0,
#endif
};

static const char *const HINTS[NB] = {
    [B_SEL] = "PRESETS: SCALE",
};

static void dirty(void) { U.dirty = 1; }
static void settings_save(void) { plat_store_save(OBJ_SETTINGS, &set, sizeof set); }
int regent_ble_off(void) { return set.ble_off; }

static void project_save_now(void)
{
    int r;
    P.bpm10 = T.bpm10;
    r = slot_save(set.slot, PROJ_MAGIC, PROJ_VER, &P, sizeof P);
    if (r == 0)
        U.dirty = 0;
    app_say(r == 0 ? "SAVED" : "SAVE", r == 0 ? "" : "FAILED");
}
static void transport_wait_stop(void)
{
    uint32_t t0 = plat_ms();
    T.want = 2;
    while (T.playing && plat_ms() - t0 < 200u)
        plat_yield();
}

/* stopped: the chord still sounds, the pad from the keys */
static void live_pad_off(void)
{
    uint32_t i;
    for (i = 0; i < U.live_n; i++)
        live_note(PA_PAD, &P.ins[PA_PAD], U.live_pad[i], 0, 0);
    U.live_n = 0;
}

static uint8_t mods_held(void)
{
    uint32_t i, m = 0;
    for (i = 0; i < NMOD; i++)
        if (K.keys >> BKEY[i] & 1u)
            m |= 1u << i;
    return (uint8_t)m;
}

static void play_chord(uint32_t deg, uint32_t mods)
{
    chord_t c;
    c.deg = (uint8_t)deg;
    c.mods = (uint8_t)mods;
    c.beats = 4;
    c.rsv = 0;
    if (U.page == PG_PROG) {                            /* the progression page: the key sets the slot */
        P.prog[U.pslot] = c;
        if (U.pslot >= P.prog_n)
            P.prog_n = (uint8_t)(U.pslot + 1u);
        dirty();
    }
    if (T.playing) {
        H.cue = c;
        H.has_cue = 1;
    } else {
        uint32_t i;
        live_pad_off();
        chord_set(&c);
        if (deg != 0xFFu)
            for (i = 0; i < H.pad_n; i++) {
                live_note(PA_PAD, &P.ins[PA_PAD], H.pad[i], 80, 1);
                U.live_pad[U.live_n++] = H.pad[i];
            }
    }
}

static void tap_tempo(void)
{
    uint32_t now = plat_ms(), i, n = 0, sum = 0;
    if (U.tap_n && now - U.tap_t[(U.tap_n - 1u) % 4u] > 2000u)
        U.tap_n = 0;
    U.tap_t[U.tap_n % 4u] = now;
    U.tap_n++;
    for (i = 1; i < U.tap_n && i < 4u; i++) {
        sum += U.tap_t[(U.tap_n - i) % 4u] - U.tap_t[(U.tap_n - i - 1u) % 4u];
        n++;
    }
    if (n) {
        T.bpm10 = (uint16_t)clamp((int32_t)(600000u / (sum / n)), 300, 3000);
        P.bpm10 = T.bpm10;
        dirty();
    }
    app_say("TAP", "");
}

/* ---- SETUP rows ---- */
static void v_u8(char *o, uint32_t v) { i_text((int32_t)v, o); }
static void ic(uint8_t *f, int32_t d, int32_t lo, int32_t hi) { *f = (uint8_t)clamp((int32_t)*f + d, lo, hi); dirty(); }
static void gv_tempo(char *o) { o[0] = 0; i_cat(o, T.bpm10 / 10); s_cat(o, "."); i_cat(o, T.bpm10 % 10); }
static void gv_root(char *o) { s_cpy(o, ROOT_NM[P.root % 12u]); }
static void gv_scale(char *o) { s_cpy(o, SCALE_NM[P.scale % NSCALE]); }
static void gv_quant(char *o) { s_cpy(o, CQ_NM[P.quant % NCQ]); }
static void gv_clk(char *o) { s_cpy(o, CK_NM[T.clk_mode % NCK]); }
static void gv_clko(char *o) { s_cpy(o, T.clk_send ? "SEND" : "OFF"); }
static void gv_et(char *o) { v_u8(o, P.echo_time); }
static void gv_ef(char *o) { v_u8(o, P.echo_fb); }
static void gv_slot(char *o) { o[0] = 0; i_cat(o, set.slot + 1); s_cat(o, slot_used(set.slot, PROJ_MAGIC) ? "  (SAVED)" : "  (EMPTY)"); }
static void gv_ble(char *o) { s_cpy(o, set.ble_off ? "OFF" : "ON"); }
static void gc_tempo(int32_t d) { T.bpm10 = (uint16_t)clamp((int32_t)T.bpm10 + d * 10, 300, 3000); P.bpm10 = T.bpm10; dirty(); }
static void gc_root(int32_t d) { P.root = (uint8_t)((P.root + 12 + d % 12) % 12); chord_set(&H.now); dirty(); }
static void gc_scale(int32_t d) { ic(&P.scale, d, 1, NSCALE - 1); chord_set(&H.now); }
static void gc_quant(int32_t d) { ic(&P.quant, d, 0, NCQ - 1); }
static void gc_clk(int32_t d) { T.clk_mode = (uint8_t)clamp(T.clk_mode + d, 0, NCK - 1); P.clk_mode = T.clk_mode; dirty(); }
static void gc_clko(int32_t d) { T.clk_send = d > 0; P.clk_send = T.clk_send; dirty(); }
static void gc_et(int32_t d) { ic(&P.echo_time, d, 0, 127); project_apply(); }
static void gc_ef(int32_t d) { ic(&P.echo_fb, d, 0, 127); project_apply(); }
static void gc_slot(int32_t d) { set.slot = (uint8_t)clamp(set.slot + d, 0, NSLOTS - 1); settings_save(); }
static void gc_ble(int32_t d) { set.ble_off = d < 0; plat_ble_enable(!set.ble_off); settings_save(); }
static void ga_save(void)
{
    if (T.playing) {
        U.save_wait = 1;
        app_say("SAVES", "AT THE STOP");
    } else {
        project_save_now();
    }
}
static void ga_load(void)
{
    if (T.playing)
        transport_wait_stop();
    if (slot_load(set.slot, PROJ_MAGIC, &P, sizeof P)) {
        project_apply();
        U.dirty = 0;
        app_say("LOADED", "");
    } else {
        app_say("SLOT", "EMPTY");
    }
}
static void ga_new(void)
{
    if (T.playing)
        transport_wait_stop();
    project_new();
    project_apply();
    dirty();
    app_say("NEW", "PROJECT");
}
static const row_def_t SETUP_ROWS[] = {
    {"TEMPO", gv_tempo, gc_tempo, 0}, {"KEY", gv_root, gc_root, 0},     {"SCALE", gv_scale, gc_scale, 0},
    {"CHORDS CHANGE", gv_quant, gc_quant, 0}, {"CLOCK", gv_clk, gc_clk, 0}, {"CLOCK OUT", gv_clko, gc_clko, 0},
    {"ECHO TIME", gv_et, gc_et, 0},   {"ECHO FEEDBACK", gv_ef, gc_ef, 0}, {"SLOT", gv_slot, gc_slot, 0},
    {"SAVE TO SLOT", 0, 0, ga_save},  {"LOAD SLOT", 0, 0, ga_load},     {"NEW PROJECT", 0, 0, ga_new},
    {"BLUETOOTH", gv_ble, gc_ble, 0},
};
#define NSETUP (sizeof SETUP_ROWS / sizeof SETUP_ROWS[0])

/* ---- each part's four knobs ---- */
static const char *const KLAB[NSOUND][4] = {{"STYLE", "OCTAVE", "SPREAD", "LEVEL"}, {"FIGURE", "OCTAVE", "GATE", "LEVEL"},
                                            {"FIGURE", "RATE", "RANGE", "OCTAVE"}, {"FIGURE", "RATE", "RANGE", "OCTAVE"},
                                            {"GROOVE", "FILL", "SWING", "LEVEL"}};
static const char *const PAD_STYLE[4] = {"HOLD", "QUARTERS", "EIGHTHS", "ON CHANGE"};
static void kval(uint32_t k, char *o)
{
    const part_t *p = &P.part[U.part];
    o[0] = 0;
    switch (U.part * 4u + k) {
    case 0: s_cpy(o, PAD_STYLE[p->style % 4u]); break;
    case 4: s_cpy(o, BASS_NM[p->style % NBASS]); break;
    case 8: case 12: s_cpy(o, MOTIF_NM[p->style > NMOTIF ? NMOTIF : p->style]); break;
    case 16: s_cpy(o, BEAT_NM[p->style % NBEAT]); break;
    case 1: case 5: case 11: case 15: s_cpy(o, "OCT "); sgn_cat(o, p->octave); break;
    case 2: s_cpy(o, "SPREAD "); i_cat(o, p->amount); break;
    case 6: s_cpy(o, "GATE "); i_cat(o, p->amount); s_cat(o, "%"); break;
    case 9: case 13: s_cpy(o, RATE_NM[p->rate % NRATE]); break;
    case 10: case 14: i_cat(o, p->amount); s_cat(o, " OCT"); break;
    case 17: s_cpy(o, p->amount ? "FILLS" : "NO FILLS"); break;
    case 18: s_cpy(o, "SWING "); i_cat(o, P.swing); break;
    default: s_cpy(o, "LEVEL "); i_cat(o, P.ins[U.part].level); break;
    }
}
static void knob(uint32_t k, int32_t d)
{
    part_t *p = &P.part[U.part];
    switch (U.part * 4u + k) {
    case 0: p->style = (uint8_t)clamp(p->style + d, 0, 3); break;
    case 4: p->style = (uint8_t)clamp(p->style + d, 0, NBASS - 1); break;
    case 8: case 12: p->style = (uint8_t)clamp(p->style + d, 0, NMOTIF); break;
    case 16: p->style = (uint8_t)clamp(p->style + d, 0, NBEAT - 1); break;
    case 1: case 5: case 11: case 15: p->octave = (int8_t)clamp(p->octave + d, -2, 2); break;
    case 2: p->amount = (uint8_t)clamp(p->amount + d * 4, 0, 127); H.pad_n = 0; pad_voice(); break;
    case 6: p->amount = (uint8_t)clamp(p->amount + d * 5, 10, 100); break;
    case 9: case 13: p->rate = (uint8_t)clamp(p->rate + d, 0, NRATE - 1); break;
    case 10: case 14: p->amount = (uint8_t)clamp(p->amount + d, 1, 3); break;
    case 17: p->amount = d > 0; break;
    case 18: P.swing = (uint8_t)clamp(P.swing + d, 0, 25); break;
    default: P.ins[U.part].level = (uint8_t)clamp(P.ins[U.part].level + d * 3, 0, 127); break;
    }
    dirty();
}

static void input(void)
{
    uint32_t k;
    kit_input();
    if (midi_chord_req) {                               /* a note in: its chord */
        midi_chord_req = 0;
        play_chord(midi_chord_deg, 0);
    }
    if (DOWN(B_PLAY)) {
        if (T.playing) {
            T.want = 2;
        } else {
            live_pad_off();
            T.want = 1;
        }
    }
    if (DOWN(B_REC)) {
        if (!H.rec) {
            H.rec = 1;
            H.follow = 0;
            P.prog_n = 0;
            for (k = 0; k < NSLOT; k++)
                P.prog[k].deg = 0xFFu;
            H.rec_start = T.tick;
            app_say("REC", "PLAY THE CHORDS");
        } else {
            rec_close();
            H.rec = 0;
            H.follow = P.prog_n > 0u;
            app_say("PROGRESSION", "RECORDED");
        }
        dirty();
    }
    if (DOWN(B_SEQ)) {
        H.follow = !H.follow;
        H.slot_t = 0;
        if (H.follow && P.prog_n)
            slot_enter(0);
        app_say("PROGRESSION", H.follow ? "PLAYS" : "OFF");
    }
    if (DOWN(B_HOME)) U.page = PG_COND;
    if (DOWN(B_EDIT)) U.page = U.page == PG_PROG ? PG_COND : PG_PROG;
    if (TAPPED(B_GLO)) U.page = U.page == PG_SETUP ? PG_COND : PG_SETUP;
    if (DOWN(B_FX)) {
        if (U.page == PG_PROG) {
            P.prog[U.pslot].deg = 0xFFu;
            while (P.prog_n && P.prog[P.prog_n - 1u].deg == 0xFFu)
                P.prog_n--;
            dirty();
        } else {
            play_chord(0xFFu, 0);
        }
    }
    if (DOWN(B_ARP) && (U.part == PA_MOTA || U.part == PA_MOTB)) {
        dice_motif(U.part - PA_MOTA);
        dirty();
        app_say("DICE", PART_NM[U.part]);
    }
    if (DOWN(B_LFO)) {
        P.part[U.part].muted ^= 1u;
        dirty();
    }
    if (TAPPED(B_SAVE))
        ga_save();
    if (DOWN(B_OCTDN) || DOWN(B_OCTUP))
        knob(U.part == PA_MOTA || U.part == PA_MOTB ? 3u : 1u, DOWN(B_OCTUP) ? 1 : -1);
    if (K.enc[EN_ALGO])
        U.part = (uint8_t)clamp(U.part + K.enc[EN_ALGO], 0, NSOUND - 1);
    if (K.enc[EN_PRESET]) {
        if (HELD(B_SEL)) {
            kit_use(B_SEL);
            gc_scale(K.enc[EN_PRESET]);
        } else {
            gc_root(K.enc[EN_PRESET]);
        }
        app_say(ROOT_NM[P.root % 12u], SCALE_NM[P.scale % NSCALE]);
    }
    if (U.page == PG_SETUP) {
        kit_list_input(SETUP_ROWS, NSETUP, &U.grow);
    } else if (U.page == PG_PROG) {
        U.pslot = (uint8_t)clamp(U.pslot + K.enc[EN_SELECT], 0, NSLOT - 1);
        if (K.enc[EN_K1] && P.prog[U.pslot].deg != 0xFFu) {
            P.prog[U.pslot].beats = (uint8_t)clamp(P.prog[U.pslot].beats + K.enc[EN_K1], 1, 16);
            dirty();
        }
    } else {
        if (K.enc[EN_SELECT])
            gc_tempo(K.enc[EN_SELECT]);
        for (k = 0; k < 4u; k++)
            if (K.enc[EN_K1 + k])
                knob(k, K.enc[EN_K1 + k]);
    }
    for (k = 0; k < NKEYS; k++) {
        int w = white_of(k), b = black_of(k);
        if (!(K.kdown >> k & 1u))
            continue;
        if (w >= 0 && w < 14) {
            U.held_white = (int8_t)w;
            play_chord((uint32_t)w, mods_held());
        } else if (w == 14) {
            play_chord(0xFFu, 0);
        } else if (w == 15) {
            tap_tempo();
        } else if (b >= 0 && b < (int)NMOD) {
            if (U.page == PG_PROG && P.prog[U.pslot].deg != 0xFFu && U.held_white < 0) {
                P.prog[U.pslot].mods ^= (uint8_t)(1u << b);   /* the slot's colour, on and off */
                dirty();
            } else if (U.held_white >= 0) {             /* a colour added to the chord held */
                play_chord((uint32_t)U.held_white, mods_held());
            }
        } else if (b >= (int)NMOD) {
            P.part[b - (int)NMOD].muted ^= 1u;
            dirty();
        }
    }
    if (U.held_white >= 0 && !(K.keys >> WKEY[U.held_white] & 1u))
        U.held_white = -1;
    kit_cards(CARD_TEXT);
}

/* ---- the screen ---- */
static void mini_keys(int32_t x, int32_t y, int32_t w, const int8_t *notes, uint32_t n, uint16_t c)
{
    /* two octaves from C3 as 24 slots; each chord note lit at its pitch class position across them */
    uint32_t i, k;
    for (k = 0; k < 24u; k++) {
        static const uint8_t BLK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
        int on = 0;
        for (i = 0; i < n; i++)
            if (((int32_t)notes[i] - 48 + 240) % 24 == (int32_t)k)
                on = 1;
        cv_rect(x + (int32_t)k * w / 24, y + (BLK[k % 12u] ? 0 : 6), w / 24 - 1, 10, on ? c : BLK[k % 12u] ? K_LINE : K_FAINT);
    }
}

static void draw_conductor(void)
{
    char name[24], roman[8], t[32];
    uint32_t i;
    chord_name(&H.now, name, roman);
    if (H.now.deg == 0xFFu || !H.n) {
        s_cpy(name, "N.C.");
        roman[0] = 0;
    }
    cv_text(8, 0, &FONT_L, name, K_FG);
    text_r(232, 6, &FONT_M, roman, ACCENT);
    t[0] = 0;
    s_cat(t, ROOT_NM[P.root % 12u]);
    s_cat(t, " ");
    s_cat(t, SCALE_NM[P.scale % NSCALE]);
    if (H.has_cue) {
        char cn[24], cr[8];
        chord_name(&H.cue, cn, cr);
        s_cat(t, "   NEXT ");
        s_cat(t, cn[0] ? cn : "N.C.");
    }
    cv_text(8, 34, &FONT_T, t, H.has_cue ? K_REC : K_DIM);
    mini_keys(8, 48, 224, H.pad, H.pad_n, ACCENT);
    /* the parts */
    for (i = 0; i < NSOUND; i++) {
        int32_t y = 68 + (int32_t)i * 18;
        const part_t *p = &P.part[i];
        uint16_t c = p->muted ? K_FAINT : PART_C[i];
        if (i == U.part)
            cv_rect(0, y - 1, 240, 17, K_PANEL);
        cv_rect(2, y + 2, 3, 11, c);
        cv_text(10, y, &FONT_XS, PART_NM[i], c);
        s_cpy(t, i == PA_PAD ? PAD_STYLE[p->style % 4u] : i == PA_BASS ? BASS_NM[p->style % NBASS] :
                 i == PA_BEAT ? BEAT_NM[p->style % NBEAT] : MOTIF_NM[p->style > NMOTIF ? NMOTIF : p->style]);
        cv_text(80, y + 1, &FONT_T, p->muted ? "MUTED" : t, K_DIM);
        if (T.playing && heard[i] && !p->muted && H.sounding[i] >= 0) {
            if (i == PA_BEAT)
                s_cpy(t, "*");
            else
                note_name(H.sounding[i], t);
            text_r(232, y, &FONT_XS, t, c);
        }
    }
    /* the progression */
    for (i = 0; i < NSLOT; i++) {
        int32_t x = 4 + (int32_t)i * 15, y = 160;
        int on = i < P.prog_n && P.prog[i].deg != 0xFFu, now = H.follow && i == H.slot && T.playing;
        cv_rect(x, y, 13, 22, now ? ACCENT : on ? K_PANEL : K_BG);
        box(x, y, 13, 22, on ? K_LINE : K_PANEL);
        if (on) {
            char n[4];
            n[0] = (char)('1' + P.prog[i].deg % 7u);
            n[1] = 0;
            text_c(x + 7, y + 3, &FONT_T, n, now ? K_BG : K_FG);
        }
    }
    if (H.rec) {
        cv_text(4, 184, &FONT_T, "RECORDING THE PROGRESSION", K_REC);
    } else {                                            /* the knobs' names for the part shown */
        char kn[48] = "";
        for (i = 0; i < 4u; i++) {
            s_cat(kn, i ? "   " : "");
            s_cat(kn, KLAB[U.part][i]);
        }
        cv_text(4, 184, &FONT_T, kn, K_DIM);
        text_r(236, 184, &FONT_T, H.follow ? "SEQ: PROGRESSION" : "", K_FAINT);
    }
}

static void draw_prog(void)
{
    uint32_t i;
    char name[24], roman[8], t[16];
    cv_text(6, 0, &FONT_T, "PROGRESSION: SELECT A SLOT, A WHITE KEY SETS IT", ACCENT);
    for (i = 0; i < NSLOT; i++) {
        int32_t x = 4 + (int32_t)(i % 4u) * 59, y = 16 + (int32_t)(i / 4u) * 42;
        const chord_t *c = &P.prog[i];
        int on = c->deg != 0xFFu && i < P.prog_n, now = H.follow && T.playing && i == H.slot;
        cv_rect(x, y, 56, 38, i == U.pslot ? K_PANEL : K_BG);
        box(x, y, 56, 38, i == U.pslot ? ACCENT : now ? K_FG : K_LINE);
        if (on) {
            chord_name(c, name, roman);
            cv_text(x + 4, y + 1, &FONT_XS, name, now ? ACCENT : K_FG);
            s_cpy(t, roman);
            s_cat(t, "  ");
            i_cat(t, c->beats);
            s_cat(t, " BT");
            cv_text(x + 4, y + 19, &FONT_T, t, K_DIM);
        } else {
            t[0] = 0;
            i_cat(t, (int32_t)i + 1);
            cv_text(x + 4, y + 1, &FONT_T, t, K_FAINT);
        }
    }
    cv_text(6, 186, &FONT_T, "K1 BEATS  BLACK 1-7 COLOURS  FX CLEARS", K_FAINT);
}

static void draw(void)
{
    static const char *const PGN[] = {"CONDUCTOR", "PROGRESSION", "SETUP"};
    kit_header("REGENT-4", PGN[U.page], ACCENT, T.playing, H.rec, T.bpm10);
    cv_begin(240, 200, K_BG);
    if (K.card < NB && CARD_TEXT[K.card])
        kit_card(K.card, CARD_TEXT[K.card], 200, ACCENT);
    else if (U.page == PG_SETUP) {
        cv_text(6, 0, &FONT_T, "SETUP", ACCENT);
        kit_list_draw(SETUP_ROWS, NSETUP, U.grow, 14, 184, ACCENT);
    } else if (U.page == PG_PROG)
        draw_prog();
    else
        draw_conductor();
    cv_blit(0, 22);
    if (U.page == PG_COND) {
        char v[4][16], a[4][32];
        uint32_t k;
        for (k = 0; k < 4u; k++) {
            kval(k, v[k]);
            s_cpy(a[k], v[k]);
        }
        kit_knobs(a[0], a[1], a[2], a[3], PART_C[U.part]);
    } else if (U.page == PG_PROG) {
        kit_knobs("BEATS", "", "", "", ACCENT);
    } else {
        kit_knobs("VALUE", "", "", "", ACCENT);
    }
    if (U.held_white >= 0 && !kit_hint(HINTS, ACCENT)) {   /* a chord key held: what the black keys add */
        cv_begin(240, 18, ACCENT);
        cv_text(4, 2, &FONT_T, "BLACK: 7  SUS2  SUS4  ADD9  6  FLIP  FLAT5   8-11: MUTES", K_BG);
        cv_blit(0, 222);
    } else {
        kit_hint(HINTS, ACCENT);
    }
}

static void lights(void)
{
    uint32_t btn = 0, keys = K.keys, dkeys = 0, i;
    if (T.playing)
        btn |= 1u << B_PLAY | LED_PLAY_GREEN;
    if (H.rec)
        btn |= 1u << B_REC;
    if (H.follow)
        btn |= 1u << B_SEQ;
    if (U.dirty)
        btn |= 1u << B_SAVE;
    if (U.page == PG_PROG)
        btn |= 1u << B_EDIT;
    if (P.part[U.part].muted)
        btn |= 1u << B_LFO;
    if (H.now.deg != 0xFFu && H.now.deg < 14u)          /* the chord's key lit, its colours on the black keys */
        keys |= 1u << WKEY[H.now.deg];
    for (i = 0; i < NMOD; i++)
        if (H.now.mods >> i & 1u)
            dkeys |= 1u << BKEY[i];
    for (i = 0; i < NPART; i++)
        if (!P.part[i].muted)
            dkeys |= 1u << BKEY[NMOD + i];
    plat_lights(btn, keys, 0, dkeys, 1);
}
