package camera.camber
/** Sensor angles are clockwise from the device's natural orientation. */
class OrientationPolicy {
    private var current = 0
    private var candidate: Int? = null
    private var since = 0L
    @Synchronized fun observe(angle: Int, now: Long) {
        if(angle !in 0..359) { candidate=null; return }
        val difference = kotlin.math.abs(angle-current).let {minOf(it,360-it)}
        val quarter = (((angle+45)/90)*90)%360
        if(quarter==current || difference<55) {candidate=null;return}
        if(candidate!=quarter) {candidate=quarter;since=now}
    }
    @Synchronized fun angle(now: Long): Int {
        if(candidate!=null && now-since>=200) {current=candidate!!;candidate=null}
        return current
    }
    fun frameRotation(sensor: Int, front: Boolean, now: Long): Int {
        val physical=angle(now)
        // Same buffer convention as the pinned WebRTC Camera2Session. Its
        // display angle is counterclockwise; our physical angle is clockwise.
        return ((sensor + if(front) -physical else physical)%360+360)%360
    }
}
