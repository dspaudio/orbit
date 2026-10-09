/* SPDX-License-Identifier: GPL-3.0-only */
/* The real UI (ui.c, ui_draw.c, ui_studio.c, ui_layers.c, ui_song.c, ui_menu.c, ui_input.c) on a
 * framebuffer with panel / flash / storage doubles, the audio (mix_block) running between frames as on
 * the device. Renders every screen to PPM for review (DIR/page-*.ppm, live-*.ppm, layer-*.ppm) and
 * drives the panel:
 *   taps    a layer button tapped opens its pages; held + a key / knob it does not
 *   FX      held + a white key: punch-in; knobs: filter, dust, duck
 *   SEQ     held: the steps on the white keys (drums: the sound played last), OCT: pages, a step key
 *           held + KNOB 2 / 3: level / ratchet
 *   EDIT    held + a key: erase; OCT- / OCT+: undo / redo; KNOB 1 shift, 2 length x2
 *   ARP     held + a key: a roll; KNOB 1 the rate
 *   SCL     held + a key: the key of the song
 *   GLO     held + keys: mute, solo, tap tempo; knobs: levels
 *   REC     press: arm / record at once; held: the clear ring, to the end: cleared (undo brings it back)
 *   SAVE    tapped: the song page; held: the SONG layer (sections A..D: play / store, SONG REC)
 *   DRUMS   the grid: sound / step / hit / level knobs, GRID <-> KIT
 * then 20000 frames of random use: every draw stays on the screen. */
#define FELUCCA_ARRANGER 1
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>
static uint16_t screen[240*240];
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{ uint32_t i,j; assert(x+w<=240 && y+h<=240); for(j=0;j<h;j++) for(i=0;i<w;i++) screen[(y+j)*240+x+i]=p[j*w+i]; }
#include "../firmware/src/gfx.c"
static void lcd_fill(uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint16_t c)
{ uint32_t i,j; assert(x+w<=240&&y+h<=240); for(j=0;j<h;j++)for(i=0;i<w;i++)screen[(y+j)*240+x+i]=swap16(c); }
static int32_t encs[7];
static uint32_t fm1_ticks(void) { return fm1_ms * 1000u * 24u; }
#define FM1_TICKS_PER_US 24u
static int32_t fm1_enc_take(uint32_t e) { int32_t s = encs[e]; encs[e] = 0; return s; }
static uint8_t fm1_led[16], fm1_led_dim[16], fm1_led_bg[16];
static volatile uint16_t fm1_led_bg_ns;
#define FM1_NCOL 16u
static const int8_t FM1_KEYMAP[5][16];
static void fm1_led_key(uint32_t id, int on) { (void)id; (void)on; }
static uint32_t edges_btn, notes_seen;
static uint32_t fm1_input_edges(int x) { uint32_t e = edges_btn; (void)x; edges_btn = 0; return e; }
static uint32_t fm1_input_note_edges(void) { uint32_t e = fm1_in.notes & ~notes_seen; notes_seen = fm1_in.notes; return e; }
static void fm1_wdt_feed(void) {}
static int32_t fm1_adc_read(int c) { (void)c; return -1; }
static struct { uint32_t magic, stage, page, home, ui_frames; } felucca_dbg;
#define FELUCCA_ICONS 1
#define SCOPE_N 512u
static int16_t scope_buf[SCOPE_N], scope_bufr[SCOPE_N];
static uint32_t scope_w;
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
static uint32_t saves, loads;
static int project_used(uint32_t i) { return i < 2; }
static void project_save(uint32_t i) { (void)i; saves++; ui_message("SAVED"); }
static void project_load(uint32_t i) { (void)i; loads++; }
static void arrangement_save(void) {}
static uint32_t arrangement_ready(void) { return 3; }
static void arrangement_apply(uint32_t s) { (void)s; }
static void song_backup(void) {}
static void song_restore(void) {}
static uint32_t sec_stores, sec_loads;
static void section_store(uint32_t s) { sec_stores++; live_sec = (int8_t)s; }
static void section_load(uint32_t s) { sec_loads++; live_sec = (int8_t)s; }
static uint32_t section_bars(uint32_t s) { (void)s; return 1u; }
static int up_used(uint32_t k) { return k < 2; }
static int up_load(uint32_t k) { (void)k; return 0; }
static uint32_t up_count(void) { return 2; }
static uint32_t up_nth(uint32_t n) { return n; }
static uint32_t up_rank(uint32_t s) { return s; }
static void up_name(uint32_t k, char *b) { str_cpy(b, k ? "MY PAD" : "MY LEAD", 13); }
static void up_slot_label(char *b, uint32_t k) { fmt_int(b, (int32_t)k + 1); }
static void up_ui(uint32_t op, uint32_t k) { (void)op; (void)k; }
static void settings_save(void) {}
#include "../firmware/src/ui_song.c"
#include "../firmware/src/ui_studio.c"
#include "../firmware/src/icons.c"
#include "../firmware/src/ui_draw.c"
#include "../firmware/src/ui_vis.c"
#include "../firmware/src/ui_layers.c"
#include "../firmware/src/ui_menu.c"
#include "../firmware/src/ui_input.c"
#include "../firmware/src/splash.c"
static const char *outdir;
static void ppm(const char *name) {
    char path[512]; snprintf(path,sizeof path,"%s/%s.ppm",outdir,name);
    FILE *f=fopen(path,"wb"); assert(f); fprintf(f,"P6\n240 240\n255\n");
    for(unsigned i=0;i<240*240;i++) { uint16_t p=swap16(screen[i]); uint8_t rgb[3]={(p>>11)*255/31,((p>>5)&63)*255/63,(p&31)*255/31}; fwrite(rgb,1,3,f); }
    fclose(f);
}
/* one UI frame (~16 ms): the audio between (as the ISR does), then input, LEDs, draw */
static void frame(void)
{
    uint32_t q;
    static int32_t o[CTL * 2];
    for (q = 0; q < 22u; q++) {
        uint32_t sample;
        mix_block(o,CTL);
        for(sample=0;sample<CTL;sample++) { scope_bufr[scope_w&(SCOPE_N-1u)]=vis_tap[sample*2+1]; scope_buf[scope_w++&(SCOPE_N-1u)]=vis_tap[sample*2]; }
    }
    ui_input(); ui_leds(); ui_draw(); fm1_ms += 16;
}
static void frames(uint32_t n) { while (n--) frame(); }
static uint32_t BT(uint32_t b) { return 1u << panel.btn[b]; }
static void press(uint32_t b) { edges_btn |= BT(b); fm1_in.buttons |= BT(b); frame(); }
static void release(uint32_t b) { fm1_in.buttons &= ~BT(b); frame(); }
static void tap(uint32_t b) { press(b); release(b); }
static void key(uint32_t k) { fm1_in.notes |= 1u << k; frame(); fm1_in.notes &= ~(1u << k); frame(); }
static int fails;
static void check(int ok, const char *what) { printf("ui: %-74s %s\n", what, ok ? "ok" : "FAIL"); fails += !ok; }

