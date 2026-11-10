#include "../media/enhancement.h"
#include "../media/reactions.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::vector<uint8_t> makeFrame(int w,int h,int luma=64) {
    std::vector<uint8_t> data(size_t(w)*h*3/2,128);std::fill(data.begin(),data.begin()+size_t(w)*h,uint8_t(luma));return data;
}
double variance(const std::vector<uint8_t>& f,size_t count) {
    double sum=0,square=0;for(size_t i=0;i<count;i++){sum+=f[i];square+=double(f[i])*f[i];}
    return square/count-std::pow(sum/count,2);
}
int wmain(int argc,wchar_t** argv) {
    try {
        constexpr int w=640,h=360;
        camber::ProcessingBudget budget;
        for(int i=0;i<89;i++)require(!budget.Observe(5),"Fallback happened before sustained overruns");
        require(budget.Observe(5),"Sustained overrun did not trigger fallback");
        budget.Reset();for(int i=0;i<120;i++)require(!budget.Observe(i%30==0?25:2),"Transient spike triggered fallback");
        auto noise=makeFrame(w,h);std::mt19937 random(42);
        for(int i=0;i<w*h;i++)noise[i]=uint8_t(64+int(random()%13)-6);
        camber::ImageEnhancement filter;
        auto original=noise;filter.Process(original.data(),w,h);require(original==noise && filter.Applied()==0,"Original must be byte-identical");
        for(int mode:{1,2}) {
            filter.Request(mode);auto processed=noise;filter.Process(processed.data(),w,h);
            require(filter.Applied()==mode,"Hardware enhancement did not apply");
            require(variance(processed,w*h)<variance(noise,w*h),"Noise variance increased");
            if(mode==2)require(processed[w*h/2]>noise[w*h/2],"Low light did not brighten shadows");
            require(std::equal(processed.begin()+w*h,processed.end(),noise.begin()+w*h),"Neutral chroma changed");
            for(int y:{0,16,235,255}) {
                auto flat=makeFrame(w,h,y);auto before=flat;filter.Process(flat.data(),w,h);
                require(flat==before,"Uniform SDR endpoints changed");
            }
            auto edge=makeFrame(w,h,16);for(int y=0;y<h;y++)for(int x=w/2;x<w;x++)edge[y*w+x]=235;
            auto before=edge;filter.Process(edge.data(),w,h);require(edge==before,"Strong edge acquired halos or clipping");
            auto resized=makeFrame(1280,720);filter.Process(resized.data(),1280,720);require(filter.Applied()==mode,"Resize lost the preset");
            auto portrait=makeFrame(1080,1920);filter.Process(portrait.data(),1080,1920);require(filter.Applied()==mode,"Portrait buffer lost the preset");
        }
        camber::ImageEnhancement unavailable(false);unavailable.Request(2);original=noise;
        unavailable.Process(original.data(),w,h);require(original==noise && unavailable.Applied()==0,"Graphics failure changed the original");
        require(unavailable.Reason().find("unavailable")!=std::string::npos,"Fallback reason missing");
        unavailable.NewSession();unavailable.Request(0);unavailable.Process(original.data(),w,h);require(original==noise,"Retry changed Original");
        bool rejected=false;try{filter.Request(3);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"Invalid preset accepted");
        if(argc>2 && std::wstring(argv[1])==L"--benchmark") {
            std::ofstream report{std::filesystem::path(argv[2])};require(bool(report),"Report cannot be written");
            report<<"{\"synthetic\":true,\"width\":1920,\"height\":1080,\"includes\":\"GPU upload, shader, readback; optional reaction render and NV12 blend\",\"excludes\":\"capture, network, decode, consumer and display\",\"runs\":[";
            int run=0;
            for(int mode:{1,2})for(auto effect:{camber::Reaction::None,camber::Reaction::Confetti,camber::Reaction::Rain,camber::Reaction::Lasers}) {
                const bool effects=effect!=camber::Reaction::None;
                camber::ImageEnhancement candidate;candidate.Request(mode);camber::ReactionAnimator reactions;
                if(effects)reactions.Trigger(effect,1000);
                auto clean=makeFrame(1920,1080);std::vector<double> times;int applied=0;
                for(int i=0;i<160;i++) {
                    auto frame=clean;auto start=std::chrono::steady_clock::now();
                    candidate.Process(frame.data(),1920,1080);if(candidate.Applied()==mode)applied++;
                    if(effects)reactions.Render(frame.data(),1920,1080,0,false,1500+(i%60)*10);
                    const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                    if(i>=10)times.push_back(elapsed);
                }
                std::sort(times.begin(),times.end());if(run++)report<<",";
                const auto p95=times[size_t(std::ceil(times.size()*.95))-1];
                report<<"{\"mode\":"<<mode<<",\"effects\":"<<(effects?"true":"false")<<",\"effectId\":"<<int(effect)<<",\"samples\":"<<times.size()<<",\"appliedFrames\":"<<applied<<",\"p95Ms\":"<<p95<<",\"maxMs\":"<<times.back()<<",\"targetPassed\":"<<(p95<=(effects?8:4) && applied==160?"true":"false")<<"}";
            }
            report<<"]}";
        }
        std::cout<<"PASS Original identity, shader noise reduction, shadows, chroma, SDR endpoints, edge preservation, resize, GPU fallback and invalid modes\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
