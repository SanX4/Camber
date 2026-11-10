package camera.camber

/** Counter resets and missing stages produce unknown rates, never invented zero timings. */
class VideoCounterWindow {
    private var previous=emptyMap<String,Double>()
    private var at=0.0
    private var stream=""
    fun reset(){previous=emptyMap();at=0.0;stream=""}
    fun sample(id:String,time:Double,counters:Map<String,Double>):Map<String,Double> {
        val current=counters.filter {it.key in setOf("bytesSent","framesEncoded","totalEncodeTime") && it.value.isFinite() && it.value>=0}
        val seconds=if(id==stream && time.isFinite())time-at else 0.0
        fun delta(key:String):Double? {
            val old=previous[key]?:return null
            val value=current[key]?:return null
            return (value-old).takeIf {seconds>0 && it>=0}
        }
        val encoded=delta("framesEncoded")
        val values=mapOf("bitrateBps" to delta("bytesSent")?.times(8)?.div(seconds),
            "encodedFps" to encoded?.div(seconds),
            "encodeMs" to if(encoded!=null && encoded>0)delta("totalEncodeTime")?.times(1000)?.div(encoded) else null)
        previous=current;at=time;stream=id
        return values.mapNotNull {(key,value)->value?.takeIf {it.isFinite() && it>=0}?.let {key to it}}.toMap()
    }
}
