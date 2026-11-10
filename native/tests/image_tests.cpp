#include "../virtual-camera/frame_transform.h"
#include <cstdlib>
#include <iostream>
#include <vector>
void require(bool condition) {if(!condition) {std::cerr<<"Image invariant failed\n";std::exit(1);}}
int main() {
    std::vector<uint8_t> frame(24,99);mycam::FillBlackNV12(frame.data(),4,4,4);
    for(int i=0;i<16;i++) require(frame[i]==16);for(int i=16;i<24;i++) require(frame[i]==128);
    std::vector<uint8_t> source={30,40,50,60,100,150};
    mycam::DrawFittedNV12(source.data(),2,2,0,false,frame.data(),4,4,4);require(frame[0]==30&&frame[15]==60);
    mycam::DrawFittedNV12(source.data(),2,2,180,false,frame.data(),4,4,4);require(frame[0]==60&&frame[15]==30);
    mycam::DrawFittedNV12(source.data(),2,2,0,true,frame.data(),4,4,4);require(frame[0]==40&&frame[15]==50);
    std::vector<uint8_t> planar(6);mycam::Nv12ToI420(source.data(),2,2,planar.data());require(planar==source);
    std::vector<uint8_t> rectangle={30,40,50,60,70,80,90,100,100,150,110,160},turned(12);
    mycam::DrawFittedNV12(rectangle.data(),4,2,90,false,turned.data(),2,2,4);
    require(turned==std::vector<uint8_t>({70,30,80,40,90,50,100,60,100,150,110,160}));
    mycam::DrawFittedNV12(rectangle.data(),4,2,270,false,turned.data(),2,2,4);
    require(turned==std::vector<uint8_t>({60,100,50,90,40,80,30,70,110,160,100,150}));
    std::vector<uint8_t> portrait(96);mycam::FillBlackNV12(portrait.data(),8,8,8);
    mycam::DrawFittedNV12(rectangle.data(),4,2,90,false,portrait.data(),8,8,8);
    require(portrait[0]==16 && portrait[2]==70 && portrait[5]==30 && portrait[7]==16);
    require(portrait[56+2]==100 && portrait[56+5]==60);
    require(portrait[64]==128 && portrait[66]==100 && portrait[67]==150);
    std::cout<<"PASS black levels, scaling, rotation, mirroring, YUV planes\n";
}
