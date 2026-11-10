package camera.camber

import org.webrtc.RTCStatsReport
import org.webrtc.RTCStats
import org.json.JSONObject

/** Numeric allowlist only. Never serialize raw RTC reports, ICE addresses or IDs. */
class VideoStatistics {
    private val counters=VideoCounterWindow()
    fun reset(){counters.reset()}
    fun sample(report:RTCStatsReport):JSONObject {
        fun number(m:Map<String,Any>,key:String):Double? = (m[key] as? Number)?.toDouble()?.takeIf {it.isFinite() && it>=0}
        fun video(s:RTCStats)=s.members["kind"]=="video" || s.members["mediaType"]=="video"
        val out=report.statsMap.values.firstOrNull {it.type=="outbound-rtp" && video(it)} ?: return JSONObject()
        val remote=report.statsMap.values.firstOrNull {it.type=="remote-inbound-rtp" && video(it)}
        val pairId=report.statsMap.values.firstOrNull {it.type=="transport"}?.members?.get("selectedCandidatePairId") as? String
        val pair=report.statsMap[pairId]
        val current=mutableMapOf<String,Double>()
        for(key in listOf("bytesSent","framesEncoded","totalEncodeTime"))number(out.members,key)?.let {current[key]=it}
        val result=JSONObject()
        fun add(key:String,value:Double?){if(value!=null && value.isFinite() && value>=0)result.put(key,value)}
        for((key,value) in counters.sample(out.id,out.timestampUs/1_000_000.0,current))add(key,value)
        add("networkRttMs",remote?.let {number(it.members,"roundTripTime")}?.times(1000.0)
            ?:pair?.let {number(it.members,"currentRoundTripTime")}?.times(1000.0))
        add("lossRatio",remote?.let {number(it.members,"fractionLost")}?.takeIf {it<=1})
        add("encodedWidth",number(out.members,"frameWidth"));add("encodedHeight",number(out.members,"frameHeight"))
        return result
    }
}
