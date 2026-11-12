/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 David Ryan */
/* REGENT-4: the harmony conducts. You play chords, not notes: a white key is a degree of the key (I to VII, and again
 * an octave up), the black keys held with it colour it (7th, sus2, sus4, add9, 6th, major/minor flipped, flat five).
 * Four parts follow the chord and never leave it, as on the NDLR and in Logic's Chord Track:
 *   PAD     the chord, voiced close to the last one (voice leading), held or pulsed
 *   BASS    the root, in one of eight figures
 *   MOTIF A and MOTIF B  arpeggio figures over the chord's notes, each at its own rate, so they cross
 * and a BEAT under them. A PROGRESSION of up to 16 chords (each a number of beats) is recorded by playing it with REC
 * and plays back with SEQ. A chord played changes at once, on the beat or at the bar (QUANTISE). */

#define NPART 4u
#define NSLOT 16u
enum { PA_PAD, PA_BASS, PA_MOTA, PA_MOTB, PA_BEAT, NSOUND };
static const char *const PART_NM[NSOUND] = {"PAD", "BASS", "MOTIF A", "MOTIF B", "BEAT"};
enum { MOD_7TH, MOD_SUS2, MOD_SUS4, MOD_ADD9, MOD_6TH, MOD_FLIP, MOD_FLAT5, NMOD };
static const char *const MOD_NM[NMOD] = {"7", "SUS2", "SUS4", "ADD9", "6", "FLIP", "b5"};
enum { CQ_NOW, CQ_BEAT, CQ_BAR, NCQ };
static const char *const CQ_NM[NCQ] = {"AT ONCE", "ON THE BEAT", "AT THE BAR"};

/* bass figures over 16 sixteenths: R root, 3 third, 5 fifth, O octave, - nothing (the last note holds) */
static const char *const BASS_FIG[] = {"R---------------", "R---R---R---R---", "R-R-R-R-R-R-R-R-", "R-O-R-O-R-O-R-O-",
                                       "R---5---R---5---", "R--R--R---R--R--", "R---3---5---O---", "-O-O-O-O-O-O-O-O"};
static const char *const BASS_NM[] = {"HOLD", "QUARTERS", "EIGHTHS", "OCTAVES", "ROOT-FIFTH", "SYNCOPATED", "WALK", "OFFBEAT"};
#define NBASS 8u
/* motifs: chord-tone indices (the chord climbs by octaves past its top), -1 a rest */
static const int8_t MOTIFS[][8] = {
    {0, 1, 2, 3, 99}, {3, 2, 1, 0, 99}, {0, 1, 2, 3, 2, 1, 99}, {0, 2, 1, 3, 99}, {0, 2, 1, 2, 99}, {0, 3, 0, 2, 0, 1, 99},
    {0, 1, 2, 3, 4, 5, 6, 7}, {0, 1, 0, 2, 0, 3, 99}, {2, 3, 99}, {0, -1, 2, -1, 1, 3, -1, 99}, {4, 2, 3, 1, 2, 0, 99},
    {0, 99}};
static const char *const MOTIF_NM[] = {"UP", "DOWN", "UP-DOWN", "BROKEN", "ALBERTI", "PEDAL", "RISE", "STEPS",
                                       "TRILL", "SPARSE", "FALL", "ROOT", "DICE"};
#define NMOTIF 12u                                       /* (and 12: DICE, the part's own) */
static const uint8_t RATE_T[] = {96, 48, 32, 24, 16, 12};
static const char *const RATE_NM[] = {"1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32"};
#define NRATE 6u
/* beats: kick, snare, hat over 16 sixteenths */
static const char *const BEATS[][3] = {
    {"x---x---x---x---", "----x-------x---", "--x---x---x---x-"}, {"x-------x-x-----", "----x-------x---", "x-x-x-x-x-x-x-x-"},
    {"x---------x-----", "--------x-------", "x-x-x-x-x-x-x-x-"}, {"x-x-------x--x--", "----x--x-x--x--x", "x-xxx-x-x-xxx-x-"},
    {"x--x--x---x--x--", "----x-------x---", "xxxxxxxxxxxxxxxx"}, {"----------------", "----------------", "----------------"}};
static const char *const BEAT_NM[] = {"FOUR", "BACKBEAT", "HALF-TIME", "BREAK", "TRESILLO", "NONE"};
#define NBEAT 6u

typedef struct {
    uint8_t deg;                                        /* 0..13 (7..13 an octave up), 0xFF none */
    uint8_t mods;                                       /* bit per MOD_* */
    uint8_t beats;                                      /* how long it lasts in the progression */
    uint8_t rsv;
} chord_t;

