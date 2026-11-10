cbuffer Settings : register(b0) { float2 Texel; uint Mode; uint Plane; }
Texture2D<float4> Input : register(t0);
SamplerState ClampSampler : register(s0);
struct Vertex { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
Vertex FullScreenVS(uint index : SV_VertexID) {
    Vertex v;v.uv=float2((index<<1)&2,index&2);
    v.position=float4(v.uv*float2(2,-2)+float2(-1,1),0,1);return v;
}
float4 EnhancePS(Vertex v) : SV_TARGET {
    float4 center=Input.SampleLevel(ClampSampler,v.uv,0),sum=0;
    float weights=0;
    float threshold=Mode==1 ? 0.045 : 0.085;
    [unroll] for(int y=-1;y<=1;y++) [unroll] for(int x=-1;x<=1;x++) {
        float4 n=Input.SampleLevel(ClampSampler,v.uv+float2(x,y)*Texel,0);
        float distance=Plane==0 ? abs(n.r-center.r) : length(n.rg-center.rg);
        float weight=(x==0?2:1)*(y==0?2:1)*max(0.001,1-distance/threshold);
        sum+=n*weight;weights+=weight;
    }
    float4 blur=sum/weights;
    if(Plane==1) return float4(lerp(center.rg,blur.rg,Mode==1?0:0.45),0,1);
    float yValue=lerp(center.r,blur.r,Mode==1?0.22:0.58);
    if(Mode==1) {
        yValue+=clamp((center.r-blur.r)*0.12,-4.0/255,4.0/255);
        // Bounded contrast, with legal SDR endpoints preserved. Never clamp
        // out-of-range source values into the legal range or reinterpret UV.
        if(yValue>=16.0/255 && yValue<=235.0/255) {
            float l=(yValue-16.0/255)/(219.0/255);
            yValue+=sin(l*6.2831853)*(-0.0015);
        }
    } else if(yValue>16.0/255 && yValue<235.0/255) {
        float l=(yValue-16.0/255)/(219.0/255);
        float bright=pow(l,0.90)*(219.0/255)+16.0/255;
        yValue=min(bright,yValue+12.0/255);
    }
    return float4(saturate(yValue),0,0,1);
}
