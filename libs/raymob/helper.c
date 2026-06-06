#include "raymob.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

JNIEnv* AttachCurrentThread(void)
{
    JavaVM *vm = GetAndroidApp()->activity->vm;
    JNIEnv *env = NULL;

    (*vm)->AttachCurrentThread(vm, &env, NULL);
    return env;
}

void DetachCurrentThread(void)
{
    JavaVM *vm = GetAndroidApp()->activity->vm;
    (*vm)->DetachCurrentThread(vm);
}

jobject GetNativeLoaderInstance(void)
{
    return GetAndroidApp()->activity->clazz;
}

char* GetCacheDir(void)
{
    struct android_app *app = GetAndroidApp();

    JavaVM* vm = app->activity->vm;
    JNIEnv* env = NULL;
    (*vm)->AttachCurrentThread(vm, &env, NULL);

    jobject activity = app->activity->clazz;
    jclass activityClass = (*env)->GetObjectClass(env, activity);

    jmethodID getCacheDirMethod = (*env)->GetMethodID(env, activityClass, "getCacheDir", "()Ljava/io/File;");

    jobject cacheDir = (*env)->CallObjectMethod(env, activity, getCacheDirMethod);

    jclass fileClass = (*env)->GetObjectClass(env, cacheDir);

    jmethodID getPathMethod = (*env)->GetMethodID(env, fileClass, "getPath", "()Ljava/lang/String;");

    jstring pathString = (jstring)(*env)->CallObjectMethod(env, cacheDir, getPathMethod);

    const char *pathChars = (*env)->GetStringUTFChars(env, pathString, NULL);

    size_t len = strlen(pathChars) + 1;
    char* cachePath = RL_MALLOC(len);

    if (cachePath)
    {
        strncpy(cachePath, pathChars, len);
        cachePath[len - 1] = '\0';
    }

    (*env)->ReleaseStringUTFChars(env, pathString, pathChars);

    (*env)->DeleteLocalRef(env, pathString);
    (*env)->DeleteLocalRef(env, fileClass);
    (*env)->DeleteLocalRef(env, cacheDir);
    (*env)->DeleteLocalRef(env, activityClass);

    (*vm)->DetachCurrentThread(vm);

    return cachePath;
}

char* LoadCacheFile(const char* fileName)
{
    char *text = NULL;
    char *cacheDir = GetCacheDir();
    size_t len1 = strlen(cacheDir);
    size_t len2 = strlen(fileName);
    size_t len = len1 + len2 + 1;
    char *filePath = RL_MALLOC(len);
    strncpy(filePath, cacheDir, len1);
    filePath[len1] = '/';
    strncpy(filePath + len1 + 1, fileName, len2);
    filePath[len] = '\0';

    FILE * file = fopen(filePath, "rt");
    if (file != NULL)
    {
        fseek(file, 0, SEEK_END);
        unsigned int size = (unsigned int)ftell(file);
        fseek(file, 0, SEEK_SET);

        if (size > 0)
        {
            text = (char *)RL_MALLOC((size + 1)*sizeof(char));

            if (text != NULL)
            {
                unsigned int count = (unsigned int)fread(text, sizeof(char), size, file);

                if (count < size) text = RL_REALLOC(text, count + 1);

                text[count] = '\0';

                TraceLog(LOG_INFO, "FILEIO: [%s] Text file loaded successfully", fileName);
            }
            else TraceLog(LOG_WARNING, "FILEIO: [%s] Failed to allocated memory for file reading", fileName);
        }
        else TraceLog(LOG_WARNING, "FILEIO: [%s] Failed to read text file", fileName);

        fclose(file);
    }
    else TraceLog(LOG_WARNING, "FILEIO: File name provided is not valid");

    RL_FREE(filePath);
    RL_FREE(cacheDir);

    return text;
}

char* GetL10NString(const char* value)
{
    jobject nativeInstance = GetNativeLoaderInstance();

    if (nativeInstance != NULL)
    {
        JNIEnv* env = AttachCurrentThread();

        jclass nativeClass = (*env)->GetObjectClass(env, nativeInstance);

        jmethodID getResourcesMethod = (*env)->GetMethodID(env, nativeClass, "getResources", "()Landroid/content/res/Resources;");
        jobject resources = (*env)->CallObjectMethod(env, nativeInstance, getResourcesMethod);

        jclass resourcesClass = (*env)->GetObjectClass(env, resources);
        jmethodID getIdentifierMethod = (*env)->GetMethodID(env, resourcesClass, "getIdentifier",
                                                            "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I");

        jstring resourceName = (*env)->NewStringUTF(env, value);
        jstring defType = (*env)->NewStringUTF(env, "string");

        jmethodID getPackageNameMethod = (*env)->GetMethodID(env, nativeClass, "getPackageName", "()Ljava/lang/String;");
        jstring packageName = (jstring)(*env)->CallObjectMethod(env, nativeInstance, getPackageNameMethod);

        jint resId = (*env)->CallIntMethod(env, resources, getIdentifierMethod, resourceName, defType, packageName);

        (*env)->DeleteLocalRef(env, resourceName);
        (*env)->DeleteLocalRef(env, defType);

        if (resId == 0) {
            DetachCurrentThread();
            return NULL;
        }

        jmethodID getStringMethod = (*env)->GetMethodID(env, nativeClass, "getString", "(I)Ljava/lang/String;");
        jstring rv = (jstring)(*env)->CallObjectMethod(env, nativeInstance, getStringMethod, resId);

        if (rv == NULL) {
            DetachCurrentThread();
            return NULL;
        }

        const char* strReturn = (*env)->GetStringUTFChars(env, rv, NULL);

        size_t len = strlen(strReturn) + 1;
        char* stringValue = RL_MALLOC(len);

        if (stringValue) {
            strncpy(stringValue, strReturn, len);
            stringValue[len - 1] = '\0';
        }

        (*env)->ReleaseStringUTFChars(env, rv, strReturn);
        (*env)->DeleteLocalRef(env, rv);

        DetachCurrentThread();
        return stringValue;
    }

    return NULL;
}

