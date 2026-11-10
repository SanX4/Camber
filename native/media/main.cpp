#include <winsock2.h>
#include <windows.h>
#include <mfapi.h>
#include <mfvirtualcamera.h>
#include <wrl.h>
#include <sddl.h>
#include <ks.h>
#include <atomic>
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <optional>
#include <functional>
#include <string>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <libwebrtc.h>
#include <rtc_peerconnection.h>
#include <rtc_video_track.h>
#include <rtc_rtp_transceiver.h>
#include <rtc_rtp_capabilities.h>
#include "../common/guids.h"
#include "../common/shared_frame.h"
#include "reactions.h"
#include "enhancement.h"
#include "../common/object_security.h"
using nlohmann::json;
using namespace libwebrtc;
using namespace mycam;
using Microsoft::WRL::ComPtr;
std::mutex outputGate;
#ifdef CAMBER_MEDIA_TEST
void emit(const json& data);
#else
void emit(const json& data) {std::lock_guard<std::mutex> lock(outputGate);std::cout<<data.dump()<<std::endl;}
#endif
class WindowsFailure final : public std::runtime_error {
public:
    HRESULT code;
    std::string operation;
    WindowsFailure(HRESULT result,const char* name):std::runtime_error(name),code(result),operation(name) {}
};
void check(HRESULT result,const char* operation="Windows camera operation") {if(FAILED(result)) throw WindowsFailure(result,operation);}

