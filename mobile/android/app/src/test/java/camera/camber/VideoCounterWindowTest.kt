package camera.camber
import org.junit.Assert.*
import org.junit.Test
class VideoCounterWindowTest {
    private fun counters(bytes:Double=1000.0,frames:Double=30.0,seconds:Double=.3)=mapOf("bytesSent" to bytes,"framesEncoded" to frames,"totalEncodeTime" to seconds)
    @Test fun measuredRatesAndAllowlist() {
        val s=VideoCounterWindow();assertTrue(s.sample("a",1.0,counters()).isEmpty())
        val result=s.sample("a",2.0,counters(3000.0,60.0,.6)+("address" to 123.0))
        assertEquals(16000.0,result.getValue("bitrateBps"),.001)
        assertEquals(30.0,result.getValue("encodedFps"),.001)
        assertEquals(10.0,result.getValue("encodeMs"),.001)
        assertFalse(result.containsKey("address"))
    }
    @Test fun resetsAndOutOfOrderSamplesStayUnknown() {
        val s=VideoCounterWindow();s.sample("a",1.0,counters())
        assertTrue(s.sample("b",2.0,counters()).isEmpty())
        assertTrue(s.sample("b",1.5,counters()).isEmpty())
        assertTrue(s.sample("b",3.0,counters(0.0,0.0,0.0)).isEmpty())
        s.reset();assertTrue(s.sample("b",4.0,counters()).isEmpty())
    }
    @Test fun missingOrInvalidTimingIsNotZero() {
        val s=VideoCounterWindow();s.sample("a",1.0,counters())
        val result=s.sample("a",2.0,mapOf("bytesSent" to 2000.0,"framesEncoded" to 60.0,"totalEncodeTime" to Double.NaN))
        assertFalse(result.containsKey("encodeMs"));assertEquals(8000.0,result.getValue("bitrateBps"),.001)
    }
}
