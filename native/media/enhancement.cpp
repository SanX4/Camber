#include "enhancement.h"
#include "../common/frame_limits.h"
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include "enhance_vs.h"
#include "enhance_ps.h"
namespace camber {
using Microsoft::WRL::ComPtr;
namespace {void check(HRESULT hr){if(FAILED(hr))throw std::runtime_error("Image processing unavailable");}}
class ImageEnhancement::Impl final {
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<ID3D11VertexShader> vs_;
    ComPtr<ID3D11PixelShader> ps_;
    ComPtr<ID3D11Buffer> settings_;
    ComPtr<ID3D11SamplerState> sampler_;
    struct Plane {
        ComPtr<ID3D11Texture2D> input,output,staging;
        ComPtr<ID3D11ShaderResourceView> srv;
        ComPtr<ID3D11RenderTargetView> rtv;
    } planes_[2];
    int width_=0,height_=0;
    void allocate(int width,int height) {
        width_=height_=0;
        for(int p=0;p<2;p++) {
            planes_[p]={};auto& plane=planes_[p];
            const auto format=p==0?DXGI_FORMAT_R8_UNORM:DXGI_FORMAT_R8G8_UNORM;
            UINT support=0;check(device_->CheckFormatSupport(format,&support));
            if(!(support&D3D11_FORMAT_SUPPORT_RENDER_TARGET))throw std::runtime_error("Unsupported image plane");
            D3D11_TEXTURE2D_DESC d{};d.Width=width/(p+1);d.Height=height/(p+1);
            d.MipLevels=1;d.ArraySize=1;d.SampleDesc.Count=1;d.Format=format;d.Usage=D3D11_USAGE_DEFAULT;
            d.BindFlags=D3D11_BIND_SHADER_RESOURCE;check(device_->CreateTexture2D(&d,nullptr,&plane.input));
            check(device_->CreateShaderResourceView(plane.input.Get(),nullptr,&plane.srv));
            d.BindFlags=D3D11_BIND_RENDER_TARGET;check(device_->CreateTexture2D(&d,nullptr,&plane.output));
            check(device_->CreateRenderTargetView(plane.output.Get(),nullptr,&plane.rtv));
            d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
            check(device_->CreateTexture2D(&d,nullptr,&plane.staging));
        }
        width_=width;height_=height;
    }
public:
    Impl() {
        check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device_,nullptr,&context_));
        check(device_->CreateVertexShader(EnhanceVS,sizeof(EnhanceVS),nullptr,&vs_));
        check(device_->CreatePixelShader(EnhancePS,sizeof(EnhancePS),nullptr,&ps_));
        D3D11_BUFFER_DESC b{};b.ByteWidth=16;b.Usage=D3D11_USAGE_DEFAULT;b.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        check(device_->CreateBuffer(&b,nullptr,&settings_));
        D3D11_SAMPLER_DESC s{};s.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;s.AddressU=s.AddressV=s.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
        s.MaxLOD=D3D11_FLOAT32_MAX;check(device_->CreateSamplerState(&s,&sampler_));
    }
    void run(uint8_t* nv12,int width,int height,int mode) {
        if(width!=width_ || height!=height_)allocate(width,height);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context_->VSSetShader(vs_.Get(),nullptr,0);context_->PSSetShader(ps_.Get(),nullptr,0);
        auto* buffer=settings_.Get();context_->PSSetConstantBuffers(0,1,&buffer);
        auto* sampler=sampler_.Get();context_->PSSetSamplers(0,1,&sampler);
        for(int p=0;p<2;p++) {
            auto& plane=planes_[p];
            const int w=width/(p+1),h=height/(p+1);
            struct {float x,y;UINT mode,plane;} constants{1.f/w,1.f/h,UINT(mode),UINT(p)};
            context_->UpdateSubresource(settings_.Get(),0,nullptr,&constants,0,0);
            context_->UpdateSubresource(plane.input.Get(),0,nullptr,nv12+(p?size_t(width)*height:0),width,0);
            D3D11_VIEWPORT viewport{0,0,float(w),float(h),0,1};context_->RSSetViewports(1,&viewport);
            auto* rtv=plane.rtv.Get();context_->OMSetRenderTargets(1,&rtv,nullptr);
            auto* srv=plane.srv.Get();context_->PSSetShaderResources(0,1,&srv);
            context_->Draw(3,0);
            ID3D11ShaderResourceView* empty=nullptr;context_->PSSetShaderResources(0,1,&empty);
            context_->OMSetRenderTargets(0,nullptr,nullptr);
            context_->CopyResource(plane.staging.Get(),plane.output.Get());
        }
        // Map BOTH outputs before touching the input. A partial GPU failure
        // must leave a byte-identical original available for safe fallback.
        D3D11_MAPPED_SUBRESOURCE maps[2]{};
        check(context_->Map(planes_[0].staging.Get(),0,D3D11_MAP_READ,0,&maps[0]));
        auto hr=context_->Map(planes_[1].staging.Get(),0,D3D11_MAP_READ,0,&maps[1]);
        if(FAILED(hr)){context_->Unmap(planes_[0].staging.Get(),0);check(hr);}
        for(int p=0;p<2;p++) {
            auto* out=nv12+(p?size_t(width)*height:0);
            for(int y=0;y<height/(p+1);y++)memcpy(out+size_t(y)*width,static_cast<uint8_t*>(maps[p].pData)+size_t(y)*maps[p].RowPitch,width);
            context_->Unmap(planes_[p].staging.Get(),0);
        }
    }
};
ImageEnhancement::ImageEnhancement(bool allowGpu):allowGpu_(allowGpu){}
ImageEnhancement::~ImageEnhancement()=default;
void ImageEnhancement::Request(int mode) {
    if(mode<0 || mode>2)throw std::invalid_argument("Unknown enhancement preset");
    requested_=mode;failed_=false;budget_.Reset();Inactive();
    if(mode==0)reason_="No additional image filters.";
}
void ImageEnhancement::NewSession(){failed_=false;budget_.Reset();Inactive();}
void ImageEnhancement::Process(uint8_t* frame,int width,int height) {
    applied_=0;milliseconds_=0;
    if(requested_==0){reason_="No additional image filters.";return;}
    if(failed_)return;
    if(!frame || !ValidCaptureDimensions(width,height))return;
    const auto start=std::chrono::steady_clock::now();
    try {
        if(!allowGpu_)throw std::runtime_error("Hardware image processing unavailable");
        if(!impl_)impl_=std::make_unique<Impl>();
        impl_->run(frame,width,height,requested_);applied_=requested_;reason_="Applied locally on this PC.";
        milliseconds_=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        if(budget_.Observe(milliseconds_)){failed_=true;reason_="Processing is slow. Using Original on the next frame; select a preset again or start a new session to retry.";}
    }catch(...) {
        impl_.reset();failed_=true;reason_="Image processing is unavailable. Using Original.";
    }
}
}
