#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include "../media/reactions.h"
#include "../media/atmosphere.h"
#include "../virtual-camera/frame_transform.h"
#include <vector>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <cmath>
void require(bool value) {if(!value){std::cerr<<"Reaction invariant failed\n";std::exit(1);}}
void savePng(const std::vector<uint8_t>& frame,int w,int h,const std::filesystem::path& path) {
    std::vector<uint8_t> pixels(size_t(w)*h*4);
    for(int y=0;y<h;y++)for(int x=0;x<w;x++) {
        int Y=int(frame[y*w+x])-16,U=int(frame[w*h+(y/2)*w+(x&~1)])-128,V=int(frame[w*h+(y/2)*w+(x&~1)+1])-128;
        auto* p=pixels.data()+(size_t(y)*w+x)*4;
        p[0]=BYTE(std::clamp((298*Y+516*U+128)>>8,0,255));p[1]=BYTE(std::clamp((298*Y-100*U-208*V+128)>>8,0,255));p[2]=BYTE(std::clamp((298*Y+409*V+128)>>8,0,255));p[3]=255;
    }
    Gdiplus::Bitmap bitmap(w,h,w*4,PixelFormat32bppARGB,pixels.data());
    CLSID png{0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};
    require(bitmap.Save(path.c_str(),&png,nullptr)==Gdiplus::Ok);
}
void atmosphereChecks() {
    using namespace camber;
    for(auto kind:{Reaction::Rain,Reaction::Lasers}) {
        for(auto size:{std::pair{640,360},std::pair{360,640},std::pair{1920,1080}}) {
            for(float t:{0.f,.01f,.38f,1.3f,3.5f,3.99f,4.f}) {
                int count=0;atmosphere::Scene(kind,size.first,size.second,t,[&](const atmosphere::Primitive& p){
                    count++;require(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.x2)&&std::isfinite(p.y2));
                    require(std::isfinite(p.width)&&p.width>0&&p.opacity>=0&&p.opacity<=1);
                });
                require(count<=400);if(t==0||t==4)require(count==0);else require(count>0);
            }
        }
        // Equal elapsed times must produce the same result regardless of how
        // many intervening frames were rendered (no frame-rate dependence).
        ReactionAnimator test;std::vector<uint8_t> clean(640*360*3/2,128);
        test.Trigger(kind,1000);auto a=clean,b=clean,c=clean;
        test.Render(a.data(),640,360,0,false,2300);test.Render(c.data(),640,360,0,false,2400);
        test.Render(b.data(),640,360,0,false,2300);require(a==b&&a!=c);
        for(bool reduced:{false,true}) {
            ReactionAnimator cpu(false);cpu.Settings(true,reduced);cpu.Trigger(kind,1000);
            a=clean;b=clean;cpu.Render(a.data(),640,360,0,false,2000);cpu.Render(b.data(),640,360,0,false,2300);
            require(a!=clean);require(reduced?a==b:a!=b);cpu.Clear();a=clean;
            cpu.Render(a.data(),640,360,0,false,2400);require(a==clean);
        }
    }
    require(camber::atmosphere::envelope(0)==0&&camber::atmosphere::envelope(4)==0);
    // Meaningful laser geometry remains continuous between high-rate frames.
    std::vector<camber::atmosphere::Primitive> a,b;
    camber::atmosphere::Scene(Reaction::Lasers,1920,1080,1.3f,[&](auto p){a.push_back(p);});
    camber::atmosphere::Scene(Reaction::Lasers,1920,1080,1.3f+1.f/120,[&](auto p){b.push_back(p);});
    require(a.size()==b.size());for(size_t i=0;i<a.size();i++)if(a[i].line)require(std::abs(a[i].x2-b[i].x2)<4);
}
int wmain(int argc,wchar_t** argv) {
    if(argc>2 && std::wstring(argv[1])==L"--preview") {
        std::filesystem::path folder(argv[2]);std::filesystem::create_directories(folder);
        constexpr int w=960,h=540;
        std::vector<uint8_t> clean(w*h*3/2,128);
        // Generated studio background only. No camera access or saved images.
        for(int y=0;y<h;y++)for(int x=0;x<w;x++) {
            float dx=(x-w*.5f)/(w*.5f),dy=(y-h*.45f)/(h*.7f);
            int luma=int(45+38*std::exp(-(dx*dx+dy*dy)*2));
            if((dx*dx/.10f+std::pow((y-h*.40f)/(h*.23f),2))<1 ||
               (y>h*.61f&&std::abs(dx)<.24f+(y-h*.61f)/h*.2f))luma=110;
            clean[y*w+x]=BYTE(luma);
        }
        for(auto kind:{camber::Reaction::Lasers,camber::Reaction::Rain}) {
            camber::ReactionAnimator renderer;renderer.Trigger(kind,1000);
            auto name=kind==camber::Reaction::Lasers?L"lasers":L"rain";
            for(int i=0;i<120;i++) {
                auto frame=clean;renderer.Render(frame.data(),w,h,0,false,1000+i*1000/30);
                wchar_t file[64];swprintf_s(file,L"%s-%03d.png",name,i);savePng(frame,w,h,folder/file);
            }
            std::cout<<"Generated 120 synthetic frames: "<<renderer.Backend()<<"\n";
        }
        return 0;
    }
    if(argc>2 && std::wstring(argv[1])==L"--benchmark") {
        std::ofstream report{std::filesystem::path(argv[2])};require(bool(report));
        report<<"{\"synthetic\":true,\"width\":1920,\"height\":1080,\"includes\":\"sprite rendering, readback and NV12 blend; excludes capture, network, decode and display\",\"effects\":[";
        constexpr int width=1920,height=1080;
        std::vector<uint8_t> input(width*height*3/2,128);
        for(int kind=1;kind<=8;kind++) {
            camber::ReactionAnimator test;
            test.Trigger(static_cast<camber::Reaction>(kind),1000);
            std::vector<double> times;
            for(int i=0;i<120;i++) {
                auto begin=std::chrono::steady_clock::now();
                test.Render(input.data(),width,height,0,false,1000+i*30);
                auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
                if(i>=10) times.push_back(elapsed);
            }
            std::sort(times.begin(),times.end());
            if(kind>1)report<<",";
            report<<"{\"id\":"<<kind<<",\"backend\":\""<<test.Backend()<<"\",\"samples\":"<<times.size()<<",\"medianMs\":"<<times[times.size()/2]<<",\"p95Ms\":"<<times[size_t(std::ceil(times.size()*.95))-1]<<",\"maxMs\":"<<times.back()<<"}";
        }
        report<<"]}";std::cout<<"Wrote synthetic compositor timings. Physical latency and sustained-call performance remain unmeasured.\n";return 0;
    }
    atmosphereChecks();
    camber::ReactionAnimator animator;
    constexpr int w=640,h=360;
    std::vector<uint8_t> clean(w*h*3/2,128);std::fill(clean.begin(),clean.begin()+w*h,70);
    std::vector<uint8_t> sheet(w*4*h*2*4,255);
    for(int kind=1;kind<=8;kind++) {
        for(unsigned rot:{0u,90u,180u,270u}) for(bool mirror:{false,true}) {
            auto frame=clean;require(animator.Trigger(static_cast<camber::Reaction>(kind),1000));animator.Render(frame.data(),w,h,rot,mirror,2300);require(frame!=clean);
            frame=clean;animator.Render(frame.data(),w,h,rot,mirror,5000);require(frame==clean);
        }
        auto frame=clean;animator.Trigger(static_cast<camber::Reaction>(kind),1000);animator.Render(frame.data(),w,h,0,false,2300);
        int ox=((kind-1)%4)*w,oy=((kind-1)/4)*h;
        for(int y=0;y<h;y++)for(int x=0;x<w;x++) {
            int Y=int(frame[y*w+x])-16,U=int(frame[w*h+(y/2)*w+(x&~1)])-128,V=int(frame[w*h+(y/2)*w+(x&~1)+1])-128;
            auto* p=sheet.data()+(size_t(y+oy)*w*4+x+ox)*4;
            p[0]=BYTE(std::clamp((298*Y+516*U+128)>>8,0,255));p[1]=BYTE(std::clamp((298*Y-100*U-208*V+128)>>8,0,255));p[2]=BYTE(std::clamp((298*Y+409*V+128)>>8,0,255));p[3]=255;
        }
    }
    animator.Settings(false,false);require(!animator.Trigger(camber::Reaction::Hearts,1000));auto frame=clean;animator.Render(frame.data(),w,h,0,false,2000);require(frame==clean);
    animator.Settings(true,true);animator.Trigger(camber::Reaction::Hearts,1000);auto still1=clean,still2=clean;animator.Render(still1.data(),w,h,0,false,2000);animator.Render(still2.data(),w,h,0,false,2500);require(still1==still2 && still1!=clean);
    animator.Clear();frame=clean;animator.Render(frame.data(),w,h,0,false,2000);require(frame==clean);
    if(argc>1) {
        Gdiplus::Bitmap bitmap(w*4,h*2,w*16,PixelFormat32bppARGB,sheet.data());
        CLSID png{0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};
        require(bitmap.Save(argv[1],&png,nullptr)==Gdiplus::Ok);
    }
    std::cout<<"Renderer backend: "<<animator.Backend()<<"\n";
    std::cout<<"PASS eight reactions, rotated/mirrored composition, expiry, disabled output, reduced motion, clear, bounded atmosphere, time-based motion and CPU fallback\n";
}
