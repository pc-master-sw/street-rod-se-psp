#include "port_common.h"
#include <stdio.h>
#include <string.h>
bool port_frame_valid(const uint8_t *p,int w,int h,int pitch,const uint32_t *pal,int n) {
    if (!p || !pal || w<1 || h<1 || w>512 || h>512 || pitch<w || pitch>65536 || n<1 || n>256) return false;
    if (n<256) for(int y=0;y<h;++y) for(int x=0;x<w;++x) if(p[(size_t)y*pitch+x]>=n) return false;
    return true;
}
uint32_t port_rgba_to_abgr(uint32_t c) { return (c>>24)|((c>>8)&0xff00)|((c<<8)&0xff0000)|(c<<24); }
bool port_palette_cache_rgb888(PortPaletteCache *c,const uint32_t *source,unsigned count) {
    if(!c || !source || !count || count>256) return false;
    if(c->count==count && !memcmp(c->source,source,count*sizeof(*source))) return true;
    memcpy(c->source,source,count*sizeof(*source));
    for(unsigned i=0;i<count;++i) c->rgba[i]=(source[i]<<8)|255u;
    c->count=count; ++c->updates; return true;
}
PortViewport port_viewport(int sw,int sh,int w,int h,bool aspect) {
    PortViewport v={0,0,0,0};
    if(sw<1 || sh<1 || sw>8192 || sh>8192 || w<1 || h<1 || w>512 || h>512) return v;
    int aw=aspect?4:w, ah=aspect?3:h;
    v.width=sw; v.height=sw*ah/aw;
    if(v.height>sh) { v.height=sh; v.width=sh*aw/ah; }
    v.x=(sw-v.width)/2; v.y=(sh-v.height)/2; return v;
}
void port_input_update(PortInput *o,uint32_t held,uint32_t *prev) {
    o->held=held; o->pressed=held&~*prev; o->released=*prev&~held; *prev=held;
}
uint32_t port_input_after_modal(uint32_t held,uint32_t *blocked) {
    *blocked&=held; return held&~*blocked;
}
size_t port_ring_push(PortAudioRing *r,const int16_t *s,size_t n) {
    if(!s) return 0;
    if(n>PORT_AUDIO_CAPACITY-r->count) n=PORT_AUDIO_CAPACITY-r->count;
    size_t tail=(r->read+r->count)%PORT_AUDIO_CAPACITY;
    size_t first=n<PORT_AUDIO_CAPACITY-tail?n:PORT_AUDIO_CAPACITY-tail;
    memcpy(r->samples+tail*2,s,first*4); memcpy(r->samples,s+first*2,(n-first)*4);
    r->count+=n; return n;
}
size_t port_ring_pop(PortAudioRing *r,int16_t *s,size_t n) {
    if(!s) return 0;
    size_t got=n<r->count?n:r->count;
    size_t first=got<PORT_AUDIO_CAPACITY-r->read?got:PORT_AUDIO_CAPACITY-r->read;
    memcpy(s,r->samples+r->read*2,first*4); memcpy(s+first*2,r->samples,(got-first)*4);
    if(n>got) memset(s+got*2,0,(n-got)*4);
    r->read=(r->read+got)%PORT_AUDIO_CAPACITY; r->count-=got; return got;
}
bool port_asset_path(char *o,size_t cap,const char *root,const char *rel) {
    if(!o || !root || !rel || !*rel || *rel=='/' || strchr(rel,'\\') || strchr(rel,':')) return false;
    const char *p=rel;
    while(*p) { const char *end=strchr(p,'/'); size_t n=end?(size_t)(end-p):strlen(p);
        if(n==0 || (n==1 && p[0]=='.') || (n==2 && p[0]=='.' && p[1]=='.')) return false;
        if(!end) break;
        p=end+1; if(!*p) return false;
    }
    int n=snprintf(o,cap,"%s/%s",root,rel); return n>=0 && (size_t)n<cap;
}
bool port_asset_read(const char *root,const char *rel,void *data,size_t cap,size_t *size) {
    char path[1024]; if(size) *size=0;
    if(!data || !size || !port_asset_path(path,sizeof(path),root,rel)) return false;
    FILE *f=fopen(path,"rb"); if(!f) return false;
    size_t n=fread(data,1,cap,f); bool ok=!ferror(f) && fgetc(f)==EOF && !ferror(f);
    fclose(f); if(ok) *size=n; return ok;
}