class WindowsSockets final {
public:
    WindowsSockets() {WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data)!=0) throw std::runtime_error("Windows video networking could not start");}
    ~WindowsSockets() {WSACleanup();}
    WindowsSockets(const WindowsSockets&)=delete;
    WindowsSockets& operator=(const WindowsSockets&)=delete;
};
class Receiver final : public RTCPeerConnectionObserver, public RTCVideoRenderer<scoped_refptr<RTCVideoFrame>> {
    WindowsSockets sockets_;
    struct DecodeStatistics {
        std::mutex gate;
        bool alive=true,busy=false;
        long long generation=-1;
        ULONGLONG tick=0;
        double frames=-1,seconds=-1;
        int width=0,height=0,rotation=0;
    };
    std::shared_ptr<DecodeStatistics> statistics_=std::make_shared<DecodeStatistics>();
    camber::ReactionAnimator reactions_;
    camber::ImageEnhancement enhancement_;
    ULONGLONG enhancementReport_=0;
    std::string enhancementRequest_;
    std::vector<uint8_t> framePixels_;
    scoped_refptr<RTCPeerConnectionFactory> factory_;
    scoped_refptr<RTCPeerConnection> peer_;
    scoped_refptr<RTCVideoTrack> video_;
    std::mutex publishGate_, pendingGate_;
    struct EnhancementCommand {int mode;std::string id;long long generation;};
    struct ReactionCommand {int kind;long long generation;ULONGLONG tick;};
    struct Controls {
        std::mutex gate;
        bool reset=true,enabled=true,reduced=false,mirror=false,fill=false;
        std::optional<EnhancementCommand> enhance;
        std::optional<ReactionCommand> reaction;
#ifdef CAMBER_MEDIA_TEST
        std::function<void()> testHook;
#endif
    } controls_;
    void applyControls(long long g) {
        bool reset,enabled,reduced;
        std::optional<EnhancementCommand> enhancement;
        std::optional<ReactionCommand> reaction;
#ifdef CAMBER_MEDIA_TEST
        std::function<void()> hook;
#endif
        {std::lock_guard lock(controls_.gate);
            reset=controls_.reset;controls_.reset=false;enabled=controls_.enabled;reduced=controls_.reduced;
            mirror_=controls_.mirror;fill_=controls_.fill;
            if(controls_.enhance && controls_.enhance->generation<=g){enhancement=std::move(controls_.enhance);controls_.enhance.reset();}
            reaction=std::move(controls_.reaction);controls_.reaction.reset();
#ifdef CAMBER_MEDIA_TEST
            hook=controls_.testHook;
#endif
        }
        if(reset){reactions_.Clear();enhancement_.NewSession();}
        reactions_.Settings(enabled,reduced);
        if(enhancement && enhancement->generation==g){enhancement_.Request(enhancement->mode);enhancementRequest_=enhancement->id;}
        if(reaction && reaction->generation==g)reactions_.Trigger(static_cast<camber::Reaction>(reaction->kind),reaction->tick);
#ifdef CAMBER_MEDIA_TEST
        if(hook)hook();
#endif
    }
    camber::FrameProofKey ownership_;
    std::condition_variable pendingCv_;
    struct Pending { std::vector<uint8_t> pixels; int width,height; unsigned rotation; long long generation; ULONGLONG tick; };
    std::optional<Pending> pending_;
    bool exiting_=false;
    std::string reportedBackend_;
    std::thread compositor_;
    void compositeLoop() {
        while(true) {
            std::optional<Pending> frame;
            {std::unique_lock lock(pendingGate_);pendingCv_.wait(lock,[this]{return exiting_||pending_.has_value();});if(exiting_)return;frame=std::move(pending_);pending_.reset();}
            if(!permitted_ || frame->generation!=generation_ || GetTickCount64()-frame->tick>250)continue;
            applyControls(frame->generation);
            if(!permitted_ || frame->generation!=generation_)continue;
            enhancement_.Process(frame->pixels.data(),frame->width,frame->height);
            if(GetTickCount64()-enhancementReport_>=1000) {
                reportEnhancement(frame->generation);enhancementReport_=GetTickCount64();
            }
            reactions_.Render(frame->pixels.data(),frame->width,frame->height,frame->rotation,mirror_,GetTickCount64());
            if(reportedBackend_!=reactions_.Backend()){reportedBackend_=reactions_.Backend();emit({{"type","effectsStatus"},{"backend",reportedBackend_}});}
            std::lock_guard publication(publishGate_);
            if(!permitted_ || frame->generation!=generation_ || GetTickCount64()-frame->tick>250 || GetTickCount64()-controlTick_.load()>6000 || !openFrameMap())continue;
            auto result=WaitForSingleObject(mutex_,0);if(result!=WAIT_OBJECT_0&&result!=WAIT_ABANDONED)continue;
            auto* header=static_cast<SharedHeader*>(view_);
            if(header->magic!=kSharedMagic||header->version!=kSharedVersion){ReleaseMutex(mutex_);continue;}
            header->ownerProofDigest=ownership_.Digest();
            memcpy(FrameData(view_),frame->pixels.data(),frame->pixels.size());
            header->width=frame->width;header->height=frame->height;header->frameTick=frame->tick;
            header->rotation=frame->rotation;header->mirror=mirror_;header->fill=fill_;
            InterlockedIncrement(&header->seq);InterlockedExchange(&header->enabled,1);ReleaseMutex(mutex_);
            emit({{"type","frame"},{"generation",frame->generation}});
        }
    }
    std::atomic<bool> permitted_{false};
    std::atomic<long long> generation_{0};
    HANDLE map_=nullptr,mutex_=nullptr;
    void* view_=nullptr;
    std::wstring mapName_;
    bool mirror_=false,fill_=false;
    std::atomic<ULONGLONG> controlTick_{GetTickCount64()};
    std::atomic<bool> sawDecodedFrame_{false};
    void stage(const char* value) {emit({{"type","videoStatus"},{"stage",value},{"generation",generation_.load()}});}
    bool openFrameMap() {
        if(view_) return true;
        map_=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,mapName_.c_str());if(!map_) return false;
        mutex_=OpenMutexW(READ_CONTROL|SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,(mapName_+L".lock").c_str());
        if(!mutex_) {CloseHandle(map_);map_=nullptr;return false;}
        auto sid=camber::CurrentUserSid();
        if(sid.empty() || !camber::RestrictedObject(map_,sid)||!camber::RestrictedObject(mutex_,sid)){CloseHandle(map_);CloseHandle(mutex_);map_=mutex_=nullptr;return false;}
        view_=MapViewOfFile(map_,FILE_MAP_ALL_ACCESS,0,0,kSharedBytes);
        if(!view_){CloseHandle(map_);CloseHandle(mutex_);map_=mutex_=nullptr;return false;}
        return true;
    }
