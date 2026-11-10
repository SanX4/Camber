package camera.camber

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.animateColorAsState
import androidx.compose.foundation.selection.selectableGroup
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.semantics.*
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.view.WindowCompat

// Graphite surfaces, cool metallic edges, and blue for actions and selected controls.
private val Graphite = darkColorScheme(
    primary=Color(0xFF80B8FF),onPrimary=Color(0xFF071B34),
    primaryContainer=Color(0xFF183552),onPrimaryContainer=Color(0xFFD7E9FF),
    background=Color(0xFF0B0E13),surface=Color(0xFF171C24),
    onBackground=Color(0xFFF0F4FA),onSurface=Color(0xFFF0F4FA),
    surfaceVariant=Color(0xFF242C38),onSurfaceVariant=Color(0xFFB1BDCE),
    outline=Color(0xFF77869C),outlineVariant=Color(0xFF344153),
    secondary=Color(0xFF80B8FF),secondaryContainer=Color(0xFF183552),onSecondaryContainer=Color(0xFFD7E9FF),
    error=Color(0xFFFFB4B4),onError=Color(0xFF591A23))

@Composable fun CamberTheme(content: @Composable ()->Unit) {
    val view=LocalView.current
    SideEffect { (view.context as? android.app.Activity)?.let {
        WindowCompat.getInsetsController(it.window,view).apply {
            isAppearanceLightStatusBars=false;isAppearanceLightNavigationBars=false
        }
    } }
    MaterialTheme(colorScheme=Graphite,shapes=Shapes(small=RoundedCornerShape(16.dp),
        medium=RoundedCornerShape(24.dp),large=RoundedCornerShape(32.dp)),content=content)
}

