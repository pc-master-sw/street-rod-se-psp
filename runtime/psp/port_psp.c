#include "port_common.h"
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspctrl.h>
#include <pspaudio.h>
#include <psppower.h>
#include <string.h>
static unsigned int list[4096] __attribute__((aligned(16)));
static unsigned char pixels[512*512] __attribute__((aligned(16)));
static unsigned int clut[256] __attribute__((aligned(16)));
static short pcm[1024*2] __attribute__((aligned(64)));
static PortAudioRing ring;
static PortStats stats;
static PortConfig cfg;
static char asset_root[768];
static SceUID lock=-1, audio_thread=-1, exit_cb=-1, power_cb=-1;
static int audio_channel=-1;
static volatile int stopping, quit, suspended;
static bool initialized, gu_ready;
static uint32_t previous;
static int exit_callback(int a,int b,void *c) { (void)a;(void)b;(void)c; quit=1; return 0; }
static int power_callback(int a,int flags,void *c) {
    (void)a;(void)c;
    if(flags&(PSP_POWER_CB_SUSPENDING|PSP_POWER_CB_STANDBY)) suspended=1;
    if(flags&PSP_POWER_CB_RESUME_COMPLETE) suspended=0;
    return 0;
}
static int play_audio(SceSize n,void *p) {
    (void)n;(void)p;
    while(!stopping) {
        if(suspended) { sceKernelDelayThread(10000); continue; }
        sceKernelWaitSema(lock,1,NULL); size_t got=port_ring_pop(&ring,pcm,1024);
        if(got<1024) ++stats.audio_underruns;
        sceKernelSignalSema(lock,1);
        int result=sceAudioOutputBlocking(audio_channel,PSP_AUDIO_VOLUME_MAX,pcm);
        if(result<0) { quit=1; break; }
        sceKernelWaitSema(lock,1,NULL); stats.audio_played+=got; sceKernelSignalSema(lock,1);
    }
    return 0;
}
void port_platform_shutdown(void) {
    stopping=1;
    if(audio_thread>=0) { sceKernelWaitThreadEnd(audio_thread,NULL); sceKernelDeleteThread(audio_thread); audio_thread=-1; }
    if(audio_channel>=0) { sceAudioChRelease(audio_channel); audio_channel=-1; }
    if(exit_cb>=0) { sceKernelDeleteCallback(exit_cb); exit_cb=-1; }
    if(power_cb>=0) { scePowerUnregisterCallback(0); sceKernelDeleteCallback(power_cb); power_cb=-1; }
    if(lock>=0) { sceKernelDeleteSema(lock); lock=-1; }
    if(gu_ready) { sceGuDisplay(GU_FALSE); sceGuTerm(); gu_ready=false; }
    initialized=false;
}
bool port_platform_init(const PortConfig *c) {
    if(initialized || !c || c->width<1 || c->height<1 || c->width>512 || c->height>512 ||
       !c->asset_root || strlen(c->asset_root)>=sizeof(asset_root)) return false;
    cfg=*c; strcpy(asset_root,c->asset_root); cfg.asset_root=asset_root;
    stopping=quit=suspended=0; previous=0; memset(&ring,0,sizeof(ring)); memset(&stats,0,sizeof(stats));
    /* Callbacks are created on main thread and serviced by DelayThreadCB below. */
    exit_cb=sceKernelCreateCallback("native_exit",exit_callback,NULL);
    power_cb=sceKernelCreateCallback("native_power",power_callback,NULL);
    if(exit_cb<0 || power_cb<0 || sceKernelRegisterExitCallback(exit_cb)<0 || scePowerRegisterCallback(0,power_cb)<0) goto fail;
    lock=sceKernelCreateSema("native_audio_lock",0,1,1,NULL); if(lock<0) goto fail;
    if(c->enable_audio) { audio_channel=sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL,1024,PSP_AUDIO_FORMAT_STEREO);
      if(audio_channel<0) goto fail; }
    sceCtrlSetSamplingCycle(0); sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    sceGuInit(); gu_ready=true; sceGuStart(GU_DIRECT,list);
    /* Two RGB565 buffers: 557056 bytes in standard 2 MiB eDRAM. */
    sceGuDrawBuffer(GU_PSM_5650,(void*)0,512);
    sceGuDispBuffer(480,272,(void*)(512*272*2),512);
    sceGuOffset(2048-240,2048-136); sceGuViewport(2048,2048,480,272);
    sceGuScissor(0,0,480,272); sceGuEnable(GU_SCISSOR_TEST); sceGuDisable(GU_DEPTH_TEST);
    sceGuEnable(GU_TEXTURE_2D); sceGuTexWrap(GU_CLAMP,GU_CLAMP);
    sceGuFinish(); sceGuSync(0,0); sceDisplayWaitVblankStart(); sceGuDisplay(GU_TRUE);
    if(c->enable_audio) { audio_thread=sceKernelCreateThread("native_audio",play_audio,0x12,8192,PSP_THREAD_ATTR_USER,NULL);
      if(audio_thread<0 || sceKernelStartThread(audio_thread,0,NULL)<0) goto fail; }
    initialized=true; return true;
