/*
 *  raymob License (MIT)
 *
 *  Copyright (c) 2023-2024 Le Juez Victor
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */

#ifndef RAYMOB_H
#define RAYMOB_H

#include "android_native_app_glue.h"
#include "raylib.h"
#include "jni.h"

typedef enum {
    SENSOR_ACCELEROMETER    = 0,
    SENSOR_GYROSCOPE        = 1,
} Sensor;

typedef enum {
    ORIENTATION_PORTRAIT           = 0,
    ORIENTATION_LANDSCAPE          = 1,
    ORIENTATION_PORTRAIT_REVERSED  = 2,
    ORIENTATION_LANDSCAPE_REVERSED = 3,
    ORIENTATION_OTHER              = -1,
} Orientation;

typedef void (*Callback)();

#if defined(__cplusplus)
extern "C" {
#endif

struct android_app *GetAndroidApp(void);

JNIEnv* AttachCurrentThread(void);
void DetachCurrentThread(void);
jobject GetNativeLoaderInstance(void);

char* GetCacheDir(void);
char* LoadCacheFile(const char* fileName);
char* GetL10NString(const char* value);

void Vibrate(float seconds);
void VibrateMS(uint64_t ms);
void VibrateEx(float seconds, float intensity);
void VibrateExMS(uint64_t ms, float intensity);

void InitSensorManager(void);
void EnableSensor(Sensor sensor);
void DisableSensor(Sensor sensor);
Orientation GetScreenOrientation();
Vector3 GetAccelerotmerAxis(void);
Vector3 GetGyroscopeAxis(void);

void ShowSoftKeyboard(void);
void HideSoftKeyboard(void);
int GetLastSoftKeyCode(void);
unsigned short GetLastSoftKeyLabel(void);
int GetLastSoftKeyUnicode(void);
char GetLastSoftKeyChar(void);
void ClearLastSoftKey(void);
void SoftKeyboardEditText(char* text, unsigned int size);

void KeepScreenOn(bool keepOn);

void InitCallBacks(void);
void SetOnStartCallBack(Callback callback);
void SetOnResumeCallBack(Callback callback);
void SetOnPauseCallBack(Callback callback);
void SetOnStopCallBack(Callback callback);

char* GetAppStoragePath();
void* ReadFromAppStorage(const char *filepath, int *size);
bool WriteToAppStorage(const char *filepath, void *data, unsigned int size);
bool IsFileExistsInAppStorage(const char *filepath);
void RemoveFileInAppStorage(const char *filepath);

#if defined(__cplusplus)
}
#endif

#endif
