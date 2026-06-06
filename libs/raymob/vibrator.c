#include "raymob.h"

void Vibrate(float seconds)
{
    VibrateMS((uint64_t)(1000 * seconds));
}

void VibrateMS(uint64_t ms)
{
    jobject nativeLoaderInst = GetNativeLoaderInstance();
    JNIEnv* env = AttachCurrentThread();

    jclass nativeLoaderClass = (*env)->GetObjectClass(env, nativeLoaderInst);
    jmethodID getSystemServiceMethod = (*env)->GetMethodID(env, nativeLoaderClass, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");

    jstring vibratorService = (*env)->NewStringUTF(env, "vibrator");
    jobject vibrator = (*env)->CallObjectMethod(env, nativeLoaderInst, getSystemServiceMethod, vibratorService);
    (*env)->DeleteLocalRef(env, vibratorService);

    jclass vibratorClass = (*env)->GetObjectClass(env, vibrator);
    jmethodID hasVibratorMethod = (*env)->GetMethodID(env, vibratorClass, "hasVibrator", "()Z");
    jboolean hasVibrator = (*env)->CallBooleanMethod(env, vibrator, hasVibratorMethod);

    if (hasVibrator) {
        jmethodID vibrateMethod = (*env)->GetMethodID(env, vibratorClass, "vibrate", "(J)V");
        if (vibrateMethod != NULL) {
            (*env)->CallVoidMethod(env, vibrator, vibrateMethod, (jlong)ms);
        }
    }

    DetachCurrentThread();
}

void VibrateEx(float seconds, float intensity)
{
    VibrateExMS((uint64_t)(1000 * seconds), intensity);
}

void VibrateExMS(uint64_t ms, float intensity)
{
    jobject nativeLoaderInst = GetNativeLoaderInstance();
    JNIEnv* env = AttachCurrentThread();

    jclass nativeLoaderClass = (*env)->GetObjectClass(env, nativeLoaderInst);
    jmethodID getSystemServiceMethod = (*env)->GetMethodID(env, nativeLoaderClass, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");

    jstring vibratorService = (*env)->NewStringUTF(env, "vibrator");
    jobject vibrator = (*env)->CallObjectMethod(env, nativeLoaderInst, getSystemServiceMethod, vibratorService);
    (*env)->DeleteLocalRef(env, vibratorService);

    jclass vibratorClass = (*env)->GetObjectClass(env, vibrator);
    jmethodID hasVibratorMethod = (*env)->GetMethodID(env, vibratorClass, "hasVibrator", "()Z");
    jboolean hasVibrator = (*env)->CallBooleanMethod(env, vibrator, hasVibratorMethod);

    if (hasVibrator) {
        jclass vibrationEffectClass = (*env)->FindClass(env, "android/os/VibrationEffect");
        jmethodID createOneShotMethod = (*env)->GetStaticMethodID(env, vibrationEffectClass, "createOneShot", "(JI)Landroid/os/VibrationEffect;");

        int intensityValue = (int)(intensity * 255);
        if (intensityValue > 255) intensityValue = 255;
        if (intensityValue < 1) intensityValue = 1;

        jobject vibrationEffect = (*env)->CallStaticObjectMethod(env, vibrationEffectClass, createOneShotMethod, (jlong)ms, (jint)intensityValue);

        if (vibrationEffect != NULL) {
            jmethodID vibrateMethod = (*env)->GetMethodID(env, vibratorClass, "vibrate", "(Landroid/os/VibrationEffect;)V");
            (*env)->CallVoidMethod(env, vibrator, vibrateMethod, vibrationEffect);
        }
    }

    DetachCurrentThread();
}
