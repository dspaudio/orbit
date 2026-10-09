/* SPDX-License-Identifier: GPL-3.0-only */
/* Native preview: actual firmware UI, DSP and sequencer behind a local HTTP bridge. */
#include "../tests/orbit_host_support.h"
int main(int argc, char **argv)
{
    char cmd[64], path[512]; int a=0,b=0; uint32_t i;
    outdir=argc>1 ? argv[1] : "build/host";
    panel=PANEL_DEFAULT; layers_init(); palette_set(5); host_tracks_init();
    for(i=0;i<NPART;i++) { set_engine_of(&trk[i],TRK_DEF[i][0]); apply_preset_to(&trk[i],TRK_DEF[i][1]); trk[i].engine=trk[i].eng_req; }
    TDRUM->p[P_E0]=DRUM_DEFAULT_KIT;
    /* Demo is preview-only: the firmware still boots into an empty project. */
    for(i=0;i<NTRK;i++) trk[i].p[P_SLEN]=16;
    for(i=0;i<16;i+=4) { trk[0].step[i]=(step_t){.note={36+(i==8?7:0)},.n=1,.time=ST_NOTE,.vel=100}; TDRUM->dstep[i].on[0]=1; }
    TDRUM->dstep[4].on[0]|=2; TDRUM->dstep[12].on[0]|=2;
    go_home(); frame(); ppm("screen");
    while(fgets(cmd,sizeof cmd,stdin)) {
        a=b=0;
        if(sscanf(cmd,"button %d",&a)==1 && a>=0 && a<NB) tap((uint32_t)a);
        else if(sscanf(cmd,"knob %d %d",&a,&b)==2 && a>=0 && a<NE) { encs[panel.enc[a]]=clamp(b,-64,64); frame(); }
        else if(sscanf(cmd,"render %d %d",&a,&b)==2) {
            int32_t mix[CTL*2]; uint32_t n=(uint32_t)clamp(a,CTL,FS)/CTL*CTL;
            fm1_in.notes=(uint32_t)b & 0xFFFFFFu; ui_input();
            snprintf(path,sizeof path,"%s/chunk.wav",outdir); FILE *f=fopen(path,"wb"); if(!f) return 2;
            wav_hdr(f,n);
            for(i=0;i<n;i+=CTL) { uint32_t k; mix_block(mix,CTL); for(k=0;k<CTL;k++) { wav_put(f,mix[k*2],mix[k*2+1]); if(k&1u) { scope_bufr[scope_w&(SCOPE_N-1u)]=vis_tap[k*2+1]; scope_buf[scope_w++&(SCOPE_N-1u)]=vis_tap[k*2]; } } }
            fclose(f); fm1_ms+=n*1000u/FS; ui_leds(); ui_draw();
        }
        /* HTTP 조작 행의 선택 상태는 추측하지 않고 실제 C 화면 상태로 돌려준다. */
        {
            const page_t *pg=cur_page();
            int module=pg->fam==FAM_EDIT?0:pg->fam==FAM_ENV?1:pg->fam==FAM_FX?2:pg->fam==FAM_LFO?3:-1;
            const char *view=ui.home?(vis_on?"visualizer":"tape"):pg->scope==SC_TRK?"mixer":
                pg->scope==SC_DRUM?"drum":module>=0?(is_drum(TSEL)?"drum":"synth"):"other";
            ppm("screen"); printf("OK %u %s %d\n",(unsigned)song.sel,view,module); fflush(stdout);
        }
    }
    return 0;
}