char* GetAppStoragePath(){
    jobject nativeInstance = GetNativeLoaderInstance();

    if (nativeInstance != NULL)
    {
        JNIEnv* env = AttachCurrentThread();

        jclass nativeClass = (*env)->GetObjectClass(env, nativeInstance);

        jmethodID getExternalFilesDirMethod = (*env)->GetMethodID(env, nativeClass, "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");

        jobject fileObj = (*env)->CallObjectMethod(env, nativeInstance, getExternalFilesDirMethod, NULL);

        jclass fileClass = (*env)->GetObjectClass(env, fileObj);

        jmethodID getAbsolutePathMethod = (*env)->GetMethodID(env, fileClass, "getAbsolutePath", "()Ljava/lang/String;");

        jstring jFilePath = (jstring)(*env)->CallObjectMethod(env, fileObj, getAbsolutePathMethod);

        const char *cFilePath = (*env)->GetStringUTFChars(env, jFilePath, NULL);

        char *filepath = strdup(cFilePath);

        (*env)->ReleaseStringUTFChars(env, jFilePath, cFilePath);
        (*env)->DeleteLocalRef(env, jFilePath);
        (*env)->DeleteLocalRef(env, fileClass);
        (*env)->DeleteLocalRef(env, fileObj);
        (*env)->DeleteLocalRef(env, nativeClass);

        DetachCurrentThread();

        return filepath;
    }

    return NULL;
}

void* ReadFromAppStorage(const char *filepath, int *dataSize){
    char *appStoragePath = GetAppStoragePath();

    size_t pathLen = strlen(appStoragePath) + strlen(filepath) + 2;
    char *path = RL_MALLOC(sizeof(char)*pathLen);
    snprintf(path, pathLen, "%s/%s", appStoragePath, filepath);

    unsigned char *data = NULL;
    *dataSize = 0;

    FILE *file = fopen(path, "rb");

    if (file == NULL){
        TraceLog(LOG_WARNING, "FILEIO: [%s] Failed to open file", path);
        RL_FREE(appStoragePath);
        RL_FREE(path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size > 0)
    {
        data = RL_MALLOC(size*sizeof(unsigned char));

        if (data != NULL)
        {
            size_t count = fread(data, sizeof(unsigned char), size, file);

            if (count > 2147483647)
            {
                TraceLog(LOG_WARNING, "FILEIO: [%s] File is bigger than 2147483647 bytes, avoid using LoadFileData()", path);

                RL_FREE(data);
                data = NULL;
            }
            else
            {
                *dataSize = (int)count;

                if ((*dataSize) != size) TraceLog(LOG_WARNING, "FILEIO: [%s] File partially loaded (%i bytes out of %i)", path, dataSize, count);
                else TraceLog(LOG_INFO, "FILEIO: [%s] File loaded successfully", path);
            }
        }
        else TraceLog(LOG_WARNING, "FILEIO: [%s] Failed to allocated memory for file reading", path);
    }
    else TraceLog(LOG_WARNING, "FILEIO: [%s] Failed to read file", path);

    fclose(file);

    RL_FREE(appStoragePath);
    RL_FREE(path);

    return data;
}

bool WriteToAppStorage(const char *filepath, void *data, unsigned int dataSize){
    char *appStoragePath = GetAppStoragePath();

    size_t pathLen = strlen(appStoragePath) + strlen(filepath) + 2;
    char *path = RL_MALLOC(sizeof(char)*pathLen);
    snprintf(path, pathLen, "%s/%s", appStoragePath, filepath);

    bool success = false;

    FILE *file = fopen(path, "wb");

    if (file != NULL)
    {
        int count = (int)fwrite(data, sizeof(unsigned char), dataSize, file);

        if (count == 0) TraceLog(LOG_WARNING, "FILEIO: [%s] Failed to write file", path);
        else if (count != dataSize) TraceLog(LOG_WARNING, "FILEIO: [%s] File partially written", path);
        else TraceLog(LOG_INFO, "FILEIO: [%s] File saved successfully", path);

        int result = fclose(file);
        if (result == 0) success = true;
    }
    else TraceLog(LOG_WARNING, "FILEIO: [%s] Failed to open file", path);

    RL_FREE(appStoragePath);
    RL_FREE(path);

    return success;
}

bool IsFileExistsInAppStorage(const char *filepath){
    char *appStoragePath = GetAppStoragePath();

    size_t pathLen = strlen(appStoragePath) + strlen(filepath) + 2;
    char *path = RL_MALLOC(sizeof(char)*pathLen);
    snprintf(path, pathLen, "%s/%s", appStoragePath, filepath);

    bool success = (access(path, F_OK) != -1);

    RL_FREE(appStoragePath);
    RL_FREE(path);

    return success;
}

void RemoveFileInAppStorage(const char *filepath){
    char *appStoragePath = GetAppStoragePath();

    size_t pathLen = strlen(appStoragePath) + strlen(filepath) + 2;
    char *path = RL_MALLOC(sizeof(char)*pathLen);
    snprintf(path, pathLen, "%s/%s", appStoragePath, filepath);

    remove(path);

    RL_FREE(appStoragePath);
    RL_FREE(path);
}
