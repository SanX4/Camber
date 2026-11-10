package camera.camber
import org.junit.Assert.*
import org.junit.Test
class OrientationPolicyTest {
    @Test fun needsStableAngleAndHasHysteresis() {
        val p=OrientationPolicy();p.observe(46,0);assertEquals(0,p.angle(300))
        p.observe(60,400);assertEquals(0,p.angle(599));assertEquals(90,p.angle(600))
        p.observe(44,700);assertEquals(90,p.angle(1000))
        p.observe(20,1100);assertEquals(0,p.angle(1300))
    }
    @Test fun unknownRetainsLastAndCancelsPending() {
        val p=OrientationPolicy();p.observe(90,0);p.observe(-1,100);assertEquals(0,p.angle(500))
        p.observe(180,600);assertEquals(180,p.angle(800));p.observe(-1,900);assertEquals(180,p.angle(5000))
    }
    @Test fun pinnedWebRtcFrontRearSensorConventions() {
        for(sensor in listOf(0,90,180,270)) for(physical in listOf(0,90,180,270)) {
            val p=OrientationPolicy();p.observe(physical,0)
            assertEquals((sensor+physical)%360,p.frameRotation(sensor,false,250))
            assertEquals((sensor-physical+360)%360,p.frameRotation(sensor,true,250))
        }
    }
}
