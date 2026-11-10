package camera.camber
import org.junit.Assert.*
import org.junit.Test

class GesturePolicyTest {
    private fun hand(name: String, score: Float = .95f) = HandPose(name, score, emptyList())
    @Test fun mapsSingleAndTwoHandPoses() {
        val p = GesturePolicy()
        assertEquals("thumbsUp", p.classify(listOf(hand("Thumb_Up"))))
        assertEquals("thumbsDown", p.classify(listOf(hand("Thumb_Down"))))
        assertEquals("balloons", p.classify(listOf(hand("Victory"))))
        assertEquals("fireworks", p.classify(listOf(hand("Thumb_Up"), hand("Thumb_Up"))))
        assertEquals("rain", p.classify(listOf(hand("Thumb_Down"), hand("Thumb_Down"))))
        assertEquals("confetti", p.classify(listOf(hand("Victory"), hand("Victory"))))
        assertNull(p.classify(listOf(hand("Victory", .3f))))
        assertEquals("balloons",p.classify(listOf(hand("Victory"), hand("None"))))
    }
    private fun run(p: GesturePolicy, hands: List<HandPose>, start: Long, end: Long): List<String> =
        (start..end step 100).mapNotNull { p.observe(hands,it) }
    @Test fun requiresHoldReleaseAndCooldown() {
        val p=GesturePolicy();val hands=listOf(hand("Thumb_Up"))
        assertTrue(run(p,hands,0,500).isEmpty())
        assertEquals("thumbsUp",p.observe(hands,600))
        assertTrue(run(p,hands,700,5000).isEmpty())
        run(p,emptyList(),5100,5500)
        assertEquals(listOf("thumbsUp"),run(p,hands,5600,6300))
    }
    @Test fun stopClearsPendingPose() {
        val p=GesturePolicy();val hands=listOf(hand("Victory"))
        run(p,hands,0,400);p.reset()
        assertTrue(run(p,hands,500,1000).isEmpty())
        assertEquals("balloons",p.observe(hands,1100))
    }
    @Test fun trackingGapDoesNotCountAsHolding() {
        val p=GesturePolicy();val hands=listOf(hand("Victory"))
        run(p,hands,0,300)
        assertNull(p.observe(hands,2000))
        assertTrue(run(p,hands,2100,2500).isEmpty())
        assertEquals("balloons",p.observe(hands,2600))
    }
    @Test fun changedPoseRearmsWithoutDroppingBothHands() {
        val p=GesturePolicy()
        assertEquals(listOf("thumbsUp"),run(p,listOf(hand("Thumb_Up")),0,900))
        assertEquals(listOf("balloons"),run(p,listOf(hand("Victory")),1000,4800))
    }
    @Test fun noiseCannotAccumulateIntoAStableGesture() {
        val p=GesturePolicy()
        for(t in 0L..6000L step 100) assertNull(p.observe(listOf(hand(if(t%200==0L) "Victory" else "Thumb_Up")),t))
    }
    @Test fun invalidConfidenceAndCoordinatesAreRejected() {
        val p=GesturePolicy()
        assertNull(p.classify(listOf(hand("Victory",Float.NaN))))
        assertNull(p.classify(listOf(HandPose("Victory",.99f,listOf(HandPoint(Float.NaN,0f))))))
    }
    @Test fun heartRequiresSeparatedPalmsAndJoinedTips() {
        fun points(left: Boolean): List<HandPoint> {
            val wrist = if(left) .25f else .75f
            val values = MutableList(21) { HandPoint(wrist, .5f) }
            values[0] = HandPoint(wrist, .8f); values[9] = HandPoint(wrist, .5f)
            values[5] = HandPoint(wrist, .48f); values[8] = HandPoint(if(left) .48f else .52f, .35f)
            values[4] = HandPoint(if(left) .48f else .52f, .65f)
            return values
        }
        val p=GesturePolicy()
        assertEquals("hearts",p.classify(listOf(HandPose("None",0f,points(true)),HandPose("None",0f,points(false)))))
        assertNull(p.classify(listOf(HandPose("None",0f,points(true)),HandPose("None",0f,points(true)))))
    }
    @Test fun roundedHeartAllowsCloseKnucklesButRejectsOpenOrInvertedHands() {
        fun points(left:Boolean):MutableList<HandPoint> {
            val x=if(left).25f else .75f
            return MutableList(21){HandPoint(x,.5f)}.also {
                it[0]=HandPoint(x,.8f);it[9]=HandPoint(x,.5f)
                it[5]=HandPoint(if(left).45f else .55f,.45f)
                it[8]=HandPoint(if(left).43f else .57f,.35f)
                it[4]=HandPoint(if(left).48f else .52f,.65f)
            }
        }
        fun pose(p:List<HandPoint>)=HandPose("None",0f,p)
        val policy=GesturePolicy();val a=points(true);val b=points(false)
        assertEquals("hearts",policy.classify(listOf(pose(a),pose(b))))
        val apart=b.toMutableList().also { it[8]=HandPoint(.8f,.35f) }
        assertNull(policy.classify(listOf(pose(a),pose(apart))))
        val inverted=listOf(a,b).map {p->pose(p.map {it.copy(y=1-it.y)})}
        assertNull(policy.classify(inverted))
        val overlap=b.toMutableList().also {it[0]=a[0]}
        assertNull(policy.classify(listOf(pose(a),pose(overlap))))
    }
    private fun hornsHand(angle:Double, allOpen:Boolean=false):HandPose {
        val points=MutableList(21){HandPoint(0f,0f,0f)}
        for((mcp,extended) in listOf(5 to true,9 to allOpen,13 to allOpen,17 to true)) {
            val x=(mcp-11)*.02f
            points[mcp]=HandPoint(x,.3f,0f);points[mcp+1]=HandPoint(x,.5f,0f)
            points[mcp+2]=HandPoint(x,if(extended).7f else .35f,0f)
            points[mcp+3]=HandPoint(x,if(extended).9f else .3f,if(extended)0f else .1f)
        }
        val rotated=points.map {HandPoint(it.x,(it.y*kotlin.math.cos(angle)-it.z*kotlin.math.sin(angle)).toFloat(),
            (it.y*kotlin.math.sin(angle)+it.z*kotlin.math.cos(angle)).toFloat())}
        return HandPose("None",0f,points,rotated)
    }
    @Test fun hornsUseJointAnglesAndRemainValidOutOfPlane() {
        val p=GesturePolicy()
        for(angle in listOf(0.0,.5,1.1,1.57)) assertEquals("lasers",p.classify(listOf(hornsHand(angle),hornsHand(-angle))))
        assertNull(p.classify(listOf(hornsHand(0.0,true),hornsHand(0.0,true))))
        assertNull(p.classify(listOf(hornsHand(0.0))))
    }
    @Test fun relaxedHornsAllowNaturalFingerBendButRejectFists() {
        fun bentHorns(pipAngle:Double,dipAngle:Double):HandPose {
            val points=hornsHand(0.0).points.toMutableList()
            for(mcp in listOf(5,17)) {
                val x=points[mcp].x
                val first=(180-pipAngle)*kotlin.math.PI/180
                val second=first+(180-dipAngle)*kotlin.math.PI/180
                points[mcp+2]=HandPoint(x+(.2*kotlin.math.sin(first)).toFloat(),.5f+(.2*kotlin.math.cos(first)).toFloat())
                points[mcp+3]=HandPoint(points[mcp+2].x+(.2*kotlin.math.sin(second)).toFloat(),points[mcp+2].y+(.2*kotlin.math.cos(second)).toFloat())
            }
            return HandPose("None",0f,points,points)
        }
        val p=GesturePolicy();val relaxed=bentHorns(142.0,132.0)
        assertEquals("lasers",p.classify(listOf(relaxed,relaxed)))
        val curled=bentHorns(110.0,110.0)
        assertNull(p.classify(listOf(curled,curled)))
        assertNull(p.classify(listOf(relaxed,hornsHand(0.0,true))))
    }
    @Test fun backwardsClockCannotFinishAnOldHold() {
        val p=GesturePolicy();val hands=listOf(hand("Victory"))
        run(p,hands,1000,1500)
        assertNull(p.observe(hands,100))
        assertTrue(run(p,hands,200,600).isEmpty())
        assertEquals("balloons",p.observe(hands,700))
    }

    @Test fun matchingModerateHandsWinOverSingleReaction() {
        for((category,reaction) in listOf("Thumb_Up" to "fireworks","Thumb_Down" to "rain","Victory" to "confetti","ILoveYou" to "lasers")) {
            val p=GesturePolicy();val hands=listOf(hand(category,.65f),hand(category,.76f))
            assertTrue(run(p,hands,0,700).isEmpty())
            assertEquals(reaction,p.observe(hands,800))
            assertTrue(run(p,hands,900,2200).isEmpty())
        }
    }
    @Test fun tentativeSecondHandCannotPrematurelyTriggerSingle() {
        val p=GesturePolicy();val hands=listOf(hand("Thumb_Up",.52f),hand("Thumb_Up",.9f))
        assertTrue(run(p,hands,0,2000).isEmpty())
        assertEquals("thumbsUp",p.classify(listOf(hand("Thumb_Up",.9f),hand("None",.9f))))
    }
}
