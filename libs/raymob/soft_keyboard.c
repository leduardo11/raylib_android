#include "raymob.h"
#include <string.h>

void ShowSoftKeyboard(void)
{
    jobject context = GetNativeLoaderInstance();

    if (context != NULL) {
        JNIEnv* env = AttachCurrentThread();
        jclass nativeLoaderClass = (*env)->GetObjectClass(env, context);

        jfieldID softKeyboardField = (*env)->GetFieldID(env, nativeLoaderClass, "softKeyboard", "Lcom/raylib/android/SoftKeyboard;");
        jobject softKeyboard = (*env)->GetObjectField(env, context, softKeyboardField);

        if (softKeyboard != NULL) {
            jclass softKeyboardClass = (*env)->GetObjectClass(env, softKeyboard);
            jmethodID method = (*env)->GetMethodID(env, softKeyboardClass, "showKeyboard", "()V");
            (*env)->CallVoidMethod(env, softKeyboard, method);
        }

        DetachCurrentThread();
    }
}

void HideSoftKeyboard(void)
{
    jobject context = GetNativeLoaderInstance();

    if (context != NULL) {
        JNIEnv* env = AttachCurrentThread();
        jclass nativeLoaderClass = (*env)->GetObjectClass(env, context);

        jfieldID softKeyboardField = (*env)->GetFieldID(env, nativeLoaderClass, "softKeyboard", "Lcom/raylib/android/SoftKeyboard;");
        jobject softKeyboard = (*env)->GetObjectField(env, context, softKeyboardField);

        if (softKeyboard != NULL) {
            jclass softKeyboardClass = (*env)->GetObjectClass(env, softKeyboard);
            jmethodID method = (*env)->GetMethodID(env, softKeyboardClass, "hideKeyboard", "()V");
            (*env)->CallVoidMethod(env, softKeyboard, method);
        }

        DetachCurrentThread();
    }
}

int GetLastSoftKeyCode(void)
{
    jobject context = GetNativeLoaderInstance();

    if (context != NULL) {
        JNIEnv* env = AttachCurrentThread();
        jclass nativeLoaderClass = (*env)->GetObjectClass(env, context);

        jfieldID softKeyboardField = (*env)->GetFieldID(env, nativeLoaderClass, "softKeyboard", "Lcom/raylib/android/SoftKeyboard;");
        jobject softKeyboard = (*env)->GetObjectField(env, context, softKeyboardField);

        if (softKeyboard != NULL) {
            jclass softKeyboardClass = (*env)->GetObjectClass(env, softKeyboard);
            jmethodID method = (*env)->GetMethodID(env, softKeyboardClass, "getLastKeyCode", "()I");
            int value = (*env)->CallIntMethod(env, softKeyboard, method);
            DetachCurrentThread();
            return value;
        }

        DetachCurrentThread();
    }

    return 0;
}

unsigned short GetLastSoftKeyLabel(void)
{
    jobject context = GetNativeLoaderInstance();

    if (context != NULL) {
        JNIEnv* env = AttachCurrentThread();
        jclass nativeLoaderClass = (*env)->GetObjectClass(env, context);

        jfieldID softKeyboardField = (*env)->GetFieldID(env, nativeLoaderClass, "softKeyboard", "Lcom/raylib/android/SoftKeyboard;");
        jobject softKeyboard = (*env)->GetObjectField(env, context, softKeyboardField);

        if (softKeyboard != NULL) {
            jclass softKeyboardClass = (*env)->GetObjectClass(env, softKeyboard);
            jmethodID method = (*env)->GetMethodID(env, softKeyboardClass, "getLastKeyLabel", "()C");
            unsigned short value = (*env)->CallCharMethod(env, softKeyboard, method);
            DetachCurrentThread();
            return value;
        }
    }

    return 0;
}

int GetLastSoftKeyUnicode(void)
{
    jobject context = GetNativeLoaderInstance();

    if (context != NULL) {
        JNIEnv* env = AttachCurrentThread();
        jclass nativeLoaderClass = (*env)->GetObjectClass(env, context);

        jfieldID softKeyboardField = (*env)->GetFieldID(env, nativeLoaderClass, "softKeyboard", "Lcom/raylib/android/SoftKeyboard;");
        jobject softKeyboard = (*env)->GetObjectField(env, context, softKeyboardField);

        if (softKeyboard != NULL) {
            jclass softKeyboardClass = (*env)->GetObjectClass(env, softKeyboard);
            jmethodID method = (*env)->GetMethodID(env, softKeyboardClass, "getLastKeyUnicode", "()I");
            int value = (*env)->CallIntMethod(env, softKeyboard, method);
            DetachCurrentThread();
            return value;
        }
    }

    return 0;
}

char GetLastSoftKeyChar(void)
{
#define KEYCODE_ENTER    66
#define KEYCODE_DEL      67

    jobject context = GetNativeLoaderInstance();

    if (context != NULL) {
        char value = '\0';

        JNIEnv* env = AttachCurrentThread();
        jclass nativeLoaderClass = (*env)->GetObjectClass(env, context);

        jfieldID softKeyboardField = (*env)->GetFieldID(env, nativeLoaderClass, "softKeyboard", "Lcom/raylib/android/SoftKeyboard;");
        jobject softKeyboard = (*env)->GetObjectField(env, context, softKeyboardField);

        if (softKeyboard != NULL) {
            jclass softKeyboardClass = (*env)->GetObjectClass(env, softKeyboard);

            jmethodID methodKeyCode = (*env)->GetMethodID(env, softKeyboardClass, "getLastKeyCode", "()I");
            int keyCode = (*env)->CallIntMethod(env, softKeyboard, methodKeyCode);

            if (keyCode != 0) {
                switch (keyCode) {
                    case KEYCODE_ENTER: {
                        value = '\n';
                    } break;
                    case KEYCODE_DEL: {
                        value = '\b';
                    } break;
                    default: {
                        jmethodID methodKeyUnicode = (*env)->GetMethodID(env, softKeyboardClass, "getLastKeyUnicode", "()I");
                        int u = (*env)->CallIntMethod(env, softKeyboard, methodKeyUnicode);
                        if (u > 0xFF) value = '?';
                        else value = (char) u;
                    }
                }
            }
        }

        DetachCurrentThread();

        return value;
    }

    return '\0';
}

void ClearLastSoftKey(void)
{
    jobject context = GetNativeLoaderInstance();

    if (context != NULL) {
        JNIEnv* env = AttachCurrentThread();
        jclass nativeLoaderClass = (*env)->GetObjectClass(env, context);

        jfieldID softKeyboardField = (*env)->GetFieldID(env, nativeLoaderClass, "softKeyboard", "Lcom/raylib/android/SoftKeyboard;");
        jobject softKeyboard = (*env)->GetObjectField(env, context, softKeyboardField);

        if (softKeyboard != NULL) {
            jclass softKeyboardClass = (*env)->GetObjectClass(env, softKeyboard);
            jmethodID method = (*env)->GetMethodID(env, softKeyboardClass, "clearLastKeyEvent", "()V");
            (*env)->CallVoidMethod(env, softKeyboard, method);
        }

        DetachCurrentThread();
    }
}

void SoftKeyboardEditText(char* text, unsigned int size)
{
    char c = GetLastSoftKeyChar();
    if (c == '\0') return;

    unsigned int len = strlen(text);

    if (c == '\b' && len > 0)  {
        text[len-1] = '\0';
    } else if (c != '\b' && len < size) {
        text[len++] = c;
        text[len] = '\0';
    }

    ClearLastSoftKey();
}
