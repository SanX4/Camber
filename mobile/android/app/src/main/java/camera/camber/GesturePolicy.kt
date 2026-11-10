package camera.camber

import kotlin.math.hypot

data class HandPoint(val x: Float, val y: Float, val z: Float = 0f)
data class HandPose(val category: String, val confidence: Float, val points: List<HandPoint>, val world: List<HandPoint> = emptyList())

/** Pure geometry/debounce policy. Never depends on screen mirroring or handedness. */
class GesturePolicy {
    data class Feedback(val pose: String? = null, val progress: Float = 0f, val cooldownMs: Long = 0, val phase: String = "ready")
    var feedback = Feedback(); private set
    var geometryDiagnostic = ""; private set
    private var candidate: String? = null
    private var since = 0L
    private var observations = 0
    private var lastObserved = -1L
    private var lastSeen = -1L
    private var lastTrigger = -10_000L
    private var latchedPose: String? = null
    private var changedSince = -1L
    private data class Track(val id: Int, var point: HandPoint?, var seen: Long, val scores: MutableMap<String, Float>)
    private val tracks = mutableListOf<Track>()
    private var nextTrack = 0
    fun reset() {
        candidate=null;since=0;observations=0;lastObserved=-1;lastSeen=-1;lastTrigger=-10_000
        latchedPose=null;changedSince=-1;tracks.clear();feedback=Feedback()
    }
    fun observe(hands: List<HandPose>, now: Long): String? {
        if(lastObserved>=0 && now<lastObserved) reset()
        val gap=if(lastObserved<0) 0 else now-lastObserved
        if(gap>250) { candidate=null;observations=0;tracks.clear() }
        lastObserved=now
        tracks.removeAll { now-it.seen>350 }
        val used=mutableSetOf<Int>()
        val smooth=hands.filter { it.points.all { p->p.x.isFinite() && p.y.isFinite() } }.mapIndexed { index, hand ->
            val center=hand.points.getOrNull(9)
            val track=tracks.filter { it.id !in used }.minByOrNull {
                if(center!=null && it.point!=null) distance(center,it.point!!) else kotlin.math.abs(it.id-index).toFloat()
            }?.takeIf { center==null || it.point==null || distance(center,it.point!!)<.5f }
                ?: Track(nextTrack++,center,now,mutableMapOf()).also { tracks.add(it) }
            used.add(track.id)
            val alpha=if(track.scores.isEmpty()) 1f else (1-kotlin.math.exp(-(now-track.seen).coerceAtLeast(1)/80.0)).toFloat()
            val keys=track.scores.keys.toSet()+hand.category
            keys.forEach { key -> track.scores[key]=(track.scores[key]?:0f)*(1-alpha)+(if(key==hand.category) hand.confidence else 0f)*alpha }
            track.point=center;track.seen=now
            val best=track.scores.maxByOrNull { it.value }
            hand.copy(category=best?.key?:"None",confidence=best?.value?:0f)
        }
        val reaction=classify(smooth)
        val cooldown=(4000-(now-lastTrigger)).coerceAtLeast(0)
        if(reaction!=latchedPose && latchedPose!=null) {
            if(changedSince<0) changedSince=now
            if(now-changedSince>=350) {latchedPose=null;changedSince=-1}
        } else changedSince=-1
        if(reaction==null) {
            if(lastSeen<0 || now-lastSeen>=350) {candidate=null;observations=0}
            feedback=Feedback(cooldownMs=cooldown,phase=if(cooldown>0) "cooldown" else "ready")
            return null
        }
        lastSeen=now
        if(candidate!=reaction) { candidate=reaction;since=now;observations=0 }
        observations++
        val holdMs=if(reaction in setOf("fireworks","rain","confetti","hearts","lasers"))800f else 600f
        val progress=((now-since)/holdMs).coerceIn(0f,1f)
        feedback=Feedback(reaction,progress,cooldown,if(latchedPose==reaction) "release" else if(cooldown>0) "cooldown" else "holding")
        if(latchedPose==reaction || observations<4 || progress<1f || cooldown>0) return null
        latchedPose=reaction;lastTrigger=now;feedback=Feedback(reaction,1f,4000,"triggered")
        return reaction
    }
    fun classify(hands: List<HandPose>): String? {
        geometryDiagnostic=""
        if(hands.isEmpty() || hands.size>2 || hands.any { h->h.points.any { !it.x.isFinite() || !it.y.isFinite() || !it.z.isFinite() } }) return null
        if (hands.size == 2 && heart(hands[0].points, hands[1].points)) return "hearts"
        val canned=setOf("Thumb_Up","Thumb_Down","Victory","ILoveYou")
        if(hands.size==2 && hands[0].category==hands[1].category && hands[0].category in canned) {
            // Two hands on the reference phone score in the mid-60s even for
            // deliberate matching poses. Require a longer stable hold instead
            // of allowing the stronger hand to trigger a single reaction first.
            if(hands.all {it.confidence.isFinite() && it.confidence>=.60f})return when(hands[0].category) {
                "Thumb_Up"->"fireworks";"Thumb_Down"->"rain";"ILoveYou"->"lasers";else->"confetti"
            }
            if(hands.all {it.confidence.isFinite() && it.confidence>=.45f})return null
        }
        val strong = hands.filter { it.confidence.isFinite() && it.confidence >= .70f }.map { it.category }.filter { it in setOf("Thumb_Up","Thumb_Down","Victory") }
        if (hands.size == 2 && hands.all { horns(if(it.world.size==21) it.world else it.points) }) return "lasers"
        if (strong.count { it == "Thumb_Up" } == 2) return "fireworks"
        if (strong.count { it == "Thumb_Down" } == 2) return "rain"
        if (strong.count { it == "Victory" } == 2) return "confetti"
        // A neutral second hand must not hide a deliberate foreground pose.
        if (strong.size != 1) return null
        return when (strong[0]) { "Thumb_Up" -> "thumbsUp"; "Thumb_Down" -> "thumbsDown"; "Victory" -> "balloons"; else -> null }
    }
    private fun distance(a: HandPoint, b: HandPoint) = hypot(a.x - b.x, a.y - b.y)
    private fun heart(a: List<HandPoint>, b: List<HandPoint>): Boolean {
        if (a.size != 21 || b.size != 21) return false
        val scale = (distance(a[0], a[9]) + distance(b[0], b[9])) / 2
        if (scale < .05f) return false
        val tips = distance(a[8], b[8]) < scale * .6f && distance(a[4], b[4]) < scale * .6f
        val opening = distance(a[8], a[4]) > scale * .55f && distance(b[8], b[4]) > scale * .55f
        // Index knuckles naturally approach each other in a rounded heart.
        // Use wrist separation to reject overlapping hands instead.
        val gap = distance(a[0], b[0]) > scale * 1.1f
        val indexAboveThumb = a[8].y < a[4].y - scale * .1f && b[8].y < b[4].y - scale * .1f
        geometryDiagnostic="heartJoined=$tips; heartOpen=$opening; heartSeparated=$gap; heartUpright=$indexAboveThumb"
        return tips && opening && gap && indexAboveThumb
    }
    private fun horns(p: List<HandPoint>): Boolean {
        if (p.size != 21) return false
        fun angle(a: HandPoint,b: HandPoint,c: HandPoint): Float {
            val ax=a.x-b.x;val ay=a.y-b.y;val az=a.z-b.z
            val cx=c.x-b.x;val cy=c.y-b.y;val cz=c.z-b.z
            val norm=kotlin.math.sqrt((ax*ax+ay*ay+az*az)*(cx*cx+cy*cy+cz*cz))
            if(norm<1e-8f)return 0f
            return (kotlin.math.acos(((ax*cx+ay*cy+az*cz)/norm).coerceIn(-1f,1f))*180/kotlin.math.PI).toFloat()
        }
        // Allow a relaxed index/pinky without accepting an open palm. The
        // middle and ring fingers must still show a definite bend.
        fun extended(mcp:Int,pip:Int,dip:Int,tip:Int)=angle(p[mcp],p[pip],p[dip])>135 && angle(p[pip],p[dip],p[tip])>125
        fun folded(mcp:Int,pip:Int,dip:Int,tip:Int)=angle(p[mcp],p[pip],p[dip])<145 || angle(p[pip],p[dip],p[tip])<135
        return extended(5,6,7,8) && extended(17,18,19,20) && folded(9,10,11,12) && folded(13,14,15,16)

    }
}