typedef struct {
    uint8_t style;                                      /* PAD: 0 hold, 1 quarters, 2 eighths, 3 strum; BASS: figure;
                                                         * MOTIF: pattern; BEAT: groove */
    uint8_t rate;                                       /* MOTIF: RATE_T index */
    int8_t octave;
    uint8_t amount;                                     /* PAD spread / BASS gate / MOTIF range / BEAT fill */
    uint8_t muted, chance;
    int8_t dice[8];                                     /* MOTIF's DICE pattern */
} part_t;

#define PROJ_MAGIC 0x34474552u                          /* "REG4" */
#define PROJ_VER 1u
typedef struct {
    uint16_t bpm10;
    uint8_t root, scale, quant, clk_mode, clk_send, echo_time, echo_fb, swing;
    part_t part[NSOUND];
    patch_t ins[NSOUND];
    chord_t prog[NSLOT];
    uint8_t prog_n;
    uint8_t rsv[23];
} proj_t;
static proj_t P;

static struct {
    chord_t now, cue;                                   /* the chord sounding; one waiting for its beat or bar */
    uint8_t has_cue;
    int8_t notes[6];                                    /* the chord's notes, low to high (from the key's octave) */
    uint8_t n;
    int8_t pad[6];                                      /* the pad's voicing (voice-led) */
    uint8_t pad_n;
    uint8_t changed;                                    /* the chord changed: the pad strikes */
    uint8_t follow;                                     /* SEQ: the progression plays */
    uint8_t rec;                                        /* REC: chords played are written */
    uint8_t slot;                                       /* the progression's slot playing */
    uint32_t slot_t;                                    /* ticks into it */
    uint32_t rec_start;                                 /* REC: the tick the open chord began */
    uint8_t mot_i[2];
    int16_t sounding[NSOUND];
    uint32_t seed;
} H;

static int32_t sdeg(int32_t d) { return scale_deg(d, P.root, P.scale, 48); }

/* the chord's notes from its degree and colours, into out (6 at most); their number */
static uint32_t chord_notes(const chord_t *c, int8_t *out)
{
    int32_t d = c->deg % 7u, up = c->deg >= 7u ? 12 : 0, r, t3, t5;
    uint32_t m = c->mods, n = 0;
    if (c->deg == 0xFFu)
        return 0;
    r = sdeg(d) + up;
    t3 = sdeg(d + 2) + up;
    t5 = sdeg(d + 4) + up;
    if (m >> MOD_FLIP & 1u)                             /* major <-> minor */
        t3 = (t3 - r) % 12 == 4 ? t3 - 1 : t3 + 1;
    if (m >> MOD_SUS2 & 1u)
        t3 = sdeg(d + 1) + up;
    if (m >> MOD_SUS4 & 1u)
        t3 = sdeg(d + 3) + up;
    if (m >> MOD_FLAT5 & 1u)
        t5 = r + 6;
    out[n++] = (int8_t)r;
    out[n++] = (int8_t)t3;
    out[n++] = (int8_t)t5;
    if (m >> MOD_6TH & 1u)
        out[n++] = (int8_t)(sdeg(d + 5) + up);
    if (m >> MOD_7TH & 1u)
        out[n++] = (int8_t)(sdeg(d + 6) + up);
    if (m >> MOD_ADD9 & 1u)
        out[n++] = (int8_t)(sdeg(d + 8) + up);
    return n;
}
static void chord_build(const chord_t *c) { H.n = (uint8_t)chord_notes(c, H.notes); }

/* the pad: each chord note at the octave nearest the last voicing's middle (voice leading) */
static void pad_voice(void)
{
    uint32_t i, k;
    int32_t centre = 64 + 12 * P.part[PA_PAD].octave, sum = 0;
    for (i = 0; i < H.pad_n; i++)
        sum += H.pad[i];
    if (H.pad_n)
        centre = sum / (int32_t)H.pad_n;
    for (i = 0; i < H.n; i++) {
        int32_t n = H.notes[i];
        while (n < centre - 6)
            n += 12;
        while (n > centre + 6)
            n -= 12;
        if (P.part[PA_PAD].amount > 64u && (i & 1u))   /* spread: every other note an octave up */
            n += 12;
        H.pad[i] = (int8_t)n;
    }
    H.pad_n = H.n;
    for (i = 1; i < H.pad_n; i++)                       /* low to high */
        for (k = i; k > 0u && H.pad[k] < H.pad[k - 1u]; k--) {
            int8_t t = H.pad[k];
            H.pad[k] = H.pad[k - 1u];
            H.pad[k - 1u] = t;
        }
    if (H.pad_n && (H.pad[0] < 40 + 12 * P.part[PA_PAD].octave || H.pad[H.pad_n - 1u] > 88 + 12 * P.part[PA_PAD].octave)) {
        H.pad_n = 0;                                    /* drifted too far: start again from the middle */
        pad_voice();
    }
}

