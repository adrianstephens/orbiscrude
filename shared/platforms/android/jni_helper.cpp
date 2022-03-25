#include "jni_helper.h"
#include <android/log.h>

namespace iso {
    namespace jni {
		Global<Object>	activity;
		thread_local JNIEnv *ObjectHolder::env;

		void ExceptionCheck(JNIEnv *env) {
			if (jthrowable exception = env->ExceptionOccurred()) {
				__android_log_print(ANDROID_LOG_DEBUG, "ISO", "exception occurred");

				jmethodID mid;

				// Until this happens most JNI operations have undefined behaviour
				env->ExceptionClear();

				jclass exceptionClass = env->GetObjectClass(exception);
				jclass classClass = env->FindClass("java/lang/Class");

				mid = env->GetMethodID(classClass, "getName", "()Ljava/lang/String;");
				jstring exceptionName = (jstring)env->CallObjectMethod(exceptionClass, mid);
				const char* exceptionNameUTF8 = env->GetStringUTFChars(exceptionName, 0);

				mid = env->GetMethodID(exceptionClass, "getMessage", "()Ljava/lang/String;");

				if (jstring exceptionMessage = (jstring)env->CallObjectMethod(exception, mid)) {
					const char* exceptionMessageUTF8 = env->GetStringUTFChars(exceptionMessage, 0);
					__android_log_print(ANDROID_LOG_ERROR, "ISO", "ExceptionMessage: %s", exceptionMessageUTF8);
					env->ReleaseStringUTFChars(exceptionMessage, exceptionMessageUTF8);
				} else {
					__android_log_print(ANDROID_LOG_ERROR, "ISO", "no exception message");
				}

				env->ReleaseStringUTFChars(exceptionName, exceptionNameUTF8);
			}
		}

		jclass FindClass(JNIEnv *env, const char* className) {
			jclass clazz = env->FindClass( className );
			if (env->ExceptionCheck()) {
				env->ExceptionClear();
				auto class_loader = activity.Call<java::lang::ClassLoader>("getClassLoader");
				clazz 	= class_loader.Call<java::lang::Class>("loadClass", java::lang::String(className));
				if (env->ExceptionCheck()) {
					env->ExceptionClear();
					clazz = nullptr;
				}
			}
			return clazz;
		}

    }
}