fail: port_platform_shutdown(); return false;
}
bool port_platform_poll_input(PortInput *o) {
    if(!initialized || !o) return false;
    sceKernelCheckCallback(); SceCtrlData pad;
    if(sceCtrlPeekBufferPositive(&pad,1)!=1) return false;
    uint32_t held=0;
    if((pad.Buttons&PSP_CTRL_UP)||pad.Ly<64) held|=PORT_UP;
    if((pad.Buttons&PSP_CTRL_DOWN)||pad.Ly>192) held|=PORT_DOWN;
    if((pad.Buttons&PSP_CTRL_LEFT)||pad.Lx<64) held|=PORT_LEFT;
    if((pad.Buttons&PSP_CTRL_RIGHT)||pad.Lx>192) held|=PORT_RIGHT;
    if(pad.Buttons&PSP_CTRL_CROSS) held|=PORT_FIRE;
    if(pad.Buttons&PSP_CTRL_CIRCLE) held|=PORT_ALT;
    if(pad.Buttons&PSP_CTRL_START) held|=PORT_START;
    if(pad.Buttons&PSP_CTRL_SELECT) held|=PORT_SELECT;
    if(pad.Buttons&PSP_CTRL_SQUARE) held|=PORT_ACTION3;
    if(pad.Buttons&PSP_CTRL_TRIANGLE) held|=PORT_ACTION4;
    if(pad.Buttons&PSP_CTRL_LTRIGGER) held|=PORT_LEFT_TRIGGER;
    if(pad.Buttons&PSP_CTRL_RTRIGGER) held|=PORT_RIGHT_TRIGGER;
    if((held&(PORT_START|PORT_SELECT))==(PORT_START|PORT_SELECT)) quit=1;
    memset(o,0,sizeof(*o)); port_input_update(o,held,&previous); o->quit=quit; o->suspended=suspended;
    return true;
}
bool port_platform_present_indexed(const uint8_t *p,int w,int h,int pitch,const uint32_t *pal,int n) {
    if(!initialized || suspended || w!=cfg.width || h!=cfg.height || !port_frame_valid(p,w,h,pitch,pal,n)) return false;
    for(int y=0;y<h;++y) memcpy(pixels+y*512,p+(size_t)y*pitch,(size_t)w);
    memset(clut,0,sizeof(clut)); for(int i=0;i<n;++i) clut[i]=port_rgba_to_abgr(pal[i]);
    sceKernelDcacheWritebackRange(pixels,h*512); sceKernelDcacheWritebackRange(clut,sizeof(clut));
    sceGuStart(GU_DIRECT,list); sceGuClearColor(0); sceGuClear(GU_COLOR_BUFFER_BIT);
    sceGuClutMode(GU_PSM_8888,0,255,0); sceGuClutLoad(32,clut);
    sceGuTexMode(GU_PSM_T8,0,0,0); sceGuTexImage(0,512,512,512,pixels); sceGuTexFlush();
    sceGuTexFunc(GU_TFX_REPLACE,GU_TCC_RGB); sceGuTexFilter(GU_NEAREST,GU_NEAREST);
    sceGuTexScale(1.0f,1.0f); sceGuTexOffset(0,0);
    PortViewport dst=port_viewport(480,272,w,h,cfg.aspect_4_3);
    /* Narrow sprites avoid the GE sprite rasterizer's wide-texture penalty. */
    struct Vertex { float u,v,x,y,z; };
    for(int left=0;left<w;left+=32) {
        int right=left+32<w?left+32:w;
        struct Vertex *v=sceGuGetMemory(2*sizeof(*v));
        v[0]=(struct Vertex){left,0,dst.x+(float)left*dst.width/w,dst.y,0};
        v[1]=(struct Vertex){right,h,dst.x+(float)right*dst.width/w,dst.y+dst.height,0};
        sceGuDrawArray(GU_SPRITES,GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_2D,2,NULL,v);
    }
    sceGuFinish(); sceGuSync(0,0); sceDisplayWaitVblankStart(); sceGuSwapBuffers(); ++stats.presented;
    return true;
}
size_t port_platform_audio_push_s16(const int16_t *s,size_t n) {
    if(!initialized || suspended || audio_channel<0) return 0;
    sceKernelWaitSema(lock,1,NULL); size_t got=port_ring_push(&ring,s,n);
    stats.audio_accepted+=got; sceKernelSignalSema(lock,1); return got;
}
void port_platform_get_stats(PortStats *o) {
    if(!o) return;
    if(lock>=0) sceKernelWaitSema(lock,1,NULL);
    *o=stats; o->audio_queued=ring.count;
    if(lock>=0) sceKernelSignalSema(lock,1);
}
uint64_t port_platform_ticks_us(void) { return sceKernelGetSystemTimeWide(); }
void port_platform_sleep_us(uint32_t us) { sceKernelDelayThreadCB(us?us:1); }
bool port_platform_read_asset(const char *r,void *d,size_t c,size_t *n) { return initialized && port_asset_read(asset_root,r,d,c,n); }
bool port_platform_read_pixel(int x,int y,uint32_t *rgb) {
    if(!initialized || !rgb || x<0 || x>=480 || y<0 || y>=272) return false;
    void *base; int stride,format;
    if(sceDisplayGetFrameBuf(&base,&stride,&format,PSP_DISPLAY_SETBUF_IMMEDIATE)<0 || !base || format!=PSP_DISPLAY_PIXEL_FORMAT_565) return false;
    uint16_t c=((volatile uint16_t*)base)[y*stride+x];
    *rgb=(((c&31)*255/31)<<16)|((((c>>5)&63)*255/63)<<8)|(((c>>11)&31)*255/31); return true;
}
