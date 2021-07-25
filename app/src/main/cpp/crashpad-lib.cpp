#include <jni.h>
#include <string>
#include <unistd.h>
#include <cstdio>
#include <android/log.h>
#include <pthread.h>
#include <bitset>

#include "client/crashpad_client.h"
#include "client/crash_report_database.h"
#include "client/settings.h"

#define LOG_TAG "GoogleCrashPad"
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

#if defined(__APPLE__)
typedef std::string StringType;
#elif  defined(__linux__)
typedef std::string StringType;
#elif defined(_MSC_VER)
typedef std::wstring StringType;
#endif

using namespace base;
using namespace crashpad;
using namespace std;


static JNIEnv *global_env;
static jobject global_native_crash_handler;


//全局变量
JavaVM *g_jvm = nullptr;

void* thread_fun(void* );

bool callback() {
    ALOGD("===============NativeCrashHandler.CRASH.DumpCallback================");
    pthread_t thread;
    pthread_create(&thread, nullptr, &thread_fun, nullptr);
    pthread_join(thread, nullptr);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_club_zohar_crashpad_demo_gn_MainActivity_initCrashPad(
        JNIEnv *env,
        jobject thiz,
        jstring path,
        jstring nativeLibDir
        ) {
    env->GetJavaVM(&g_jvm);
    global_env = env;
    global_native_crash_handler = env->NewGlobalRef(thiz);

    string dataDir = env->GetStringUTFChars(path, nullptr);
    string nativeLibDirPath = env->GetStringUTFChars(nativeLibDir, nullptr);

    // Ensure that handler is shipped with your application
    FilePath handler(nativeLibDirPath + "/libcrashpad_handler.so");

    // Directory where reports will be saved. Important! Must be writable or crashpad_handler will crash.
    FilePath reportsDir(dataDir + "/crashpad");

    // Directory where metrics will be saved. Important! Must be writable or crashpad_handler will crash.
    FilePath metricsDir(dataDir + "/crashpad");

    // Crashpad upload URL for BugSplat database
    string url = "http://{{database}}.bugsplat.com/post/bp/crash/crashpad.php";

    // Crashpad annotations
    map<string, string> annotations;
    annotations["format"] = "minidump";           // Required: Crashpad setting to save crash as a minidump
    annotations["database"] = "{{database}}";     // Required: BugSplat database
    annotations["product"] = "{{appName}}";       // Required: BugSplat appName
    annotations["version"] = "{{appVersion}}";    // Required: BugSplat appVersion
    annotations["key"] = "Key";                   // Optional: BugSplat key field
    annotations["user"] = "fred@bugsplat.com";    // Optional: BugSplat user email
    annotations["list_annotations"] = "Sample comment"; // Optional: BugSplat crash description


    // Disable crashpad rate limiting so that all crashes have dmp files
    vector<StringType> arguments;
    arguments.emplace_back("--no-rate-limit");

    // Initialize Crashpad database
    unique_ptr<CrashReportDatabase> database = CrashReportDatabase::Initialize(reportsDir);
    if (database == nullptr) return false;

    // Enable automated crash uploads
    Settings *settings = database->GetSettings();
    if (settings == nullptr) return false;
    settings->SetUploadsEnabled(true);

    // File paths of attachments to be uploaded with the minidump file at crash time - default bundle limit is 2MB
    vector<FilePath> attachments;
    FilePath attachment(dataDir + "/files/attachment.txt");
    attachments.push_back(attachment);

    // Start Crashpad crash handler
    static auto *client = new CrashpadClient();
    bool status = client->StartHandler(handler, reportsDir, metricsDir, url, annotations, arguments, true, true, attachments);
    return status;
}

extern "C" JNIEXPORT void JNICALL
Java_club_zohar_crashpad_demo_gn_MainActivity_testNativeCrash(
        __unused JNIEnv *env,
        __unused jobject thiz
        ) {
    int* a = nullptr;
    *a = 1;
}

void *thread_fun(void*) {
    ALOGD("===============NativeCrashHandler.thread_fun.Start================");
    //Attach主线程
    if (g_jvm->AttachCurrentThread(&global_env, nullptr) != JNI_OK) {
        ALOGD("===============AttachCurrentThread.NOT.JNI_OK================");
    }
    jclass class_native_crash_handler = global_env->GetObjectClass(global_native_crash_handler);
    jmethodID methodID = global_env->GetMethodID(class_native_crash_handler, "test","()V");
    global_env->CallVoidMethod(global_native_crash_handler,methodID);
    //Detach主线程
    if (g_jvm->DetachCurrentThread() != JNI_OK) {
        ALOGD("===============DetachCurrentThread.NOT.JNI_OK================");
    }
    ALOGD("===============NativeCrashHandler.thread_fun.End================");
    pthread_exit(nullptr);

}