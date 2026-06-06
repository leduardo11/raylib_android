#include "raymob.h"

#include <android/sensor.h>
#include <stdlib.h>

struct SensorInputs {
    Vector3 accelerometerAxis;
    Vector3 gyroscopeAxis;
};

static struct {

    ASensorManager* manager;
    ASensorEventQueue* eventQueue;
    const ASensor *sensors[2];
    int looperID;

    struct SensorInputs inputs;

} State = { 0 };

static const char* GetSensorName(Sensor sensor)
{
    const char *sensorName = "UNKNOWN";
    switch (sensor) {
        case SENSOR_ACCELEROMETER: sensorName = "SENSOR_ACCELEROMETER"; break;
        case SENSOR_GYROSCOPE: sensorName = "SENSOR_GYROSCOPE"; break;
        default: break;
    }
    return sensorName;
}

static int SensorCallback(int fd, int events, void* data)
{
    ASensorEvent event;
    while (ASensorEventQueue_getEvents(State.eventQueue, &event, 1) > 0) {
        switch (event.type) {
            case ASENSOR_TYPE_ACCELEROMETER:
                State.inputs.accelerometerAxis.x = event.acceleration.x;
                State.inputs.accelerometerAxis.y = event.acceleration.y;
                State.inputs.accelerometerAxis.z = event.acceleration.z;
                break;
            case ASENSOR_TYPE_GYROSCOPE:
                State.inputs.gyroscopeAxis.x = event.gyro.x;
                State.inputs.gyroscopeAxis.y = event.gyro.y;
                State.inputs.gyroscopeAxis.z = event.gyro.z;
                break;
            default:
                break;
        }
    }
    return 1;
}

void InitSensorManager(void)
{
    JNIEnv* env = AttachCurrentThread();
    jobject activity = GetAndroidApp()->activity->clazz;
    jclass activityClass = (*env)->GetObjectClass(env, activity);
    jmethodID getPackageNameMethod = (*env)->GetMethodID(env, activityClass, "getPackageName", "()Ljava/lang/String;");
    jstring packageNameJ = (jstring)(*env)->CallObjectMethod(env, activity, getPackageNameMethod);
    const char* packageName = (*env)->GetStringUTFChars(env, packageNameJ, NULL);
    State.manager = ASensorManager_getInstanceForPackage(packageName);
    (*env)->ReleaseStringUTFChars(env, packageNameJ, packageName);
    (*env)->DeleteLocalRef(env, packageNameJ);
    (*env)->DeleteLocalRef(env, activityClass);
    DetachCurrentThread();

    ASensorList sensorList;
    int sensorCount = ASensorManager_getSensorList(State.manager, &sensorList);

    for (int i = 0 ; i < sensorCount ; i++) {
        const char *type = ASensor_getStringType(sensorList[i]);
        const char *vendor = ASensor_getVendor(sensorList[i]);
        const char *name = ASensor_getName(sensorList[i]);
        bool supported = true;
        switch (ASensor_getType(sensorList[i])) {
            case ASENSOR_TYPE_ACCELEROMETER:
                State.sensors[SENSOR_ACCELEROMETER] = sensorList[i];
                break;
            case ASENSOR_TYPE_GYROSCOPE:
                State.sensors[SENSOR_GYROSCOPE] = sensorList[i];
                break;
            default:
                supported = false;
                break;
        }
        TraceLog(LOG_INFO, "Sensor %s:\n    > Name: %s\n    > Vendor: %s\n    > Supported: %s",
                 type, name, vendor, supported ? "YES" : "NO");
    }

    State.looperID = 1;
    State.eventQueue = ASensorManager_createEventQueue(
            State.manager, ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS),
            State.looperID, SensorCallback, NULL);

    if (State.eventQueue == NULL) {
        TraceLog(LOG_ERROR, "Cannot create sensor event queue");
    }
}

void EnableSensor(Sensor sensor)
{
    if (State.sensors[sensor] == NULL) {
        TraceLog(LOG_WARNING, "Cannot enable unsupported sensor: %s", GetSensorName(sensor));
        return;
    }
    if (ASensorEventQueue_enableSensor(State.eventQueue, State.sensors[sensor]) != 0) {
        TraceLog(LOG_ERROR, "Cannot enable sensor: %s", GetSensorName(sensor));
    }
}

void DisableSensor(Sensor sensor)
{
    if (State.sensors[sensor] == NULL) {
        TraceLog(LOG_WARNING, "Cannot disable unsupported sensor: %s", GetSensorName(sensor));
        return;
    }
    if (ASensorEventQueue_disableSensor(State.eventQueue, State.sensors[sensor]) != 0) {
        TraceLog(LOG_ERROR, "Cannot disable sensor: %s", GetSensorName(sensor));
    }
}

bool IsSensorAvailable(Sensor sensor)
{
    return State.sensors[sensor] != NULL;
}

Vector3 GetAccelerotmerAxis(void)
{
    return State.inputs.accelerometerAxis;
}

Vector3 GetGyroscopeAxis(void)
{
    return State.inputs.gyroscopeAxis;
}