static int32_t chord_tone(int32_t i)                   /* index past the top climbs by octaves */
{
    if (!H.n)
        return -1;
    return H.notes[(uint32_t)i % H.n] + 12 * (i / (int32_t)H.n);
}

static void chord_set(const chord_t *c)
{
    H.now = *c;
    chord_build(c);
    pad_voice();
    H.changed = 1;
}

static void part_note(uint32_t p, int32_t note, uint32_t vel, uint32_t len_ticks)
{
    if (P.part[p].muted || note < 0)
        return;
    if (P.part[p].chance < 100u && xs32(&H.seed) % 100u >= P.part[p].chance)
        return;
    play_note(p, &P.ins[p], note, vel, ticks_to_samples(len_ticks), p == PA_BASS);
    H.sounding[p] = (int16_t)note;
}

static void pad_strike(uint32_t len)
{
    uint32_t i;
    if (P.part[PA_PAD].muted)
        return;
    offs_owner(PA_PAD);
    for (i = 0; i < H.pad_n; i++)
        play_note(PA_PAD, &P.ins[PA_PAD], H.pad[i], 80, ticks_to_samples(len), 0);
    H.sounding[PA_PAD] = H.pad_n ? H.pad[0] : -1;
}

static void sixteenth(uint32_t s16)                     /* every part on a sixteenth (s16: 0..15 in the bar) */
{
    part_t *pp = &P.part[PA_PAD], *pb = &P.part[PA_BASS];
    uint32_t m;
    /* PAD */
    if (H.n) {
        if (H.changed || (pp->style == 1u && s16 % 4u == 0u) || (pp->style == 2u && s16 % 2u == 0u) ||
            (pp->style == 0u && s16 == 0u)) {
            uint32_t len = pp->style == 1u ? 90u : pp->style == 2u ? 42u : 380u;
            pad_strike(len);
        }
        H.changed = 0;
    }
    /* BASS */
    if (H.n) {
        char c = BASS_FIG[pb->style % NBASS][s16];
        int32_t n = -1, root = H.notes[0] - 12 + 12 * pb->octave;
        while (root > 52)
            root -= 12;
        while (root < 28)
            root += 12;
        if (c == 'R') n = root;
        else if (c == 'O') n = root + 12;
        else if (c == '5') n = root + (H.notes[2] - H.notes[0]);
        else if (c == '3') n = root + (H.notes[1] - H.notes[0]);
        if (n >= 0) {
            uint32_t len = 1u, k;
            for (k = s16 + 1u; k < 16u && BASS_FIG[pb->style % NBASS][k] == '-'; k++)
                len++;
            part_note(PA_BASS, n, s16 % 4u ? 92 : 112, len * 24u * (uint32_t)clamp(pb->amount, 10, 100) / 100u);
        }
    }
    /* BEAT */
    {
        const part_t *b = &P.part[PA_BEAT];
        static const uint8_t DRUM[3] = {36, 38, 42};
        for (m = 0; m < 3u; m++)
            if (BEATS[b->style % NBEAT][m][s16] == 'x')
                part_note(PA_BEAT, DRUM[m], m == 2u ? (s16 % 4u == 2u ? 96 : 64) : 112, 12);
        if (b->amount && H.has_cue && s16 >= 12u && (s16 & 1u) == 0u)   /* a fill before a chord change at the bar */
            part_note(PA_BEAT, 38, 70 + s16 * 2u, 12);
    }
}

static void motif_tick(uint32_t which)
{
    uint32_t p = PA_MOTA + which;
    part_t *pm = &P.part[p];
    const int8_t *pat = pm->style >= NMOTIF ? pm->dice : MOTIFS[pm->style];
    uint32_t len = 0, range = pm->amount ? pm->amount : 1u;
    int32_t i, n;
    if (T.tick % RATE_T[pm->rate % NRATE] != 0u || !H.n)
        return;
    while (len < 8u && pat[len] != 99)
        len++;
    if (!len)
        return;
    i = pat[H.mot_i[which] % len];
    H.mot_i[which]++;
    if (i < 0)
        return;
    i %= (int32_t)(H.n * range);
    n = chord_tone(i) + 12 * pm->octave + 12;
    part_note(p, n, 70 + (H.mot_i[which] % 4u == 1u ? 30 : 0), RATE_T[pm->rate % NRATE] * 3u / 4u);
}

