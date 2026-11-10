#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include "reactions.h"
#include "atmosphere.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <SpriteBatch.h>
#include <stdexcept>
using namespace Gdiplus;
namespace camber {
namespace {
constexpr float pi=3.14159265359f;
float seed(int i) {return std::fmod(std::sin(float(i)*127.1f+31.7f)*43758.5453f,1.f)+1.f;}
float random(int i) {return std::fmod(seed(i),1.f);}
Color palette(int i,int alpha=255) {
    constexpr unsigned rgb[]={0xff6b8a,0x6ac4ff,0xffd66b,0x95e6c1,0xb79cfa,0xffa775};
    auto c=rgb[i%6];return Color(BYTE(std::clamp(alpha,0,255)),BYTE(c>>16),BYTE(c>>8),BYTE(c));
}
void heart(Graphics& g,float x,float y,float size,int alpha) {
    GraphicsPath path;
    path.AddBezier(x,y+size*.85f,x-size*1.1f,y+size*.1f,x-size*.65f,y-size*.65f,x,y-size*.15f);
    path.AddBezier(x,y-size*.15f,x+size*.65f,y-size*.65f,x+size*1.1f,y+size*.1f,x,y+size*.85f);
    path.CloseFigure();SolidBrush brush(Color(BYTE(alpha),250,85,126));g.FillPath(&brush,&path);
    Pen shine(Color(BYTE(alpha/2),255,224,235),size*.07f);g.DrawBezier(&shine,x-size*.42f,y+size*.03f,x-size*.5f,y-size*.17f,x-size*.25f,y-size*.28f,x-size*.1f,y-size*.16f);
}
void thumb(Graphics& g,float x,float y,float size,bool down,int alpha) {
    auto state=g.Save();g.TranslateTransform(x,y);if(down) g.RotateTransform(180);
    SolidBrush shadow(Color(BYTE(alpha*.25f),0,0,0));g.FillEllipse(&shadow,-size+3,-size+5,size*2,size*2);
    LinearGradientBrush disc(PointF(-size,-size),PointF(size,size),Color(BYTE(alpha),105,183,255),Color(BYTE(alpha),18,58,116));g.FillEllipse(&disc,-size,-size,size*2,size*2);
    const PointF hand[]={{-.5f,.05f},{-.25f,.05f},{-.05f,-.38f},{-.04f,-.68f},{.12f,-.68f},{.23f,-.45f},{.18f,-.16f},{.55f,-.16f},{.68f,-.02f},{.48f,.57f},{-.25f,.57f},{-.25f,.43f},{-.5f,.43f}};
    PointF points[13];for(int i=0;i<13;i++) points[i]=PointF(hand[i].X*size,hand[i].Y*size);
    LinearGradientBrush white(PointF(0,-size),PointF(0,size),Color(BYTE(alpha),255,255,255),Color(BYTE(alpha),172,197,228));g.FillClosedCurve(&white,points,13,FillModeAlternate,.04f);g.Restore(state);
}
void atmosphereCpu(Graphics& g,int w,int h,Reaction kind,float t) {
    atmosphere::Scene(kind,w,h,t,[&](const atmosphere::Primitive& p) {
        BYTE r=BYTE(p.rgb>>16),green=BYTE(p.rgb>>8),b=BYTE(p.rgb);
        if(p.line) {
            // A bounded stack of rounded strokes approximates the hardware
            // Gaussian cross-section without a new offscreen blur buffer.
            for(int layer=4;layer>=0;layer--) {
                float width=p.width*(.2f+layer*.2f);
                float opacity=p.opacity*(layer==0?.5f:.13f);
                Pen pen(Color(BYTE(std::clamp(opacity*255,0.f,255.f)),r,green,b),std::max(.35f,width));
                pen.SetStartCap(LineCapRound);pen.SetEndCap(LineCapRound);
                g.DrawLine(&pen,p.x,p.y,p.x2,p.y2);
            }
        } else {
            GraphicsPath path;path.AddEllipse(p.x-p.width*.5f,p.y-p.height*.5f,p.width,p.height);
            PathGradientBrush brush(&path);brush.SetCenterColor(Color(BYTE(std::clamp(p.opacity*255,0.f,255.f)),r,green,b));
            Color edge(0,r,green,b);int count=1;brush.SetSurroundColors(&edge,&count);
            g.FillPath(&brush,&path);
        }
    });
}
void draw(Graphics& g,int w,int h,Reaction kind,float t) {
    if(kind==Reaction::Rain||kind==Reaction::Lasers){atmosphereCpu(g,w,h,kind,t);return;}
    const float fade=std::clamp(std::min(t/.18f,(4.f-t)/.65f),0.f,1.f),unit=float(std::min(w,h));
    const int alpha=int(235*fade);
    if(kind==Reaction::ThumbsUp||kind==Reaction::ThumbsDown) {
        const float grow=1.f-std::pow(1.f-std::min(1.f,t/.4f),3.f);
        thumb(g,w*.8f,h*.35f,unit*.14f*grow,kind==Reaction::ThumbsDown,alpha);return;
    }
    if(kind==Reaction::Hearts) {
        for(int i=0;i<12;i++) {float p=std::fmod(t*.24f+random(i),1.2f);heart(g,w*(.08f+.84f*random(i+30)) + std::sin(t*1.4f+i)*unit*.025f,h*(1.14f-p),unit*(.035f+.045f*random(i+50)),alpha);}return;
    }
    if(kind==Reaction::Balloons) {
        for(int i=0;i<8;i++) {float p=t*.32f-random(i)*.35f;float x=w*(.06f+.88f*random(i+10))+std::sin(t+i)*unit*.025f,y=h*(1.25f-p);float r=unit*(.055f+.03f*random(i+40));
            Pen string(Color(BYTE(alpha/2),231,237,244),1);g.DrawBezier(&string,x,y+r,x+r*.4f,y+r*2,x-r*.3f,y+r*2.5f,x,y+r*3.4f);
            SolidBrush brush(palette(i,alpha));g.FillEllipse(&brush,x-r,y-r*1.2f,r*2,r*2.4f);SolidBrush highlight(Color(BYTE(alpha/3),255,255,255));g.FillEllipse(&highlight,x-r*.48f,y-r*.75f,r*.32f,r*.55f);
        }return;
    }
    if(kind==Reaction::Confetti) {
        for(int i=0;i<100;i++) {float p=std::fmod(t*.31f+random(i+30),1.3f);float x=w*random(i+200)+std::sin(t*2+i)*unit*.06f,y=h*(p-.15f);auto state=g.Save();g.TranslateTransform(x,y);g.RotateTransform(t*100+i*37.f);SolidBrush brush(palette(i,alpha));g.FillRectangle(&brush,-unit*.006f,-unit*.012f,unit*.012f,unit*.024f);g.Restore(state);}return;
    }
    if(kind==Reaction::Fireworks) {
        for(int b=0;b<5;b++) {float age=t-b*.52f-.2f;if(age<0||age>1.8f) continue;float x=w*(.15f+.7f*random(b+20)),y=h*(.12f+.4f*random(b+80));float r=unit*.38f*(1-std::exp(-age*2));
            int a=int(alpha*(1-age/1.8f));for(int i=0;i<32;i++) {float angle=i*2*pi/32;float sx=x+std::cos(angle)*r,sy=y+std::sin(angle)*r+age*age*unit*.07f;Pen glow(palette(b,a/5),unit*.016f);Pen spark(palette(b,a),std::max(1.f,unit*.003f));float tx=x+std::cos(angle)*r*.8f,ty=y+std::sin(angle)*r*.8f+age*age*unit*.07f;g.DrawLine(&glow,tx,ty,sx,sy);g.DrawLine(&spark,tx,ty,sx,sy);}
        }return;
    }
}
}

// GPU sprite rendering uses original lit artwork. Readback is owned by the
// compositor worker, never the decoder/UI thread; the GDI renderer is fallback.
class GpuEffects final {
    using Texture=Microsoft::WRL::ComPtr<ID3D11Texture2D>;
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target_;
    Texture surface_,staging_;
    std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> sprites_;
    std::unique_ptr<DirectX::SpriteBatch> batch_;
    int width_=0,height_=0;
    static void ok(HRESULT hr){if(FAILED(hr))throw std::runtime_error("GPU effects unavailable");}
public:
    GpuEffects() {
        ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device_,nullptr,&context_));
        batch_=std::make_unique<DirectX::SpriteBatch>(context_.Get());
        for(int kind=0;kind<10;kind++) {
            std::vector<uint8_t> bytes(128*128*4,0);
            if(kind>=8) {
                // Premultiplied white sprites: radial scattering and a soft,
                // tapered exposure streak. Created once, reused every frame.
                for(int y=0;y<128;y++)for(int x=0;x<128;x++) {
                    float nx=(x-63.5f)/63.5f,ny=(y-63.5f)/63.5f;
                    float a=kind==8 ? std::exp(-4.8f*(nx*nx+ny*ny))*atmosphere::smooth((1-std::sqrt(nx*nx+ny*ny))/.18f)
                        : std::exp(-7.f*ny*ny)*atmosphere::smooth((1-std::abs(nx))/.12f);
                    BYTE value=BYTE(std::clamp(a*255,0.f,255.f));auto* p=bytes.data()+(y*128+x)*4;
                    p[0]=p[1]=p[2]=p[3]=value;
                }
            } else {Bitmap bitmap(128,128,512,PixelFormat32bppPARGB,bytes.data());Graphics g(&bitmap);g.SetSmoothingMode(SmoothingModeAntiAlias);
                if(kind==0)heart(g,64,55,48,255);
                else if(kind==1||kind==2)thumb(g,64,64,54,kind==2,255);
                else if(kind==3){
                    SolidBrush shadow(Color(60,0,0,0));g.FillEllipse(&shadow,24,12,90,110);
                    LinearGradientBrush fill(Point(20,10),Point(105,110),Color(255,255,255,255),Color(255,95,115,145));
                    g.FillEllipse(&fill,18,6,88,108);SolidBrush shine(Color(180,255,255,255));g.FillEllipse(&shine,32,20,15,32);
                    Pen string(Color(180,230,240,255),2);g.DrawBezier(&string,62,112,72,116,53,121,63,128);
                } else if(kind==4){LinearGradientBrush fill(Point(36,22),Point(92,106),Color(255,255,255,255),Color(255,130,150,180));g.FillRectangle(&fill,36,22,56,84);}
                else if(kind==5){for(int i=24;i>0;i--){SolidBrush glow(Color(BYTE((25-i)*2),255,255,255));g.FillEllipse(&glow,64-i,64-i,i*2,i*2);}SolidBrush core(Color(255,255,255,255));g.FillEllipse(&core,60,60,8,8);}
                else if(kind==7){for(int i=16;i>0;i--){SolidBrush glow(Color(BYTE((17-i)*3),255,255,255));g.FillRectangle(&glow,0,64-i,128,i*2);}SolidBrush core(Color(255,255,255,255));g.FillRectangle(&core,0,62,128,4);}
                else {SolidBrush cloud(Color(240,185,205,230));g.FillEllipse(&cloud,4,46,72,54);g.FillEllipse(&cloud,32,24,68,76);g.FillEllipse(&cloud,68,46,56,50);}
                g.Flush();}
            D3D11_TEXTURE2D_DESC d{};d.Width=d.Height=128;d.MipLevels=d.ArraySize=1;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_IMMUTABLE;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
            D3D11_SUBRESOURCE_DATA initial{bytes.data(),512,0};Texture texture;ok(device_->CreateTexture2D(&d,&initial,&texture));
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;ok(device_->CreateShaderResourceView(texture.Get(),nullptr,&view));sprites_.push_back(std::move(view));
        }
    }
    void Render(std::vector<uint8_t>& out,int w,int h,Reaction kind,float t) {
        if(w!=width_||h!=height_) {
            target_.Reset();surface_.Reset();staging_.Reset();
            D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=d.ArraySize=1;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.SampleDesc.Count=1;d.BindFlags=D3D11_BIND_RENDER_TARGET;
            ok(device_->CreateTexture2D(&d,nullptr,&surface_));ok(device_->CreateRenderTargetView(surface_.Get(),nullptr,&target_));
            d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ok(device_->CreateTexture2D(&d,nullptr,&staging_));width_=w;height_=h;
        }
        float clear[4]{};context_->ClearRenderTargetView(target_.Get(),clear);auto* target=target_.Get();context_->OMSetRenderTargets(1,&target,nullptr);
        D3D11_VIEWPORT viewport{0,0,float(w),float(h),0,1};context_->RSSetViewports(1,&viewport);
        const bool atmospheric=kind==Reaction::Rain||kind==Reaction::Lasers;
        batch_->Begin(atmospheric?DirectX::SpriteSortMode_Deferred:DirectX::SpriteSortMode_BackToFront);
        const float unit=float(std::min(w,h)),fade=std::clamp(std::min(t/.18f,(4.f-t)/.65f),0.f,1.f);
        auto sprite=[&](int asset,int index,float x,float y,float radius,float rotation,float depth,float opacity=1.f){
            auto c=palette(index);bool colored=asset==3||asset==4||asset==5;
            float a=fade*opacity;DirectX::XMVECTOR tint=DirectX::XMVectorSet(colored?c.GetR()/255.f*a:a,colored?c.GetG()/255.f*a:a,colored?c.GetB()/255.f*a:a,a);
            batch_->Draw(sprites_[asset].Get(),DirectX::XMFLOAT2(x,y),nullptr,tint,rotation,DirectX::XMFLOAT2(64,64),radius/64,DirectX::SpriteEffects_None,depth);
        };
        auto beam=[&](int index,float ax,float ay,float bx,float by,float opacity){
            float dx=bx-ax,dy=by-ay;auto c=palette(index);float a=fade*opacity;
            auto tint=DirectX::XMVectorSet(c.GetR()/255.f*a,c.GetG()/255.f*a,c.GetB()/255.f*a,a);
            batch_->Draw(sprites_[7].Get(),DirectX::XMFLOAT2((ax+bx)/2,(ay+by)/2),nullptr,tint,std::atan2(dy,dx),DirectX::XMFLOAT2(64,64),DirectX::XMFLOAT2(std::sqrt(dx*dx+dy*dy)/128,unit*.06f/128));
        };
        if(kind==Reaction::Hearts||kind==Reaction::Balloons){for(int i=0;i<14;i++){
            float depth=random(i+72),p=t*(.2f+depth*.12f)-random(i)*.3f;
            sprite(kind==Reaction::Hearts?0:3,i,w*(.07f+.86f*random(i+18))+std::sin(t+i)*unit*.035f,h*(1.2f-p),unit*(.035f+depth*.065f),std::sin(t+i)*.12f,depth);
        }}else if(kind==Reaction::ThumbsUp||kind==Reaction::ThumbsDown){float scale=1-std::pow(1-std::min(1.f,t/.35f),3.f);sprite(kind==Reaction::ThumbsUp?1:2,0,w*.8f,h*.35f,unit*.17f*scale,0,0);}
        else if(kind==Reaction::Confetti){for(int i=0;i<180;i++){float d=random(i+80);sprite(4,i,w*random(i+200)+std::sin(t*1.8f+i)*unit*.04f,h*(std::fmod(random(i+30)+t*(.23f+d*.12f),1.3f)-.18f),unit*(.008f+d*.012f),t*(1+d*3)+i,d);}}
        else if(kind==Reaction::Fireworks){for(int b=0;b<5;b++){float age=t-b*.52f-.2f;if(age<0||age>1.8f)continue;float radius=unit*.4f*(1-std::exp(-age*2));for(int i=0;i<48;i++){float angle=i*2*pi/48;float x=w*(.15f+.7f*random(b+20))+std::cos(angle)*radius,y=h*(.12f+.4f*random(b+80))+std::sin(angle)*radius+age*age*unit*.07f;beam(b,x-std::cos(angle)*unit*.055f,y-std::sin(angle)*unit*.055f,x,y,1-age/1.8f);}}}
        else if(atmospheric)atmosphere::Scene(kind,w,h,t,[&](const atmosphere::Primitive& p) {
            float a=p.opacity;
            auto tint=DirectX::XMVectorSet(((p.rgb>>16)&255)/255.f*a,((p.rgb>>8)&255)/255.f*a,(p.rgb&255)/255.f*a,a);
            float dx=p.x2-p.x,dy=p.y2-p.y;
            batch_->Draw(sprites_[p.line?9:8].Get(),
                DirectX::XMFLOAT2(p.line?(p.x+p.x2)*.5f:p.x,p.line?(p.y+p.y2)*.5f:p.y),nullptr,tint,
                p.line?std::atan2(dy,dx):0,DirectX::XMFLOAT2(64,64),
                DirectX::XMFLOAT2((p.line?std::sqrt(dx*dx+dy*dy):p.width)/128,(p.line?p.width:p.height)/128));
        });

        batch_->End();context_->CopyResource(staging_.Get(),surface_.Get());D3D11_MAPPED_SUBRESOURCE data{};ok(context_->Map(staging_.Get(),0,D3D11_MAP_READ,0,&data));
        out.resize(size_t(w)*h*4);for(int y=0;y<h;y++)memcpy(out.data()+size_t(y)*w*4,static_cast<const uint8_t*>(data.pData)+size_t(y)*data.RowPitch,w*4);
        context_->Unmap(staging_.Get(),0);ok(device_->GetDeviceRemovedReason());
    }
};
ReactionAnimator::ReactionAnimator(bool allowGpu):gpuFailed_(!allowGpu) {GdiplusStartupInput input;ULONG_PTR token=0;if(GdiplusStartup(&token,&input,nullptr)==Ok) graphicsToken_=token;}
ReactionAnimator::~ReactionAnimator() {if(graphicsToken_) GdiplusShutdown(graphicsToken_);}
void ReactionAnimator::Settings(bool enabled,bool reducedMotion) {enabled_=enabled;reducedMotion_=reducedMotion;if(!enabled) Clear();}
void ReactionAnimator::Clear() {reaction_=Reaction::None;started_=0;}
bool ReactionAnimator::Trigger(Reaction reaction,uint64_t now) {if(!enabled_||!graphicsToken_||reaction<Reaction::Hearts||reaction>Reaction::Fireworks)return false;reaction_=reaction;started_=now;return true;}
void ReactionAnimator::Render(uint8_t* frame,int width,int height,unsigned rotation,bool mirror,uint64_t now) {
    if(!enabled_||reaction_==Reaction::None||now<started_||now-started_>=4000) return;
    if(!frame||width<=0||height<=0||width%2||height%2) return;
    const int cw=width/2,ch=height/2;bool swap=rotation==90||rotation==270;int w=swap?ch:cw,h=swap?cw:ch;
    overlay_.resize(size_t(w)*h*4);std::memset(overlay_.data(),0,overlay_.size());
    Bitmap bitmap(w,h,w*4,PixelFormat32bppPARGB,overlay_.data());
    const float elapsed=float(now-started_)/1000;
    bool rendered=false;
    if(!reducedMotion_&&!gpuFailed_)try {
        if(!gpu_)gpu_=std::make_unique<GpuEffects>();
        gpu_->Render(overlay_,w,h,reaction_,elapsed);rendered=true;
    }catch(...){gpu_.reset();gpuFailed_=true;}
    if(!rendered){Graphics graphics(&bitmap);graphics.SetSmoothingMode(SmoothingModeAntiAlias);draw(graphics,w,h,reaction_,reducedMotion_?1.3f:elapsed);graphics.Flush();}

    if(reducedMotion_) {
        const float fade=std::clamp(std::min(elapsed/.18f,(4.f-elapsed)/.65f),0.f,1.f);
        for(auto& channel:overlay_) channel=uint8_t(channel*fade);
    }
    auto* uv=frame+size_t(width)*height;
    for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
        auto* p=overlay_.data()+(size_t(y)*w+x)*4;int a=p[3];if(!a)continue;
        int r=std::min(255,p[2]*255/a),g=std::min(255,p[1]*255/a),b=std::min(255,p[0]*255/a);
        int Y=((66*r+129*g+25*b+128)>>8)+16,U=((-38*r-74*g+112*b+128)>>8)+128,V=((112*r-94*g-18*b+128)>>8)+128;
        int u=mirror?w-1-x:x,sx=0,sy=0;
        switch(rotation){case 90:sx=y;sy=ch-1-u;break;case 180:sx=cw-1-u;sy=ch-1-y;break;case 270:sx=cw-1-y;sy=u;break;default:sx=u;sy=y;}
        auto blend=[a](uint8_t& dst,int src){dst=uint8_t((int(dst)*(255-a)+src*a+127)/255);};
        auto* row=frame+size_t(sy*2)*width+sx*2;blend(row[0],Y);blend(row[1],Y);blend(row[width],Y);blend(row[width+1],Y);
        blend(uv[size_t(sy)*width+sx*2],U);blend(uv[size_t(sy)*width+sx*2+1],V);
    }
}
}