public:
    explicit Receiver(const std::wstring& name):mapName_(name) {
        if(!LibWebRTC::Initialize()) throw std::runtime_error("WebRTC initialization failed");
        factory_=LibWebRTC::CreateRTCPeerConnectionFactory();
        if(!factory_||!factory_->Initialize()) throw std::runtime_error("WebRTC factory unavailable");
        compositor_=std::thread([this]{compositeLoop();});
    }
    ~Receiver() {{std::lock_guard lock(statistics_->gate);statistics_->alive=false;}stop();closePeer();{std::lock_guard lock(pendingGate_);exiting_=true;pending_.reset();}pendingCv_.notify_one();if(compositor_.joinable())compositor_.join();factory_->Terminate();factory_=nullptr;LibWebRTC::Terminate();if(view_) UnmapViewOfFile(view_);if(map_) CloseHandle(map_);if(mutex_) CloseHandle(mutex_);}
    void closePeer() {
        if(video_) {video_->RemoveRenderer(this);video_=nullptr;}
        if(peer_) {peer_->DeRegisterRTCPeerConnectionObserver();peer_->Close();factory_->Delete(peer_);peer_=nullptr;}
    }
    void permit(long long g) {
        if(g<=generation_)return;
        stop();closePeer();
        {std::lock_guard publication(publishGate_);
        if(!ownership_.Rotate())throw std::runtime_error("Camera authorization could not be created");
        generation_=g;sawDecodedFrame_=false;permitted_=true;}
        {std::lock_guard lock(statistics_->gate);statistics_->generation=g;statistics_->frames=statistics_->seconds=-1;statistics_->busy=false;statistics_->tick=0;statistics_->width=statistics_->height=0;statistics_->rotation=0;}
        stage("permitted");
    }
    bool authorizeCamera(IMFVirtualCamera* camera) {
        camber::ProofBytes token{};long long generation;
        {std::lock_guard publication(publishGate_);if(!permitted_)return true;token=ownership_.Token();generation=generation_;}
        ULONG returned=0;
        const auto hr=camera->SendCameraProperty(kOwnershipPropertySet,kOwnershipPropertyId,KSPROPERTY_TYPE_SET,
            nullptr,0,token.data(),ULONG(token.size()),&returned);
        SecureZeroMemory(token.data(),token.size());
        if(hr!=S_OK) {
            revoke(generation);
            emit({{"type","ownershipError"},{"generation",generation},{"message","Windows could not authorize this camera session. Sharing stopped. Restart Camber. If this continues, update or repair the Windows app."}});
            return false;
        }
        return true;
    }
    void revoke(long long g) {stop();if(g>generation_) generation_=g;}
    void stop() {
        {std::lock_guard publication(publishGate_);permitted_=false;ownership_.Clear();if(view_){
            InterlockedExchange(&static_cast<SharedHeader*>(view_)->enabled,0);
            // The output enable bit cuts off immediately. Digest revocation is
            // serialized with readers; if contended, disabled+stale cutoff apply.
            auto result=WaitForSingleObject(mutex_,0);
            if(result==WAIT_OBJECT_0 || result==WAIT_ABANDONED){SecureZeroMemory(static_cast<SharedHeader*>(view_)->ownerProofDigest.data(),32);ReleaseMutex(mutex_);}
        }}
        {std::lock_guard lock(pendingGate_);pending_.reset();}
        {std::lock_guard lock(controls_.gate);controls_.reset=true;controls_.reaction.reset();controls_.enhance.reset();}
    }
    void renew() {
        controlTick_=GetTickCount64();
        if(!peer_ || !permitted_)return;
        auto state=statistics_;const auto g=generation_.load();
        {std::lock_guard lock(state->gate);if(state->busy || GetTickCount64()-state->tick<1000)return;state->busy=true;state->tick=GetTickCount64();}
        peer_->GetStats([state,g](const vector<scoped_refptr<MediaRTCStats>> reports) {
            std::lock_guard lock(state->gate);
            if(!state->alive || state->generation!=g)return;
            state->busy=false;
            json result={{"type","decoderStats"},{"generation",g}};
            if(state->width>0 && state->height>0){result["width"]=state->width;result["height"]=state->height;result["rotation"]=state->rotation;}
            try {
                for(size_t i=0;i<reports.size();i++) {
                    if(reports[i]->type().std_string()!="inbound-rtp")continue;
                    const auto text=reports[i]->ToJson().std_string();if(text.size()>32768)continue;
                    const auto data=json::parse(text);
                    if(data.value("kind",data.value("mediaType",std::string()))!="video")continue;
                    auto number=[&](const char* key)->double {auto it=data.find(key);if(it==data.end()||!it->is_number())return -1;auto v=it->get<double>();return std::isfinite(v)&&v>=0?v:-1;};
                    const auto frames=number("framesDecoded"),seconds=number("totalDecodeTime");
                    if(frames>state->frames && state->frames>=0 && seconds>=state->seconds && state->seconds>=0) {
                        const auto ms=(seconds-state->seconds)*1000/(frames-state->frames);
                        if(std::isfinite(ms)&&ms<=10000)result["decodeMs"]=ms;
                    }
                    state->frames=frames;state->seconds=seconds;break;
                }
            } catch(...) {} // Optional statistics never interrupt video; no raw report leaves this callback.
            emit(result);
        },[state,g](const char*) {std::lock_guard lock(state->gate);if(state->generation==g)state->busy=false;});
    }
    void reportEnhancement(long long g) {
        emit({{"type","enhancementState"},{"requested",enhancement_.Requested()},{"applied",enhancement_.Applied()},
            {"reason",enhancement_.Reason()},{"processingMs",enhancement_.Milliseconds()},{"fallback",enhancement_.FallenBack()},{"generation",g},{"id",enhancementRequest_}});
    }
    void enhance(int mode,const std::string& id,long long g) {
        if(g<generation_ || mode<0 || mode>2 || id.size()>64)return;
        {std::lock_guard lock(controls_.gate);controls_.enhance=EnhancementCommand{mode,id,g};}
        emit({{"type","enhancementState"},{"requested",mode},{"applied",0},
            {"reason","Selected; awaiting a fresh video frame."},{"processingMs",0},{"generation",g},{"id",id}});
    }
    void effects(bool enabled,bool reduced) {std::lock_guard lock(controls_.gate);controls_.enabled=enabled;controls_.reduced=reduced;}
    void react(int kind,long long g) {std::lock_guard lock(controls_.gate);if(permitted_&&g==generation_)controls_.reaction=ReactionCommand{kind,g,GetTickCount64()};}
    void configure(const json& c) {std::lock_guard lock(controls_.gate);controls_.mirror=c.value("outputMirror",false);controls_.fill=c.value("framing",std::string("fit"))=="fill";}
