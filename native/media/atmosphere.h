#pragma once
#include "reactions.h"
#include <algorithm>
#include <cmath>

namespace camber::atmosphere {
// A bounded, time-based scene shared by hardware rendering and CPU fallback.
// No simulation steps, retained particles, camera images or external assets.
struct Primitive {
    bool line;
    float x, y, x2, y2, width, height, opacity;
    unsigned rgb;
};
inline float smooth(float x) {x=std::clamp(x,0.f,1.f);return x*x*(3-2*x);}
inline float fract(float x) {return x-std::floor(x);}
inline float noise(unsigned i) {
    i^=i>>16;i*=0x7feb352du;i^=i>>15;i*=0x846ca68bu;i^=i>>16;
    return float(i&0xffffffu)/16777216.f;
}
inline float envelope(float t) {return smooth(t/.38f)*smooth((4.f-t)/.75f);}

template<class Emit> void Scene(Reaction kind,int w,int h,float t,Emit emit) {
    const float u=float(std::min(w,h)),fade=envelope(t);
    if(fade<=0 || (kind!=Reaction::Rain && kind!=Reaction::Lasers))return;
    auto glow=[&](float x,float y,float width,float height,unsigned color,float opacity) {
        emit(Primitive{false,x,y,0,0,width,height,fade*opacity,color});
    };
    auto line=[&](float x,float y,float x2,float y2,float width,unsigned color,float opacity) {
        emit(Primitive{true,x,y,x2,y2,width,0,fade*opacity,color});
    };
    constexpr float pi=3.14159265359f;
    if(kind==Reaction::Lasers) {
        constexpr unsigned colors[]={0x43bbff,0x617dff,0x9676ff,0xe782ff,0x5bddff};
        // Low-opacity atmospheric scattering stays near the edges.
        for(int side=0;side<2;side++) {
            float x=w*(side?.88f:.12f);
            glow(x,h*.91f,u*.85f,u*.65f,side?0xa879ff:0x439cff,.14f);
            glow(x,h*.96f,u*.35f,u*.19f,0x91caff,.24f);
        }
        for(int side=0;side<2;side++)for(int i=0;i<5;i++) {
            float phase=i*.63f+side*1.8f;
            float ax=w*(side?.88f:.12f)+std::sin(t*.65f+side)*u*.018f;
            float ay=h*.97f;
            auto endpoint=[&](float time) {
                return w*(.12f+.76f*i/4.f)+std::sin(time*.8f+phase)*w*.16f;
            };
            float bx=endpoint(t),by=-h*.12f;
            float reveal=smooth((t-.05f-i*.025f)/.55f);
            bx=ax+(bx-ax)*reveal;by=ay+(by-ay)*reveal;
            float breath=.88f+.12f*std::sin(t*1.7f+phase);
            unsigned color=colors[(i+side)%5];
            // Continuous exposure tail, broad scattering, colored shaft and
            // a narrow near-white core. No flashing or frame-random jitter.
            float previous=endpoint(t-.045f);
            line(ax,ay,ax+(previous-ax)*reveal,by,u*.08f,color,.09f*breath);
            line(ax,ay,bx,by,u*.07f,color,.24f*breath);
            line(ax,ay,bx,by,std::max(4.5f,u*.017f),color,.60f*breath);
            // Gaussian sprites have a narrow effective center. Keep it above
            // one overlay pixel so diagonal cores remain continuous at 720p.
            line(ax,ay,bx,by,std::max(2.8f,u*.006f),0xe4f3ff,.88f*breath);
            for(int p=0;p<2;p++) {
                float travel=fract(t*(.26f+i*.018f)+p*.5f+noise(i+side*20));
                float light=std::sin(pi*travel)*.48f;
                glow(ax+(bx-ax)*travel,ay+(by-ay)*travel,u*.034f,u*.034f,color,light);
            }
        }
        return;
    }
    // Soft, drifting cloud shadows; an irregular layered band rather than
    // opaque scallops. Keep the subject visible through the weather.
    for(int i=0;i<12;i++) {
        float depth=noise(i+500);
        glow(w*(-.08f+1.16f*i/11.f)+std::sin(t*.22f+i)*u*.025f,
             -h*.015f+u*.028f*std::sin(i*1.7f),u*(.43f+depth*.3f),u*(.2f+depth*.12f),
             i%2?0x52677f:0x9baec3,.20f+depth*.12f);
    }
    float wind=-.105f+.028f*std::sin(t*.8f);
    // Three parallax layers: fine distant rain, medium streaks and sparse,
    // longer foreground drops. Each drop keeps its own speed and phase.
    for(int layer=0;layer<3;layer++) {
        int count=layer==0?64:layer==1?48:24;
        for(int i=0;i<count;i++) {
            unsigned id=unsigned(i+layer*170);
            float depth=(layer+noise(id+71)) / 3.f;
            float speed=.48f+depth*.72f+noise(id+290)*.18f;
            float fall=fract(noise(id+130)+t*speed);
            float y=h*(-.14f+1.3f*fall);
            float x=w*(-.18f+1.36f*noise(id+20))+wind*y
                +u*.015f*std::sin(t*.8f+noise(id+42)*6.28f);
            float length=u*(.015f+depth*.055f);
            float width=std::max(1.5f,u*(.003f+depth*.006f));
            float a=.18f+depth*.43f;
            line(x-wind*length,y-length,x,y,width*2.6f,0x8cbbdd,a*.18f);
            line(x-wind*length,y-length,x,y,width,layer==2?0xd1e9fa:0x91b7d5,a);
        }
    }
    // Small ballistic impact droplets and expanding broken ripples at the
    // bottom edge. Alpha vanishes before the deterministic cycle wraps.
    for(int i=0;i<18;i++) {
        float life=fract(t*(.8f+noise(i+980)*.5f)+noise(i+900));
        float visibility=std::sin(pi*life)*(1-life);
        float x=w*noise(i+800),y=h*(.955f+.065f*noise(i+850));
        float radius=u*(.004f+.03f*life);
        for(int side:{-1,1}) {
            float sx=x+side*u*.033f*life,sy=y-u*.09f*life*(1-life);
            line(sx-side*u*.004f,sy+u*.006f,sx,sy,std::max(.7f,u*.0017f),0xc7e5f7,visibility*.5f);
            line(x+side*radius*.45f,y,x+side*radius,y+u*.001f,std::max(.65f,u*.0012f),0x8bb9d8,visibility*.28f);
        }
    }
    for(int i=0;i<4;i++)glow(w*(.05f+i*.3f)+std::sin(t*.3f+i)*u*.05f,h*1.03f,
        u*.85f,u*.20f,0x8cacbd,.055f);
}
}
