#include "raymob.h"

void KeepScreenOn(bool keepOn)
{
    jobject nativeLoaderInst = GetNativeLoaderInstance();

    if (nativeLoaderInst != NULL) {
        JNIEnv* env = AttachCurrentThread();

        jclass nativeLoaderClass = (*env)->GetObjectClass(env, nativeLoaderInst);
        jfieldID displayManagerField = (*env)->GetFieldID(env, nativeLoaderClass, "displayManager", "Lcom/raylib/android/DisplayManager;");
        jobject displayManager = (*env)->GetObjectField(env, nativeLoaderInst, displayManagerField);

        if (displayManager != NULL) {
            jclass displayManagerClass = (*env)->GetObjectClass(env, displayManager);
            jmethodID method = (*env)->GetMethodID(env, displayManagerClass, "keepScreenOn", "(Z)V");
            (*env)->CallVoidMethod(env, displayManager, method, (jboolean)keepOn);
        }

        DetachCurrentThread();
    }
}

Orientation GetScreenOrientation()
{
    Orientation result = 0;
    jobject nativeLoaderInst = GetNativeLoaderInstance();

    if (nativeLoaderInst != NULL) {
        JNIEnv* env = AttachCurrentThread();

        jclass nativeLoaderClass = (*env)->GetObjectClass(env, nativeLoaderInst);
        jfieldID displayManagerField = (*env)->GetFieldID(env, nativeLoaderClass, "displayManager", "Lcom/raylib/android/DisplayManager;");
        jobject displayManager = (*env)->GetObjectField(env, nativeLoaderInst, displayManagerField);

        if (displayManager != NULL) {
            jclass displayManagerClass = (*env)->GetObjectClass(env, displayManager);
            jmethodID screenOrientationMethod = (*env)->GetMethodID(env, displayManagerClass, "getOrientation", "()I");
            jint screenOrientation = (*env)->CallIntMethod(env, displayManager, screenOrientationMethod);

            if (result >= 0 && result < 4) {
                result = screenOrientation;
            }
        }

        DetachCurrentThread();
    }

    return result;
}
