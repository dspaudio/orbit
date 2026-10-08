/* SPDX-License-Identifier: GPL-3.0-only */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>
static int32_t reference[FS], actual[FS];
static void render_lfo(uint32_t engine, int dest, int rate, int wave, int phase, int fade, int32_t *pcm)
{
    uint32_t b, i;
    memset(trk,0,sizeof trk); memset(&song,0,sizeof song); host_tracks_init();
    host_preset(&trk[0],engine,0);
    trk[0].p[P_ATK]=0; trk[0].p[P_SUS]=127; trk[0].p[P_REL]=0;
    trk[0].p[P_LRATE]=rate; trk[0].p[P_LWAVE]=wave;
    trk[0].p[P_LPHASE]=phase; trk[0].p[P_LFADE]=fade;
    trk[0].p[P_E4]=70;
    trk[0].p[P_E6]=0;
    for(i=P_LD_PIT;i<=P_LD_AMP;i++)trk[0].p[i]=0;
    if(dest>=0)trk[0].p[dest]=dest==P_LD_AMP?100:40;
    trk_note_on(&trk[0],60,100);
    for(b=0;b<FS/CTL;b++)track_render(&trk[0],pcm+b*CTL,CTL);
    for(i=(FS/CTL)*CTL;i<FS;i++)pcm[i]=0;
}
static double difference(void)
{
    double sum=0;
    for(uint32_t i=0;i<FS;i++){double d=(double)actual[i]-reference[i];sum+=d*d;}
    return sqrt(sum/FS);
}
int main(void)
{
    uint32_t e; int d;
    for(e=ORBIT_SWARM;e<NENGINES;e++) {
        render_lfo(e,-1,60,0,0,0,reference);
        render_lfo(e,-1,95,3,32,100,actual);
        assert(difference()==0);
        for(d=P_LD_PIT;d<=P_LD_AMP;d++) {
            render_lfo(e,d,60,0,0,0,actual);
            double rms=difference(); assert(rms>50);
            printf("%s destination %s: PCM difference RMS %.1f\n",ENGINES[e]->name,TP[d].label,rms);
        }
        render_lfo(e,P_LD_AMP,60,0,0,0,reference);
        render_lfo(e,P_LD_AMP,95,0,0,0,actual);assert(difference()>50);
        render_lfo(e,P_LD_AMP,60,3,0,0,actual);assert(difference()>50);
        render_lfo(e,P_LD_AMP,60,0,32,0,actual);assert(difference()>50);
        render_lfo(e,P_LD_AMP,60,0,0,100,actual);assert(difference()>50);
    }
    puts("PASS: zero-depth source controls are neutral; all four destinations and all four source controls change actual PCM on SWARM/PULSE/FM4");
    return 0;
}
