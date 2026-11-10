# Native APIs and callback contracts must survive R8 in the release candidate.
-keep class org.webrtc.** { *; }
# WebRTC m150 bootstraps Chromium JNI Zero by native class/method lookup.
-keep class org.jni_zero.JniZero {
    private static java.lang.Object[] init();
    private static void crashIfMultiplexingMisaligned(long, long);
}
-keep class com.google.mediapipe.** { *; }
# Flogger discovers its enclosing caller from stack frames. Inlining its
# factory during shrinking breaks MediaPipe Graph's static initializer.
-keep class com.google.common.flogger.** { *; }
# Protobuf Lite resolves generated message fields from schema name strings.
# This includes well-known types such as Any outside the MediaPipe namespace.
-keepclassmembers class * extends com.google.protobuf.GeneratedMessageLite {
    <fields>;
}
-keepclasseswithmembernames,includedescriptorclasses class * { native <methods>; }

# Optional profiler/template APIs shipped in MediaPipe core are not used by
# GestureRecognizer. Keep this list exact; never hide unrelated missing types.
-dontwarn com.google.auto.value.extension.memoized.Memoized
-dontwarn com.google.mediapipe.proto.CalculatorProfileProto$CalculatorProfile
-dontwarn com.google.mediapipe.proto.GraphTemplateProto$CalculatorGraphTemplate