/* the progression */
static void slot_enter(uint32_t s)
{
    H.slot = (uint8_t)s;
    H.slot_t = 0;
    if (P.prog[s].deg != 0xFFu)
        chord_set(&P.prog[s]);
}

static uint32_t quant_ticks(void) { return P.quant == CQ_BAR ? 384u : P.quant == CQ_BEAT ? 96u : 1u; }

/* a chord played (the main loop calls this through the cue; the render applies it) */
static void rec_close(void)                             /* REC: the open chord's beats */
{
    if (P.prog_n && H.rec) {
        uint32_t beats = (T.tick - H.rec_start + 48u) / 96u;
        P.prog[P.prog_n - 1u].beats = (uint8_t)clamp((int32_t)beats, 1, 16);
    }
}
static void chord_apply(const chord_t *c)
{
    if (H.rec && T.playing) {
        rec_close();
        if (P.prog_n < NSLOT) {
            P.prog[P.prog_n] = *c;
            P.prog[P.prog_n].beats = 4;
            P.prog_n++;
        }
        H.rec_start = T.tick;
    }
    chord_set(c);
}

static void seq_tick(void)
{
    if (H.has_cue && T.tick % quant_ticks() == 0u) {
        H.has_cue = 0;
        chord_apply(&H.cue);
    }
    if (H.follow && P.prog_n && !H.rec) {               /* the progression */
        if (H.slot_t == 0u && T.tick == 0u)
            slot_enter(0);
        if (H.slot_t >= (uint32_t)P.prog[H.slot % P.prog_n].beats * 96u)
            slot_enter((H.slot + 1u) % P.prog_n);
        H.slot_t++;
    }
    {                                                   /* sixteenths, the odd ones late by the swing */
        uint32_t ph = T.tick % 48u, s16 = (T.tick / 24u) % 16u;
        if (ph == 0u || ph == 24u + (uint32_t)clamp(P.swing, 0, 23))
            sixteenth(ph == 0u ? s16 : s16 | 1u);
    }
    motif_tick(0);
    motif_tick(1);
}

static void seq_started(void)
{
    H.slot = 0;
    H.slot_t = 0;
    H.mot_i[0] = H.mot_i[1] = 0;
    H.changed = 1;
    if (H.rec)
        H.rec_start = 0;
}
static void seq_stopped(void)
{
    uint32_t i;
    rec_close();
    for (i = 0; i < NSOUND; i++)
        H.sounding[i] = -1;
}

/* MIDI in: a note played chooses the chord whose root it is nearest (a keyboard can conduct) */
static volatile uint8_t midi_chord_req, midi_chord_deg;
static void seq_midi_note(int32_t note, uint32_t vel, uint32_t chan)
{
    uint32_t d, best = 0;
    int32_t bd = 99;
    (void)chan;
    if (!vel)
        return;
    for (d = 0; d < 7u; d++) {
        int32_t x = ((note - sdeg((int32_t)d)) % 12 + 12) % 12;
        if (x < bd) {
            bd = x;
            best = d;
        }
    }
    midi_chord_deg = (uint8_t)best;
    midi_chord_req = 1;
}

static void dice_motif(uint32_t which)
{
    uint32_t i;
    part_t *pm = &P.part[PA_MOTA + which];
    for (i = 0; i < 8u; i++) {
        uint32_t r = xs32(&H.seed);
        pm->dice[i] = (int8_t)(r % 5u == 0u ? -1 : (int32_t)(r >> 8) % 6);
    }
    pm->style = NMOTIF;
}