int main(int argc, char **argv)
{
    uint32_t i;
    outdir = argc > 1 ? argv[1] : "build/host";
    panel = PANEL_DEFAULT; layers_init(); palette_set(5); fm6_init(); host_tracks_init();
    for (i=0;i<NPART;i++) {
        set_engine_of(&trk[i], TRK_DEF[i][0]); apply_preset_to(&trk[i], TRK_DEF[i][1]);
        trk[i].engine=trk[i].eng_req;
    }
    TDRUM->p[P_E0]=DRUM_DEFAULT_KIT;
    trk[0].step[0]=(step_t){.note={60,64,67},.n=3,.time=ST_NOTE,.vel=100,.rat=1};
    trk[0].step[1]=(step_t){.time=ST_TIE};
    trk[0].step[3]=(step_t){.note={72},.n=1,.time=ST_NOTE,.vel=110};
    trk[0].p[P_SLEN]=16; trk[1].p[P_SLEN]=16;
    trk[0].micro[1]=-12; step_fill_set(&trk[0],1,FC_FILL);
    check(lock_set(&trk[0],1,P_TFLT,22),"create Tape source parameter lock");
    check(orbit_capture(0,0,3,0),"copy event range");
    check(trk[0].step[0].n==3 && orbit_tape.count==4,"copy preserves source / clipboard length");
    check(orbit_drop(1,14)==2,"drop clips to destination pattern boundary");
    check(trk[1].step[14].n==3 && trk[1].step[15].time==ST_TIE,"chord, ties and dynamics preserved");
    check(!orbit_drop(NPART,0),"reject synth clipboard on drum track");
    check(!orbit_capture(NTRK,0,3,0) && !orbit_capture(0,4,3,0),"reject invalid track and selection");
    check(orbit_capture(0,0,3,1) && trk[0].step[0].time==ST_REST,"lift removes source events");
    check(orbit_drop(0,8)==4 && trk[0].step[8].vel==100,"lifted events recover on drop");
    check(trk[0].micro[9]==-12 && step_fill(&trk[0],9)==FC_FILL && lock_find(&trk[0],9,P_TFLT,0)>=0,"Tape preserves micro timing, fill conditions and remapped locks");
    check(!trk[0].micro[1] && step_fill(&trk[0],1)==FC_NORM && lock_find(&trk[0],1,P_TFLT,0)<0,"Tape lift clears source metadata");
    {
        step_t before = trk[1].step[8];
        uint32_t k;
        for(k=0;k<NLOCK;k++) trk[1].lock[k]=(plock_t){.step=(uint8_t)(k/6),.param=(uint8_t)(P_E0+k%6),.val=1};
        check(!orbit_drop(1,8) && !memcmp(&before,&trk[1].step[8],sizeof before),"Tape lock capacity failure leaves destination events untouched");
        locks_clear(&trk[1]);
    }
    TDRUM->dstep[0].on[1]=128; TDRUM->dstep[0].rat[3]=192;
    check(orbit_capture(NPART,0,0,0) && orbit_drop(NPART,2)==1,"drum clipboard copy/drop");
    check(TDRUM->dstep[2].on[1]==128 && TDRUM->dstep[2].rat[3]==192,"drum lane 16 / ratchet bits preserved");
    check(!orbit_drop(0,0),"reject drum clipboard on synth track");
    bank_resolve();
    { uint32_t matched=0; for(i=0;i<NBANK;i++) matched+=bank_pi[i]!=0xFF;
      check(matched==NBANK,"all preset bank names resolve to real engine presets"); }
    song.sel=0; go_home(); preset_go(0); frame();
    encs[panel.enc[EN_PRESET]]=1; frame();
    check(TSEL->eng_req==BANK[1].e && TSEL->preset==bank_pi[1] && str_eq(ui.msg,"SWARM ORBIT GLASS"),
          "HOME PRESETS selects and displays the second individual ORBIT sound");
    encs[panel.enc[EN_PRESET]]=2; frame();
    check(TSEL->preset==bank_pi[3] && str_eq(ui.msg,"SWARM ORBIT CHOIR"),
          "HOME PRESETS multi-detent display matches the loaded sound");
    ppm("orbit-preset-feedback");
    preset_go(0); ui.msg_t=0; frame(); ppm("orbit-tape");
    tap(B_PLAY); frames(10); check(song.playing,"Tape PLAY reaches real transport"); ppm("orbit-playing");
    tap(B_PLAY); frame(); check(!song.playing,"Tape PLAY stops transport");
    encs[panel.enc[EN_K1]]=5; frame(); check(orbit_tape.cursor==5,"Tape knob moves head");
    encs[panel.enc[EN_K1+3]]=1; frame(); check(orbit_tape.lift==1,"Tape knob chooses lift");
    orbit_tape.first=8; orbit_tape.last=11; tap(B_OCTDN);
    check(orbit_tape.count==4 && trk[0].step[8].time==ST_REST,"OCT- lift goes through panel input");
    orbit_tape.cursor=0; tap(B_OCTUP); check(trk[0].step[0].n==3,"OCT+ drops via panel input");
    /* LIFT와 Undo/Redo가 스텝 메타데이터와 활성 lock의 원래 값을 보존한다. */
    trk[0].micro[1]=-12; step_fill_set(&trk[0],1,FC_FILL);
    lock_set(&trk[0],1,P_TFLT,22);
    {
        step_t original[NSTEP]; int8_t micro[NSTEP]; uint8_t fill[NSTEP/4]; plock_t locks[NLOCK];
        memcpy(original,trk[0].step,sizeof original); memcpy(micro,trk[0].micro,sizeof micro);
        memcpy(fill,trk[0].fill,sizeof fill); memcpy(locks,trk[0].lock,sizeof locks);
        orbit_tape.first=0; orbit_tape.last=3; orbit_tape.lift=1; tap(B_OCTDN);
        check(undo_swap(0) && !memcmp(original,trk[0].step,sizeof original) &&
              !memcmp(micro,trk[0].micro,sizeof micro) && !memcmp(fill,trk[0].fill,sizeof fill) &&
              !memcmp(locks,trk[0].lock,sizeof locks),"LIFT Undo restores notes, micro, fill and lock table");
        trk[0].p[P_TFLT]=7; lock_step(&trk[0],1);
        check(trk[0].p[P_TFLT]==22 && trk[0].lk_n,"restored lock applies before Redo");
        check(undo_swap(1) && trk[0].step[0].time==ST_REST && !trk[0].micro[1] &&
              step_fill(&trk[0],1)==FC_NORM && lock_find(&trk[0],1,P_TFLT,0)<0 &&
              !trk[0].lk_n && trk[0].p[P_TFLT]==7,"LIFT Redo clears metadata and restores active lock base");
        undo_swap(0);
    }
    /* Visualizer에서 OCT±는 숨겨진 Tape와 클립보드를 바꾸지 않는다. */
    tap(B_HOME);
    {
        step_t original[NSTEP]; uint32_t revision=orbit_tape.revision; int octave=song.octave;
        memcpy(original,trk[0].step,sizeof original);
        tap(B_OCTDN);
        check(song.octave==octave-1,"Visualizer OCT- lowers octave");
        tap(B_OCTUP);
        check(vis_shown() && orbit_tape.revision==revision && !memcmp(original,trk[0].step,sizeof original),
              "Visualizer OCT buttons preserve Tape and clipboard");
        check(song.octave==octave,"Visualizer OCT buttons retain octave controls");
    }
    tap(B_HOME);
    ui.msg_t=0; frame(); ppm("orbit-edited");
    open_family(FAM_EDIT); frame(); ppm("orbit-synth");
    open_family(FAM_ENV); frame(); ppm("orbit-envelope");
    open_family(FAM_LFO); frame(); ppm("orbit-lfo");
    check(cur_page()->graph==GR_LFO,"LFO opens source page");
    open_family(FAM_LFO); frame(); ppm("orbit-lfo-dest");
    check(cur_page()->id[0]==P_LD_PIT && cur_page()->id[3]==P_LD_AMP,"LFO second press opens audible modulation destinations");
    encs[panel.enc[EN_K1+3]]=30; frame();
    check(TSEL->p[P_LD_AMP]>0,"LFO DEST KNOB4 updates the actual amplitude depth");
    set_engine_of(TSEL,1); apply_preset_to(TSEL,0); TSEL->engine=TSEL->eng_req;
    open_family(FAM_EDIT); frame(); ppm("orbit-digital");
    set_engine_of(TSEL,4); apply_preset_to(TSEL,0); TSEL->engine=TSEL->eng_req;
    open_family(FAM_EDIT); fm1_in.notes=1; frames(4); ppm("orbit-sampler"); fm1_in.notes=0; frame();
    for(i=0;i<NENGINES;i++) {
        set_engine_of(TSEL,i); apply_preset_to(TSEL,0); TSEL->engine=TSEL->eng_req;
        go_home(); open_family(FAM_EDIT); frame();
        if(i==ORBIT_SWARM) ppm("orbit-swarm");
        if(i==ORBIT_PULSE) ppm("orbit-pulse");
        if(i==ORBIT_FM4) ppm("orbit-fm4");
        encs[panel.enc[EN_K1]]=1; frame();
    }
    check(1,"every engine graphic renders with bounded framebuffer access");
    /* Representative sequence for screenshots; this does not alter firmware boot defaults. */
    {
        const uint8_t notes[16]={57,0,60,64,0,67,64,60,57,0,60,64,67,64,60,57};
        steps_clear(TSEL); TSEL->p[P_SLEN]=16;
        for(i=0;i<16;i++) if(notes[i]) TSEL->step[i]=(step_t){.note={notes[i]},.n=1,.time=ST_NOTE,.vel=100};
        TSEL->step[3]=(step_t){.note={64,67,71},.n=3,.time=ST_NOTE,.vel=110,.flags=SF_ACCENT};
        TSEL->step[6].time=ST_TIE;
    }
    set_engine_of(TSEL,0); apply_preset_to(TSEL,0); TSEL->engine=TSEL->eng_req;
    go_home(); open_family(FAM_SEQ); cursor_set(3); frame(); ppm("orbit-sequencer");
    open_family(FAM_SEQ); frame(); ppm("orbit-pattern");
    studio_open(SC_TRK); frame(); ppm("orbit-mixer");
    go_home();
    for(i=0;i<2000;i++) {
        song.sel=i%NTRK; trk[song.sel].p[P_SLEN]=(int16_t)(1+i%NSTEP);
        orbit_knob(i%4,(i%3)-1); frame();
    }
    check(orbit_tape.first<=orbit_tape.last && orbit_tape.last<NSTEP,"selection bounds survive track/length changes");
    /* Actual menu confirmation and transport for the original demo composition. */
    song.playing=0; ui.menu=1; ui.menu_sel=MI_DEMO;
    { uint32_t saved=saves, prior=trk[0].p[P_SLEN];
      menu_input(BT(B_OCTUP)); frame();
      check(orbit_demo_pending && trk[0].p[P_SLEN]==prior,"demo first confirmation preserves working song");
      menu_input(BT(B_OCTDN));
      check(!orbit_demo_pending,"demo cancellation clears confirmation");
      ui.menu=1; ui.menu_sel=MI_DEMO;
      menu_input(BT(B_OCTUP)); menu_input(BT(B_OCTUP)); frames(8);
      check(saves==saved && ui.menu==0 && song.g[G_BPM]==108,"demo loads without writing a numbered project slot"); }
    check(trk[0].eng_req==ORBIT_FM4 && trk[1].eng_req==ORBIT_SWARM && trk[2].eng_req==ORBIT_PULSE,
          "demo uses all three independent engines");
    check(trk[0].p[P_SLEN]==64 && trk[1].step[16].n==3 && trk[1].step[17].time==ST_TIE &&
          dstep_has(&TDRUM->dstep[4],2) && dstep_has(&TDRUM->dstep[2],4),"demo has four bars, chords, ties, snare and hats");
    go_home(); tap(B_PLAY); check(song.playing,"demo starts with PLAY"); ppm("orbit-demo-tape");
    { char path[512]; FILE *f; uint32_t b, j, nonzero=0; int32_t o[CTL*2];
      snprintf(path,sizeof path,"%s/orbit-first-light.wav",outdir); f=fopen(path,"wb"); assert(f);
      wav_hdr(f,(FS*12/CTL)*CTL);
      for(b=0;b<FS*12/CTL;b++) {
          mix_block(o,CTL);
          for(j=0;j<CTL;j++) { nonzero+=o[j*2]!=0 || o[j*2+1]!=0; wav_put(f,o[j*2],o[j*2+1]); }
      }
      fclose(f); check(nonzero>FS*8,"demo sequencer produces sustained non-silent PCM across its loop"); }
    ui.menu=1; ui.menu_sel=MI_DEMO; menu_input(BT(B_OCTUP));
    check(!orbit_demo_pending && song.playing,"demo loader rejects replacement while playing");
    menu_close(); tap(B_PLAY);
    go_home(); ui.msg_t=0;
    for (i=0;i<NPALETTES;i++) {
        settings.palette=i; palette_set(i); ui.force=1; frame();
        if(i>=4u) { char n[40]; snprintf(n,sizeof n,"orbit-style-%s",PALETTES[i].name); ppm(n); }
        check(i!=4u || (TE_COL[0]>>11)==((TE_COL[0]>>5)&63u)/2u,"MONO uses gray track colors");
        ui.menu=1; ui.menu_sel=MI_COLOR; ui.force=1; frame(); ui.menu=0;
    }
    /* 실제 활성 오디오 프레임의 모든 픽셀을 RGB565 양자화 오차 안에서 검사한다. */
    settings.palette=4; palette_set(4); go_home(); vis_open(); fm1_in.notes=1;
    for(i=0;i<VIS_N;i++) {
        unsigned j, colored=0, visible=0; char label[100];
        vis_style=(uint8_t)i; ui.force=1; frames(6);
        for(j=0;j<240*240;j++) {
            uint16_t p=swap16(screen[j]); int r=(p>>11)*255/31, g=((p>>5)&63)*255/63, b=(p&31)*255/31;
            colored+=abs(r-g)>8 || abs(r-b)>8 || abs(g-b)>8; visible+=p!=0;
        }
        snprintf(label,sizeof label,"MONO Visualizer %u active framebuffer is grayscale",i);
        check(!colored && visible>100,label);
    }
    fm1_in.notes=0; vis_on=0;
    printf("Undo storage: %zu bytes\n",sizeof undo);
    palette_set(5); settings.palette=5;
    printf("ORBIT checks: %d failures\n",fails);
    return fails ? 1 : 0;
}