@OptIn(ExperimentalMaterial3Api::class, ExperimentalLayoutApi::class)
@Composable fun CamberScreen(status: String,sharing: Boolean,connected: Boolean,paused: Boolean,
    working: Boolean,pcName: String?,gestures: Boolean,front: Boolean,switching: Boolean,cameras: Set<Boolean>,gestureFeedback: String,practicing: Boolean,enhancement: String,videoStatus: String,requestedHeight:Int,onQuality:(Int)->Unit,onEnhancement:(String)->Unit,onPractice: ()->Unit,
    onCamera: (Boolean)->Unit,onGestures: (Boolean)->Unit,onStart: ()->Unit,onPause: ()->Unit,
    onStop: ()->Unit,onScan: ()->Unit,onReconnect: ()->Unit,onForget: ()->Unit) {
    val colors=MaterialTheme.colorScheme
    var settings by remember { mutableStateOf(false) }
    var confirmForget by remember { mutableStateOf(false) }
    val title=when {practicing->"Find your expression.";sharing->"You're live.";working->"Connecting…";!connected->"Your phone.\nA better camera.";paused->"Take your time.";else->"Ready when you are."}
    val tone by animateColorAsState(if(sharing) colors.primaryContainer else colors.surfaceVariant,label="connection surface")
    Surface(Modifier.fillMaxSize(),color=colors.background) {
        BoxWithConstraints(Modifier.fillMaxSize().safeDrawingPadding()) {
            Column(Modifier.align(Alignment.TopCenter).widthIn(max=600.dp).fillMaxWidth()
                .verticalScroll(rememberScrollState()).padding(horizontal=if(maxWidth>600.dp) 40.dp else 24.dp,vertical=16.dp),
                verticalArrangement=Arrangement.spacedBy(24.dp)) {
                Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically) {
                    Icon(painterResource(R.drawable.camber_mark),null,tint=colors.primary,modifier=Modifier.size(32.dp))
                    Spacer(Modifier.width(10.dp))
                    Text("camber",fontSize=26.sp,fontWeight=FontWeight.SemiBold,letterSpacing=(-1).sp,modifier=Modifier.weight(1f))
                    TextButton(onClick={settings=true},modifier=Modifier.heightIn(min=48.dp)) {Text("Options")}
                }
                Column(verticalArrangement=Arrangement.spacedBy(12.dp)) {
                    Surface(shape=CircleShape,color=colors.surfaceVariant) {
                        Row(Modifier.padding(horizontal=14.dp,vertical=8.dp),verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.spacedBy(8.dp)) {
                            Box(Modifier.size(7.dp).background(if(sharing) colors.primary else colors.onSurfaceVariant,CircleShape))
                            Text(if(practicing) "LOCAL PRACTICE · CAMERA ON" else if(sharing) "CAMERA SHARING" else if(connected) "PC CONNECTED" else "CAMERA OFF",style=MaterialTheme.typography.labelMedium)
                        }
                    }
                    AnimatedContent(targetState=title,label="session heading") {
                        Text(it,fontSize=34.sp,lineHeight=40.sp,fontWeight=FontWeight.Medium,letterSpacing=(-1).sp)
                    }
                }
                Surface(shape=RoundedCornerShape(32.dp),color=colors.surface,border=BorderStroke(1.dp,colors.outlineVariant.copy(alpha=.65f))) {
                    Column(Modifier.fillMaxWidth().background(Brush.verticalGradient(listOf(tone.copy(alpha=.5f),colors.surface)))
                        .padding(24.dp),horizontalAlignment=Alignment.CenterHorizontally,verticalArrangement=Arrangement.spacedBy(16.dp)) {
                        Box(Modifier.size(72.dp).background(colors.background.copy(alpha=.5f),CircleShape),contentAlignment=Alignment.Center) {
                            LensSymbol(Modifier.size(38.dp),colors.primary,front)
                        }
                        Text(if(connected) pcName ?: "Your Windows PC" else "Your camera stays yours",style=MaterialTheme.typography.titleMedium)
                        Text(status,color=colors.onSurfaceVariant,style=MaterialTheme.typography.bodyMedium,
                            modifier=Modifier.semantics {liveRegion=LiveRegionMode.Polite})
                        Button(onClick=if(!connected) {if(pcName==null) onScan else onReconnect} else if(sharing) onPause else onStart,
                            enabled=!working,shape=CircleShape,modifier=Modifier.fillMaxWidth().heightIn(min=56.dp)) {
                            if(working) {CircularProgressIndicator(Modifier.size(20.dp),strokeWidth=2.dp);Spacer(Modifier.width(12.dp))}
                            Text(if(working) "Connecting" else if(!connected) {if(pcName==null) "Connect a PC" else "Reconnect to PC"}
                                else if(sharing) "Pause camera" else if(paused) "Resume sharing" else "Start sharing",fontSize=16.sp)
                        }
                        if(sharing || paused) TextButton(onClick=onStop,modifier=Modifier.heightIn(min=48.dp),
                            colors=ButtonDefaults.textButtonColors(contentColor=colors.error)) {Text("Stop sharing")}
                    }
                }
                Column(verticalArrangement=Arrangement.spacedBy(12.dp)) {
                    Row(Modifier.fillMaxWidth(),verticalAlignment=Alignment.CenterVertically) {
                        Text("Camera",style=MaterialTheme.typography.titleLarge,modifier=Modifier.weight(1f))
                        Text(if(switching) "Switching…" else if(front) "Front selected" else "Rear selected",
                            color=colors.primary,style=MaterialTheme.typography.labelLarge,
                            modifier=Modifier.semantics {liveRegion=LiveRegionMode.Polite})
                    }
                    Row(Modifier.fillMaxWidth().height(IntrinsicSize.Min).selectableGroup(),horizontalArrangement=Arrangement.spacedBy(12.dp)) {
                        LensTile("Rear camera","The world in view",false,!front,switching,false in cameras,onCamera,Modifier.weight(1f))
                        LensTile("Front camera","You, in focus",true,front,switching,true in cameras,onCamera,Modifier.weight(1f))
                    }
                    Text(if(sharing) "Switch lenses without leaving your call." else "Choose a lens now. It opens only when you start sharing.",
                        style=MaterialTheme.typography.bodyMedium,color=colors.onSurfaceVariant)
                }
                Surface(modifier=Modifier.fillMaxWidth(),shape=RoundedCornerShape(26.dp),color=colors.surface,border=BorderStroke(1.dp,colors.outlineVariant.copy(alpha=.5f))) {
                    Column(Modifier.padding(20.dp),verticalArrangement=Arrangement.spacedBy(12.dp)) {
                        Text("Video quality",style=MaterialTheme.typography.titleMedium)
                        Text("Maximum resolution",style=MaterialTheme.typography.bodyMedium,color=colors.onSurfaceVariant)
                        FlowRow(horizontalArrangement=Arrangement.spacedBy(8.dp),modifier=Modifier.selectableGroup()) {
                            for(height in listOf(720,1080))FilterChip(selected=requestedHeight==height,onClick={onQuality(height)},
                                enabled=sharing && !switching,label={Text("${height}p · 30 fps")},
                                modifier=Modifier.heightIn(min=48.dp).semantics {role=Role.RadioButton})
                        }
                        Text("Improve video",style=MaterialTheme.typography.titleMedium)
                        Text("Processed locally on your PC",style=MaterialTheme.typography.bodyMedium,color=colors.onSurfaceVariant)
                        FlowRow(horizontalArrangement=Arrangement.spacedBy(8.dp),modifier=Modifier.selectableGroup()) {
                            for((value,label) in listOf("original" to "Original","natural" to "Natural","lowLight" to "Low light")) {
                                FilterChip(selected=enhancement==value,onClick={onEnhancement(value)},enabled=connected,
                                    label={Text(label)},modifier=Modifier.heightIn(min=48.dp).semantics {role=Role.RadioButton})
                            }
                        }
                        Text(if(connected)videoStatus else "Connect a PC to choose an image preset.",color=colors.onSurfaceVariant,
                            style=MaterialTheme.typography.bodyMedium,modifier=Modifier.semantics {liveRegion=LiveRegionMode.Polite})
                    }
                }
                Surface(modifier=Modifier.fillMaxWidth(),shape=RoundedCornerShape(26.dp),color=colors.surface,border=BorderStroke(1.dp,colors.outlineVariant.copy(alpha=.5f))) {
                    Column(Modifier.padding(20.dp),verticalArrangement=Arrangement.spacedBy(12.dp)) {
                        Row(verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.spacedBy(16.dp)) {
                            Column(Modifier.weight(1f),verticalArrangement=Arrangement.spacedBy(4.dp)) {
                                Text("Hand reactions",style=MaterialTheme.typography.titleMedium)
                                Text("A little expression",style=MaterialTheme.typography.bodyMedium,color=colors.onSurfaceVariant)
                            }
                            Switch(checked=gestures,onCheckedChange=onGestures,modifier=Modifier.semantics {contentDescription="Hand reactions"})
                        }
                        OutlinedButton(onClick=onPractice,enabled=!sharing && !working,modifier=Modifier.fillMaxWidth().heightIn(min=48.dp)) {Text(if(practicing) "Finish local practice" else "Try gestures locally")}
                        Text(if(practicing || (sharing && gestures)) gestureFeedback else "Hold a pose briefly. Enable hand reactions on Windows too.",color=colors.onSurfaceVariant,style=MaterialTheme.typography.bodyMedium)
                    }
                }
                Text("Camera only. No microphone, recording or cloud video.",color=colors.onSurfaceVariant,
                    style=MaterialTheme.typography.bodySmall,modifier=Modifier.padding(bottom=12.dp))
            }
        }
    }
    if(settings) ModalBottomSheet(onDismissRequest={settings=false},containerColor=colors.surface,shape=RoundedCornerShape(topStart=32.dp,topEnd=32.dp)) {
        Column(Modifier.fillMaxWidth().verticalScroll(rememberScrollState()).padding(horizontal=24.dp).padding(bottom=32.dp),verticalArrangement=Arrangement.spacedBy(12.dp)) {
            Text("Your connection",style=MaterialTheme.typography.headlineSmall)
            Text(pcName ?: "Pair with the QR code shown on Windows.",color=colors.onSurfaceVariant)
            FilledTonalButton(onClick={settings=false;onScan()},shape=CircleShape,modifier=Modifier.fillMaxWidth().heightIn(min=56.dp)) {Text("Scan a new pairing code")}
            if(pcName!=null) {
                OutlinedButton(onClick={settings=false;onReconnect()},enabled=!working,shape=CircleShape,modifier=Modifier.fillMaxWidth().heightIn(min=56.dp)) {Text("Reconnect to PC")}
                TextButton(onClick={confirmForget=true},modifier=Modifier.heightIn(min=48.dp),colors=ButtonDefaults.textButtonColors(contentColor=colors.error)) {Text("Forget this PC")}
            }
            Text("Choose Camber Camera in your meeting app. Keep both devices on the same private Wi-Fi.",color=colors.onSurfaceVariant,style=MaterialTheme.typography.bodyMedium)
        }
    }
    if(confirmForget) AlertDialog(onDismissRequest={confirmForget=false},title={Text("Forget this PC?")},text={Text("You’ll need to scan a new code to connect again.")},
        confirmButton={TextButton(onClick={confirmForget=false;settings=false;onForget()}) {Text("Forget PC")}},dismissButton={TextButton(onClick={confirmForget=false}) {Text("Keep PC")}})
}