static void project_new(void)
{
    uint32_t i;
    uint8_t *b = (uint8_t *)&P;
    static const uint8_t PROG[4] = {0, 5, 3, 4};        /* I vi IV V */
    for (i = 0; i < sizeof P; i++)
        b[i] = 0;
    P.bpm10 = 1100;
    P.root = 0;
    P.scale = SC_MAJOR;
    P.quant = CQ_BEAT;
    P.clk_mode = CK_INTERNAL;
    P.clk_send = 1;
    P.echo_time = 85;
    P.echo_fb = 60;
    for (i = 0; i < NSOUND; i++) {
        patch_default(&P.ins[i], PT_SYNTH, i + 1u);
        P.part[i].chance = 100;
        P.part[i].amount = 60;
        P.part[i].rate = 3;
    }
    P.ins[PA_PAD].wave = WV_SUPER;
    P.ins[PA_PAD].attack = 50;
    P.ins[PA_PAD].release = 70;
    P.ins[PA_PAD].sustain = 100;
    P.ins[PA_PAD].cutoff = 55;
    P.ins[PA_PAD].level = 55;
    P.ins[PA_PAD].echo = 50;
    P.part[PA_PAD].style = 0;
    P.part[PA_PAD].amount = 30;
    P.ins[PA_BASS].cutoff = 40;
    P.ins[PA_BASS].reso = 70;
    P.ins[PA_BASS].envmod = 60;
    P.ins[PA_BASS].decay = 50;
    P.ins[PA_BASS].sustain = 50;
    P.part[PA_BASS].style = 4;
    P.part[PA_BASS].amount = 80;
    P.ins[PA_MOTA].wave = WV_SQR;
    P.ins[PA_MOTA].decay = 40;
    P.ins[PA_MOTA].sustain = 10;
    P.ins[PA_MOTA].echo = 70;
    P.ins[PA_MOTA].level = 70;
    P.part[PA_MOTA].style = 3;
    P.part[PA_MOTA].rate = 3;
    P.part[PA_MOTA].amount = 2;
    P.ins[PA_MOTB].wave = WV_TRI;
    P.ins[PA_MOTB].decay = 55;
    P.ins[PA_MOTB].sustain = 0;
    P.ins[PA_MOTB].echo = 90;
    P.part[PA_MOTB].style = 5;
    P.part[PA_MOTB].rate = 2;
    P.part[PA_MOTB].octave = 1;
    P.part[PA_MOTB].amount = 1;
    P.part[PA_MOTB].muted = 1;
    patch_default(&P.ins[PA_BEAT], PT_DRUM, 10);
    P.part[PA_BEAT].style = 1;
    P.part[PA_BEAT].amount = 1;
    for (i = 0; i < NSLOT; i++)
        P.prog[i].deg = 0xFFu;
    for (i = 0; i < 4u; i++) {
        P.prog[i].deg = PROG[i];
        P.prog[i].beats = 4;
    }
    P.prog[1].mods = 1u << MOD_7TH;
    P.prog_n = 4;
}

static void project_apply(void)
{
    T.bpm10 = P.bpm10;
    T.clk_mode = P.clk_mode;
    T.clk_send = P.clk_send;
    echo_time = 0.05f + (float)P.echo_time * (0.32f / 127.0f);
    echo_fb = (float)P.echo_fb * (0.85f / 127.0f);
}

/* the chord's name: C, Am7, Fsus4, G7 ... and its numeral (ii, V, viio) */
static void chord_name(const chord_t *c, char *o, char *roman)
{
    static const char *const RN[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
    static const char *const rn[7] = {"i", "ii", "iii", "iv", "v", "vi", "vii"};
    int8_t nt[6];
    uint32_t n = chord_notes(c, nt);
    int32_t r, t3, t5;
    o[0] = roman[0] = 0;
    if (!n)
        return;
    r = nt[0];
    t3 = nt[1] - r;
    t5 = nt[2] - r;
    s_cpy(o, ROOT_NM[r % 12]);
    if (c->mods >> MOD_SUS2 & 1u) s_cat(o, "sus2");
    else if (c->mods >> MOD_SUS4 & 1u) s_cat(o, "sus4");
    else if (t3 == 3 && t5 == 6) s_cat(o, "dim");
    else if (t3 == 4 && t5 == 8) s_cat(o, "aug");
    else if (t3 == 3) s_cat(o, "m");
    if (c->mods >> MOD_6TH & 1u) s_cat(o, "6");
    if (c->mods >> MOD_7TH & 1u)
        s_cat(o, nt[n - 1u - (c->mods >> MOD_ADD9 & 1u)] - r == 11 ? "maj7" : "7");
    if (c->mods >> MOD_ADD9 & 1u) s_cat(o, "add9");
    if ((c->mods >> MOD_FLAT5 & 1u) && !(t3 == 3 && t5 == 6)) s_cat(o, "b5");
    s_cpy(roman, t3 == 4 || (c->mods & (1u << MOD_SUS2 | 1u << MOD_SUS4)) ? RN[c->deg % 7u] : rn[c->deg % 7u]);
    if (t3 == 3 && t5 == 6)
        s_cat(roman, "o");
}