#ifdef CAMBER_MEDIA_TEST
    void SetCompositorHookForTest(std::function<void()> hook){std::lock_guard lock(controls_.gate);controls_.testHook=std::move(hook);}
#endif
    void offer(const json& message) {
        if(!permitted_||message.at("generation").get<long long>()!=generation_) return;
        stage("offer-received");
        closePeer();RTCConfiguration config{};config.srtp_type=MediaSecurityType::kDTLS_SRTP;config.offer_to_receive_audio=false;config.offer_to_receive_video=true;config.local_video_bandwidth=12000;
        peer_=factory_->Create(config,RTCMediaConstraints::Create());if(!peer_) throw std::runtime_error("Could not create receiver");
        peer_->RegisterRTCPeerConnectionObserver(this);const auto g=generation_.load();
        peer_->SetRemoteDescription(message.at("sdp").get<std::string>().c_str(),"offer",[this,g] {
            if(g!=generation_||!permitted_) return;
            stage("remote-description-set");
            peer_->CreateAnswer([this,g](const string sdp,const string type) {
                if(g!=generation_||!permitted_) return;auto text=sdp.std_string();
                stage("answer-created");
                if(type.std_string()!="answer") {emit({{"type","error"},{"message","Unexpected video description type"}});return;}
                peer_->SetLocalDescription(sdp,type,[this,g,text] {stage("local-description-set");emit({{"type","answer"},{"sdp",text},{"generation",g}});},[](const char*) {emit({{"type","error"},{"message","Local description failed"}});});
            },[](const char*) {emit({{"type","error"},{"message","Answer creation failed"}});},RTCMediaConstraints::Create());
        },[](const char*) {emit({{"type","error"},{"message","Invalid video offer"}});});
    }
    void candidate(const json& j) {if(peer_&&permitted_&&j.at("generation").get<long long>()==generation_) peer_->AddCandidate(j.value("sdpMid",std::string("0")).c_str(),j.value("sdpMLineIndex",0),j.at("candidate").get<std::string>().c_str());}
    void OnFrame(scoped_refptr<RTCVideoFrame> frame) override {
        if(!sawDecodedFrame_.exchange(true)) stage("decoded-frame");
        if(!permitted_)return;
        if(GetTickCount64()-controlTick_.load()>6000) {stop();return;}
        const int w=frame->width(),h=frame->height();
        if(!camber::ValidCaptureDimensions(w,h)||frame->StrideY()<w||frame->StrideU()<w/2||frame->StrideV()<w/2)return;
        const auto rotation=int(frame->rotation());
        if(rotation!=0 && rotation!=90 && rotation!=180 && rotation!=270)return;
        {std::lock_guard lock(statistics_->gate);statistics_->width=w;statistics_->height=h;statistics_->rotation=rotation;}
        Pending next{{},w,h,unsigned(rotation),generation_.load(),GetTickCount64()};
        next.pixels.resize(size_t(w)*h*3/2);auto* dst=next.pixels.data();
        for(int y=0;y<h;y++)memcpy(dst+size_t(y)*w,frame->DataY()+size_t(y)*frame->StrideY(),w);
        for(int y=0;y<h/2;y++)for(int x=0;x<w/2;x++){dst[size_t(w)*h+size_t(y)*w+x*2]=frame->DataU()[size_t(y)*frame->StrideU()+x];dst[size_t(w)*h+size_t(y)*w+x*2+1]=frame->DataV()[size_t(y)*frame->StrideV()+x];}
        {std::lock_guard lock(pendingGate_);if(exiting_||!permitted_)return;pending_=std::move(next);}
        pendingCv_.notify_one();
    }

    void OnTrack(scoped_refptr<RTCRtpTransceiver> t) override {if(t->media_type()!=RTCMediaType::VIDEO) return;stage("video-track-received");auto r=t->receiver();r->SetJitterBufferMinimumDelay(.02);if(video_) video_->RemoveRenderer(this);video_=static_cast<RTCVideoTrack*>(r->track().get());video_->AddRenderer(this);}
    void OnIceCandidate(scoped_refptr<RTCIceCandidate> c) override {emit({{"type","candidate"},{"candidate",c->candidate().std_string()},{"sdpMid",c->sdp_mid().std_string()},{"sdpMLineIndex",c->sdp_mline_index()},{"generation",generation_.load()}});}
    void OnPeerConnectionState(RTCPeerConnectionState s) override {if(s==RTCPeerConnectionStateConnected) stage("peer-connected");if(s==RTCPeerConnectionStateConnecting) stage("peer-connecting");if(s==RTCPeerConnectionStateFailed) {stop();emit({{"type","error"},{"message","Direct video connection failed. Check local Wi-Fi and firewall."}});}}
    void OnSignalingState(RTCSignalingState) override {}
    void OnIceGatheringState(RTCIceGatheringState) override {}
    void OnIceConnectionState(RTCIceConnectionState s) override {if(s==RTCIceConnectionStateChecking) stage("ice-checking");if(s==RTCIceConnectionStateConnected||s==RTCIceConnectionStateCompleted) stage("ice-connected");}
    void OnAddStream(scoped_refptr<RTCMediaStream>) override {}
    void OnRemoveStream(scoped_refptr<RTCMediaStream>) override {}
    void OnDataChannel(scoped_refptr<RTCDataChannel>) override {}
    void OnRenegotiationNeeded() override {}
    void OnAddTrack(vector<scoped_refptr<RTCMediaStream>>,scoped_refptr<RTCRtpReceiver>) override {}
    void OnRemoveTrack(scoped_refptr<RTCRtpReceiver>) override {}
};
#ifndef CAMBER_MEDIA_TEST
int wmain(int argc,wchar_t** argv) {
    try {
        if(argc==2 && std::wstring(argv[1])==L"--probe") {
            if(!LibWebRTC::Initialize()) return 1;
            auto factory=LibWebRTC::CreateRTCPeerConnectionFactory();if(!factory->Initialize()) return 1;
            auto codecs=factory->GetRtpReceiverCapabilities(RTCMediaType::VIDEO)->codecs();
            json names=json::array();bool h264=false;
            for(size_t i=0;i<codecs.size();i++) {auto name=codecs[i]->mime_type().std_string();names.push_back(name);if(name=="video/H264") h264=true;}
            emit({{"type","probe"},{"h264",h264},{"codecs",names}});
            factory->Terminate();factory=nullptr;LibWebRTC::Terminate();return h264?0:1;
        }
        if(argc<2) throw std::runtime_error("User SID required");
        const bool removeCurrent=std::wstring(argv[1])==L"--remove-current";
        std::wstring sid;
        if(removeCurrent) {
            HANDLE token=nullptr; if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)) throw std::runtime_error("Token unavailable");
            DWORD size=0;GetTokenInformation(token,TokenUser,nullptr,0,&size);std::vector<unsigned char> data(size);
            if(!GetTokenInformation(token,TokenUser,data.data(),size,&size)) {CloseHandle(token);throw std::runtime_error("Identity unavailable");}
            LPWSTR text=nullptr;ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(data.data())->User.Sid,&text);sid=text;LocalFree(text);CloseHandle(token);
        } else {PSID parsed=nullptr;if(!ConvertStringSidToSidW(argv[1],&parsed)) throw std::runtime_error("Invalid identity");LocalFree(parsed);sid=argv[1];}
        if(sid!=camber::CurrentUserSid())throw std::runtime_error("Camera owner must match the process user");
        std::wstring name=L"Global\\Camber."+sid;check(CoInitializeEx(nullptr,COINIT_MULTITHREADED),"Initialize Windows COM");check(MFStartup(MF_VERSION),"Initialize Media Foundation");
        ComPtr<IMFVirtualCamera> camera;check(MFCreateVirtualCamera(MFVirtualCameraType_SoftwareCameraSource,MFVirtualCameraLifetime_System,MFVirtualCameraAccess_CurrentUser,kCameraFriendlyName,MYCAM_VCAM_CLSID_STRING,nullptr,0,&camera),"Create virtual camera");
        if(removeCurrent||(argc>2&&std::wstring(argv[2])==L"--remove")) {check(camera->Remove());return 0;}
        check(camera->SetString(kMappingAttribute,name.c_str()),"Configure frame mapping");check(camera->SetString(kOwnerAttribute,sid.c_str()),"Configure camera owner");check(camera->Start(nullptr),"Start virtual camera");
        {Receiver receiver(name);emit({{"type","ready"}});std::string line;while(std::getline(std::cin,line)) {
            if(line.size()>65536) break;auto j=json::parse(line);auto type=j.at("type").get<std::string>();receiver.renew();
            if(type=="permit") {receiver.permit(j.at("generation"));receiver.authorizeCamera(camera.Get());}else if(type=="stop") receiver.revoke(j.at("generation"));else if(type=="offer") receiver.offer(j);else if(type=="candidate") receiver.candidate(j);else if(type=="configure") receiver.configure(j.at("configuration"));
            else if(type=="lease") receiver.authorizeCamera(camera.Get());
            else if(type=="enhance") receiver.enhance(j.at("mode"),j.value("id",std::string()),j.at("generation"));
            else if(type=="effects") receiver.effects(j.at("enabled"),j.value("reducedMotion",false));
            else if(type=="reaction") receiver.react(j.at("reaction"),j.at("generation"));
        }}
        check(camera->Stop());camera.Reset();MFShutdown();CoUninitialize();return 0;
    } catch(const WindowsFailure& failure) {
        std::ostringstream code;code<<"0x"<<std::uppercase<<std::hex<<std::setfill('0')<<std::setw(8)<<static_cast<unsigned>(failure.code);
        emit({{"type","error"},{"message",failure.operation+" failed ("+code.str()+")."},{"code",code.str()},{"stage",failure.operation}});return 1;
    } catch(const std::exception&) {emit({{"type","error"},{"message","Native camera initialization failed."}});return 1;}
}
#endif