@Composable private fun LensTile(title: String,subtitle: String,front: Boolean,selected: Boolean,
    switching: Boolean,available: Boolean,onCamera: (Boolean)->Unit,modifier: Modifier) {
    val colors=MaterialTheme.colorScheme
    val fill by animateColorAsState(if(selected) colors.primaryContainer else colors.surface,label="lens surface")
    val edge by animateColorAsState(if(selected) colors.primary else colors.outlineVariant,label="lens edge")
    Surface(onClick={onCamera(front)},enabled=available && !switching,
        modifier=modifier.fillMaxHeight().semantics {this.selected=selected;role=Role.RadioButton},
        shape=RoundedCornerShape(26.dp),color=fill,border=BorderStroke(1.dp,edge)) {
        Column(Modifier.background(Brush.verticalGradient(listOf(Color.White.copy(alpha=.035f),Color.Transparent)))
            .padding(18.dp),verticalArrangement=Arrangement.spacedBy(12.dp)) {
            Row(Modifier.fillMaxWidth().heightIn(min=48.dp),verticalAlignment=Alignment.CenterVertically) {
                LensSymbol(Modifier.size(30.dp),if(selected) colors.primary else colors.onSurfaceVariant,front)
                Spacer(Modifier.weight(1f))
                if(selected) Canvas(Modifier.size(24.dp).clearAndSetSemantics {}) {
                    val mark=Path().apply {moveTo(size.width*.18f,size.height*.52f);lineTo(size.width*.4f,size.height*.74f);lineTo(size.width*.84f,size.height*.26f)}
                    drawPath(mark,colors.primary,style=Stroke(width=2.dp.toPx(),cap=StrokeCap.Round,join=StrokeJoin.Round))
                }
            }
            Text(title,style=MaterialTheme.typography.titleMedium)
            Text(if(!available) "Unavailable" else subtitle,color=colors.onSurfaceVariant,style=MaterialTheme.typography.bodySmall)
        }
    }
}

@Composable private fun LensSymbol(modifier: Modifier,color: Color,front: Boolean) {
    Canvas(modifier) {
        val stroke=Stroke(width=size.width*.055f)
        drawRoundRect(color,Offset(size.width*.08f,size.height*.19f),Size(size.width*.84f,size.height*.64f),CornerRadius(size.width*.17f),style=stroke)
        if(front) {
            drawCircle(color,size.width*.10f,Offset(size.width*.5f,size.height*.40f),style=stroke)
            drawArc(color,180f,180f,false,Offset(size.width*.31f,size.height*.55f),Size(size.width*.38f,size.height*.25f),style=stroke)
        } else {
            drawCircle(color,size.width*.19f,Offset(size.width*.5f,size.height*.51f),style=stroke)
            drawCircle(color,size.width*.035f,Offset(size.width*.78f,size.height*.34f))
        }
    }
}
