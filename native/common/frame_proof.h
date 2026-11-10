#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>

namespace camber {
using ProofBytes = std::array<uint8_t,32>;
inline bool NonzeroProof(const ProofBytes& value) {
    uint8_t any=0;for(auto byte:value)any|=byte;return any!=0;
}
inline bool ProofDigest(const void* token,size_t bytes,ProofBytes& digest) {
    SecureZeroMemory(digest.data(),digest.size());
    if(!token || bytes!=32)return false;
    ProofBytes copy{};memcpy(copy.data(),token,copy.size());
    const bool valid=NonzeroProof(copy) && BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,
        copy.data(),ULONG(copy.size()),digest.data(),ULONG(digest.size()))>=0;
    SecureZeroMemory(copy.data(),copy.size());
    if(!valid)SecureZeroMemory(digest.data(),digest.size());
    return valid;
}
inline bool SameProof(const ProofBytes& a,const ProofBytes& b) {
    uint8_t difference=0;for(size_t i=0;i<a.size();i++)difference|=a[i]^b[i];
    return NonzeroProof(a) && NonzeroProof(b) && difference==0;
}
class FrameProofKey final {
    ProofBytes token_{},digest_{};
public:
    FrameProofKey()=default;
    FrameProofKey(const FrameProofKey&)=delete;
    FrameProofKey& operator=(const FrameProofKey&)=delete;
    ~FrameProofKey(){Clear();}
    bool Rotate() {
        Clear();
        if(BCryptGenRandom(nullptr,token_.data(),ULONG(token_.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0 ||
            !ProofDigest(token_.data(),token_.size(),digest_)){Clear();return false;}
        return true;
    }
    void Clear(){SecureZeroMemory(token_.data(),token_.size());SecureZeroMemory(digest_.data(),digest_.size());}
    const ProofBytes& Token()const{return token_;}
    const ProofBytes& Digest()const{return digest_;}
};
// Store only a digest in the source; never expose the bearer proof through GET,
// activation attributes, mapped memory, diagnostics or the phone protocol.
class FrameProofGrant final {
    ProofBytes digest_{};
public:
    FrameProofGrant()=default;
    FrameProofGrant(const FrameProofGrant&)=delete;
    FrameProofGrant& operator=(const FrameProofGrant&)=delete;
    ~FrameProofGrant(){Clear();}
    bool Set(const void* token,size_t bytes) {
        ProofBytes next{};
        if(!ProofDigest(token,bytes,next))return false;
        digest_=next;SecureZeroMemory(next.data(),next.size());return true;
    }
    bool Matches(const ProofBytes& digest)const{return SameProof(digest_,digest);}
    void Clear(){SecureZeroMemory(digest_.data(),digest_.size());}
};
}
