#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "processing_budget.h"
namespace camber {
class ImageEnhancement final {
    class Impl;
    std::unique_ptr<Impl> impl_;
    int requested_=0,applied_=0;
    bool failed_=false,allowGpu_=true;
    ProcessingBudget budget_;
    double milliseconds_=0;
    std::string reason_="No additional image filters.";
public:
    explicit ImageEnhancement(bool allowGpu=true);
    ~ImageEnhancement();
    ImageEnhancement(const ImageEnhancement&)=delete;
    ImageEnhancement& operator=(const ImageEnhancement&)=delete;
    void Request(int mode);
    void NewSession();
    void Inactive(){applied_=0;milliseconds_=0;reason_="Selected; applies when video arrives.";}
    void Process(uint8_t* nv12,int width,int height);
    int Requested()const{return requested_;}
    int Applied()const{return applied_;}
    bool FallenBack()const{return requested_!=0 && failed_;}
    double Milliseconds()const{return milliseconds_;}
    const std::string& Reason()const{return reason_;}
};
}
