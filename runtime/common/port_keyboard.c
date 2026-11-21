/* DOS2PSP GPL-2.0-or-later. Small reusable ASCII editor for controller input. */
#include "port_keyboard.h"
#include <string.h>
#include <stdio.h>
static const char cells[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .-";
static const uint32_t palette[]={0x111B29FF,0x263C55FF,0x25A8B8FF,0xF3F6FAFF};
const char *port_keyboard_cells(void) { return cells; }
const uint32_t *port_keyboard_palette(void) { return palette; }
bool port_keyboard_init(PortKeyboard *s,const char *initial,unsigned limit) {
    if(!s || !initial || !limit || limit>PORT_KEYBOARD_MAX) return false;
    size_t n=0; while(n<=limit && initial[n]) ++n;
    if(n>limit) return false;
    memset(s,0,sizeof(*s)); memcpy(s->text,initial,n+1); s->length=(unsigned)n; s->limit=limit;
    return true;
}
static void accept(PortKeyboard *s) {
    s->invalid_ascii=false;
    for(unsigned i=0;i<s->length;++i) if((unsigned char)s->text[i]<32 || (unsigned char)s->text[i]>126) s->invalid_ascii=true;
    if(!s->invalid_ascii) s->result=PORT_KEYBOARD_ACCEPTED;
}
void port_keyboard_update(PortKeyboard *s,const PortInput *i) {
    if(!s || !i || s->result!=PORT_KEYBOARD_EDITING) return;
    if(i->quit || (i->pressed&PORT_ALT)) { s->result=PORT_KEYBOARD_CANCELLED; return; }
    if(i->suspended) return;
    unsigned row=s->selected/6, col=s->selected%6;
    if(i->pressed&PORT_UP) row=(row+6)%7;
    else if(i->pressed&PORT_DOWN) row=(row+1)%7;
    else if(i->pressed&PORT_LEFT) col=(col+5)%6;
    else if(i->pressed&PORT_RIGHT) col=(col+1)%6;
    s->selected=row*6+col;
    if(i->pressed&PORT_START) { accept(s); return; }
    if((i->pressed&PORT_ACTION3) || ((i->pressed&PORT_FIRE) && s->selected==39)) {
        if(s->length) s->text[--s->length]=0;
        s->invalid_ascii=false; return;
    }
    if(i->pressed&PORT_FIRE) {
        if(s->selected==40) accept(s);
        else if(s->selected==41) s->result=PORT_KEYBOARD_CANCELLED;
        else if(s->selected<39 && s->length<s->limit) {
            s->text[s->length++]=cells[s->selected]; s->text[s->length]=0; s->invalid_ascii=false;
        }
    }
}
/* Authored bitmap glyphs, rows stored as five bits. Lowercase displays uppercase. */
static const uint8_t glyphs[36][7]={
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
 {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {14,17,16,23,17,17,14},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
 {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
 {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
 {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
 {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},{14,17,17,15,1,1,14}};
static void rectangle(uint8_t *f,int pitch,int x,int y,int w,int h,uint8_t color) {
    for(int row=y;row<y+h;++row) memset(f+row*pitch+x,color,(size_t)w);
}
bool port_keyboard_draw_text(uint8_t *f,int pitch,int x,int y,const char *word,uint8_t color) {
    if(!f || !word || pitch<320 || pitch>65536 || x<0 || x>314 || y<0 || y>193) return false;
    for(;*word && x<=314;++word,x+=6) {
        unsigned char c=(unsigned char)*word; if(c>='a' && c<='z') c=(unsigned char)(c-'a'+'A');
        const uint8_t *g=NULL;
        if(c>='A' && c<='Z') g=glyphs[c-'A']; else if(c>='0' && c<='9') g=glyphs[26+c-'0'];
        for(int row=0;row<7;++row) {
            unsigned bits=g?g[row]:c==' '?0:c=='_'?(row==6?31:0):c=='-'?(row==3?14:0):
              c=='.'?(row==6?4:0):c==','?(row==5?4:row==6?8:0):
              c=='/'?(1u<<(row<5?row:4)):c==':'?((row==2||row==5)?4:0):((row==0?14:row==1?17:row==2?2:row==3?4:row==6?4:0));
            for(int col=0;col<5;++col) if(bits&(16u>>col)) f[(y+row)*pitch+x+col]=color;
        }
    }
    return true;
}
bool port_keyboard_draw(const PortKeyboard *s,uint8_t *f,int pitch) {
    return port_keyboard_draw_title(s,f,pitch,"SAVE NAME");
}
bool port_keyboard_draw_title(const PortKeyboard *s,uint8_t *f,int pitch,const char *title) {
    if(!s || !f || !title || pitch<320 || s->selected>=42 || s->length>s->limit || !s->limit || s->limit>PORT_KEYBOARD_MAX || s->text[s->length]) return false;
    for(int y=0;y<200;++y) memset(f+y*pitch,0,320);
    char heading[37]; snprintf(heading,sizeof(heading),"%.36s",title);
    rectangle(f,pitch,12,10,296,166,1); port_keyboard_draw_text(f,pitch,22,18,heading,3);
    char count[8]; snprintf(count,sizeof(count),"%u/%u",s->length,s->limit); port_keyboard_draw_text(f,pitch,250,18,count,3);
    rectangle(f,pitch,22,32,276,17,0); port_keyboard_draw_text(f,pitch,28,37,s->text,3);
    for(unsigned cell=0;cell<42;++cell) {
        int x=22+(int)(cell%6)*46, y=57+(int)(cell/6)*16;
        rectangle(f,pitch,x,y,42,14,cell==s->selected?2:0);
        char label[4]={0};
        if(cell<39) { label[0]=cells[cell]; if(cell==36) memcpy(label,"SP",3); }
        else strcpy(label,cell==39?"DEL":cell==40?"OK":"ESC");
        port_keyboard_draw_text(f,pitch,x+5,y+3,label,3);
    }
    port_keyboard_draw_text(f,pitch,12,181,s->invalid_ascii?"ERASE UNSUPPORTED CHARACTERS":"CROSS ADD  SQUARE DELETE  START OK",3);
    port_keyboard_draw_text(f,pitch,12,191,"D-PAD SELECT  CIRCLE CANCEL",3); return true;
}
