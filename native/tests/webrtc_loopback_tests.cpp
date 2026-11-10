// Real H.264 encode/transport/decode test using generated pixels only.
// Includes the production receiver so this catches its signaling and frame handoff.
#define CAMBER_MEDIA_TEST
#include "../media/main.cpp"
#include <chrono>
#include <deque>
#include <thread>
std::mutex eventGate;
std::deque<json> receivedEvents, senderEvents;
void emit(const json& data) {std::lock_guard<std::mutex> lock(eventGate);receivedEvents.push_back(data);}
void senderEvent(json data) {std::lock_guard<std::mutex> lock(eventGate);senderEvents.push_back(std::move(data));}
class SenderObserver final : public RTCPeerConnectionObserver {
public:
    void OnIceCandidate(scoped_refptr<RTCIceCandidate> c) override {senderEvent({{"type","candidate"},{"candidate",c->candidate().std_string()},{"sdpMid",c->sdp_mid().std_string()},{"sdpMLineIndex",c->sdp_mline_index()},{"generation",1}});}
    void OnPeerConnectionState(RTCPeerConnectionState) override {}
    void OnSignalingState(RTCSignalingState) override {}
    void OnIceGatheringState(RTCIceGatheringState) override {}
    void OnIceConnectionState(RTCIceConnectionState) override {}
    void OnAddStream(scoped_refptr<RTCMediaStream>) override {}
    void OnRemoveStream(scoped_refptr<RTCMediaStream>) override {}
    void OnDataChannel(scoped_refptr<RTCDataChannel>) override {}
    void OnRenegotiationNeeded() override {}
    void OnTrack(scoped_refptr<RTCRtpTransceiver>) override {}
    void OnAddTrack(vector<scoped_refptr<RTCMediaStream>>,scoped_refptr<RTCRtpReceiver>) override {}
    void OnRemoveTrack(scoped_refptr<RTCRtpReceiver>) override {}
};
int main() {
    CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    const auto name=L"Local\\Camber.GeneratedVideoTest."+std::to_wstring(GetCurrentProcessId());
    const auto acl=std::wstring(L"D:P(A;;GA;;;SY)(A;;GA;;;LS)(A;;GA;;;")+camber::CurrentUserSid()+L")";
    PSECURITY_DESCRIPTOR descriptor=nullptr;
    if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(acl.c_str(),SDDL_REVISION_1,&descriptor,nullptr))return 2;
    SECURITY_ATTRIBUTES security{sizeof(security),descriptor,FALSE};
    HANDLE map=CreateFileMappingW(INVALID_HANDLE_VALUE,&security,PAGE_READWRITE,0,kSharedBytes,name.c_str());
    HANDLE frameMutex=CreateMutexW(&security,FALSE,(name+L".lock").c_str());LocalFree(descriptor);
    auto* view=MapViewOfFile(map,FILE_MAP_ALL_ACCESS,0,0,kSharedBytes);
    if(!map||!frameMutex||!view) return 2;
    auto* header=static_cast<SharedHeader*>(view);header->magic=kSharedMagic;header->version=kSharedVersion;
    bool delivered=false;
    int phase=0;LONG phaseSequence=0;
    std::atomic<bool> remoteReady{false};
    std::string failure;
    {
        Receiver receiver(name);receiver.permit(1);
        auto factory=LibWebRTC::CreateRTCPeerConnectionFactory();factory->Initialize();
        auto source=factory->CreateCustomVideoSource("generated-pattern",RTCMediaConstraints::Create());
        auto track=factory->CreateVideoTrack(source,"generated-video");
        RTCConfiguration config{};config.offer_to_receive_audio=false;config.offer_to_receive_video=false;
        auto peer=factory->Create(config,RTCMediaConstraints::Create());
        SenderObserver observer;peer->RegisterRTCPeerConnectionObserver(&observer);
        peer->AddTrack(track,vector<string>(std::vector<string>{string("test")}));
        std::vector<scoped_refptr<RTCRtpCodecCapability>> codecs;
        auto all=factory->GetRtpSenderCapabilities(RTCMediaType::VIDEO)->codecs();
        for(size_t i=0;i<all.size();i++) if(all[i]->mime_type().std_string()=="video/H264") codecs.push_back(all[i]);
        if(codecs.empty()) return 3;
        peer->transceivers()[0]->SetCodecPreferences(codecs);
        peer->CreateOffer([peer](const string sdp,const string type) {
            const auto text=sdp.std_string();
            peer->SetLocalDescription(sdp,type,[text] {senderEvent({{"type","offer"},{"sdp",text},{"generation",1}});},[](const char* error){senderEvent({{"type","error"},{"message",error}});});
        },[](const char* error){senderEvent({{"type","error"},{"message",error}});},RTCMediaConstraints::Create());
        std::vector<uint8_t> pixels(1280*720*3/2,128);
        std::fill(pixels.begin(),pixels.begin()+1280*720,180);
        std::deque<json> pendingCandidates;
        const auto start=GetTickCount64();
        while(GetTickCount64()-start<15000 && failure.empty()) {
            std::deque<json> fromReceiver,fromSender;
            {std::lock_guard<std::mutex> lock(eventGate);fromReceiver.swap(receivedEvents);fromSender.swap(senderEvents);}
            for(const auto& event:fromSender) {
                const auto type=event.at("type").get<std::string>();
                if(type=="offer") receiver.offer(event);
                else if(type=="candidate") receiver.candidate(event);
                else if(type=="error") failure=event.at("message");
            }
            for(const auto& event:fromReceiver) {
                const auto type=event.at("type").get<std::string>();
                if(type=="answer") peer->SetRemoteDescription(event.at("sdp").get<std::string>(),"answer",[&remoteReady]{remoteReady=true;},[](const char* error){senderEvent({{"type","error"},{"message",error}});});
                else if(type=="candidate") pendingCandidates.push_back(event);
                else if(type=="error") failure=event.at("message");
                else if(type=="videoStatus") std::cout<<"stage: "<<event.at("stage").get<std::string>()<<"\n";
            }
            if(remoteReady) while(!pendingCandidates.empty()) {
                auto event=pendingCandidates.front();pendingCandidates.pop_front();
                peer->AddCandidate(event.at("sdpMid").get<std::string>(),event.at("sdpMLineIndex"),event.at("candidate").get<std::string>());
            }
            auto frame=RTCVideoFrame::Create(1280,720,pixels.data(),1280,pixels.data()+1280*720,640,pixels.data()+1280*720*5/4,640);
            source->OnCapturedFrame(frame);
            bool plain=false,animated=false;LONG sequence=0;
            if(WaitForSingleObject(frameMutex,100)==WAIT_OBJECT_0) {
                // WebRTC may adapt resolution during startup. Verify valid 16:9
                // output and known bright pixels rather than requiring its input size.
                const auto w=header->width,h=header->height;
                const bool valid=header->enabled && header->seq>=3 && w>0 && w<=1280 && h>0 && h<=720 && w*720==h*1280;
                if(valid) {
                    const auto minimum=*std::min_element(FrameData(view),FrameData(view)+size_t(w)*h);
                    if(!camber::NonzeroProof(header->ownerProofDigest)){failure="Live frame lacks producer proof digest";ReleaseMutex(frameMutex);break;}
                    plain=minimum>160;animated=minimum<150;sequence=header->seq;
                }
                ReleaseMutex(frameMutex);
            }
            // Exercise the complete receiver path, not just the drawing helper.
            if(phase==0&&plain) {receiver.react(2,1);phase=1;phaseSequence=sequence;}
            else if(phase==1&&animated&&sequence>phaseSequence+2) {receiver.effects(false,false);phase=2;phaseSequence=sequence;}
            else if(phase==2&&plain&&sequence>phaseSequence+2) {receiver.effects(true,false);receiver.react(2,0);phase=3;phaseSequence=sequence;}
            else if(phase==3&&plain&&sequence>phaseSequence+2) delivered=true;
            if(delivered) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }
        if(delivered) {
            std::this_thread::sleep_for(std::chrono::milliseconds(6200));
            for(int attempt=0;attempt<10 && header->enabled;attempt++) {
                auto frame=RTCVideoFrame::Create(1280,720,pixels.data(),1280,pixels.data()+1280*720,640,pixels.data()+1280*720*5/4,640);
                source->OnCapturedFrame(frame);std::this_thread::sleep_for(std::chrono::milliseconds(60));
            }
            if(header->enabled) {failure="Native receiver did not expire host heartbeat";delivered=false;}
        }
        // Simulate a blocked GPU/compositor while exercising the real command
        // handlers. Setting changes and privacy cutoff must not wait for it.
        if(delivered) {
            receiver.permit(2);receiver.renew();
            auto direct=RTCVideoFrame::Create(720,1280,pixels.data(),720,pixels.data()+1280*720,360,pixels.data()+1280*720*5/4,360);
            receiver.OnFrame(direct);
            auto until=GetTickCount64()+1000;while(!header->enabled && GetTickCount64()<until)std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if(!header->enabled){failure="Test compositor did not publish";delivered=false;}
            struct Stall {std::atomic<bool> blocked=false,release=false;};auto stall=std::make_shared<Stall>();
            receiver.SetCompositorHookForTest([stall]{stall->blocked=true;auto deadline=GetTickCount64()+2000;while(!stall->release && GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));});
            receiver.OnFrame(direct);until=GetTickCount64()+1000;
            while(!stall->blocked && GetTickCount64()<until)std::this_thread::sleep_for(std::chrono::milliseconds(1));
            const auto before=GetTickCount64();
            receiver.configure({{"outputMirror",false},{"framing","fit"}});receiver.effects(true,false);
            receiver.enhance(1,"blocked-graphics-test",2);receiver.react(2,2);receiver.revoke(3);
            if(!stall->blocked || header->enabled || GetTickCount64()-before>100){failure="Slow compositor delayed settings or privacy cutoff";delivered=false;}
            stall->release=true;receiver.SetCompositorHookForTest({});
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
        receiver.revoke(3);
        if(header->enabled || camber::NonzeroProof(header->ownerProofDigest)) {failure="Privacy stop did not clear output and proof";delivered=false;}
        peer->DeRegisterRTCPeerConnectionObserver();peer->Close();factory->Delete(peer);peer=nullptr;track=nullptr;source=nullptr;factory->Terminate();factory=nullptr;
    }
    UnmapViewOfFile(view);CloseHandle(map);CloseHandle(frameMutex);CoUninitialize();
    if(!delivered) {std::cerr<<"FAIL H.264 generated video was not delivered: "<<failure<<"\n";return 1;}
    std::cout<<"PASS generated H.264 pixels, reaction composition, effects-off clearing, stale reaction rejection, blocked-compositor Stop and settings handoff. No physical camera was opened.\n";return 0;
}
