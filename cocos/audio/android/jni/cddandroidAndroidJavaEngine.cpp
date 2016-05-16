/****************************************************************************
Copyright (c) 2010-2012 cocos2d-x.org
Copyright (c) 2013-2015 Chukong Technologies Inc.

http://www.cocos2d-x.org

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/

#include "audio/android/jni/cddandroidAndroidJavaEngine.h"
#include <stdlib.h>
#include <android/log.h>
#include <dlfcn.h>
#include <jni.h>
#include <sys/system_properties.h>
#include "audio/android/ccdandroidUtils.h"
#include "audio/include/AudioEngine.h"
#include "platform/android/jni/JniHelper.h"

// logging
#define  LOG_TAG    "cocosdenshion::android::AndroidJavaEngine"
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG,__VA_ARGS__)

#if (__ANDROID_API__ >= 21)
// Android 'L' makes __system_property_get a non-global symbol.
// Here we provide a stub which loads the symbol from libc via dlsym.
typedef int (*PFN_SYSTEM_PROP_GET)(const char *, char *);
int __system_property_get(const char* name, char* value)
{
    static PFN_SYSTEM_PROP_GET __real_system_property_get = NULL;
    if (!__real_system_property_get) {
        // libc.so should already be open, get a handle to it.
        void *handle = dlopen("libc.so", RTLD_NOLOAD);
        if (!handle) {
            __android_log_print(ANDROID_LOG_ERROR, "foobar", "Cannot dlopen libc.so: %s.\n", dlerror());
        } else {
            __real_system_property_get = (PFN_SYSTEM_PROP_GET)dlsym(handle, "__system_property_get");
        }
        if (!__real_system_property_get) {
            __android_log_print(ANDROID_LOG_ERROR, "foobar", "Cannot resolve __system_property_get(): %s.\n", dlerror());
        }
    }
    return (*__real_system_property_get)(name, value);
} 
#endif // __ANDROID_API__ >= 21

// Java class
static const std::string helperClassName = "org/cocos2dx/lib/Cocos2dxHelper";

using namespace cocos2d;
using namespace cocos2d::experimental;
using namespace CocosDenshion::android;

AndroidJavaEngine::AndroidJavaEngine()
    : _implementBaseOnAudioEngine(false)
    , _effectVolume(1.f)
{
    char sdk_ver_str[PROP_VALUE_MAX] = "0";
    auto len = __system_property_get("ro.build.version.sdk", sdk_ver_str);
    if (len > 0)
    {
        auto sdk_ver = atoi(sdk_ver_str);
        __android_log_print(ANDROID_LOG_DEBUG, "cocos2d", "android build version:%d", sdk_ver);
        if (sdk_ver == 21)
        {
            _implementBaseOnAudioEngine = true;
        }
    }
    else
    {
        __android_log_print(ANDROID_LOG_DEBUG, "cocos2d", "%s", "Fail to get android build version.");
    }
}

AndroidJavaEngine::~AndroidJavaEngine()
{
    if (_implementBaseOnAudioEngine)
    {
        stopAllEffects();
    }

    JniHelper::callStaticVoidMethod(helperClassName, "end");
}

void AndroidJavaEngine::preloadBackgroundMusic(const char* filePath) {
    std::string fullPath = CocosDenshion::android::getFullPathWithoutAssetsPrefix(filePath);
    JniHelper::callStaticVoidMethod(helperClassName, "preloadBackgroundMusic", filePath);
}

void AndroidJavaEngine::playBackgroundMusic(const char* filePath, bool loop) {
    std::string fullPath = CocosDenshion::android::getFullPathWithoutAssetsPrefix(filePath);
    JniHelper::callStaticVoidMethod(helperClassName, "playBackgroundMusic", filePath, loop);
}

void AndroidJavaEngine::stopBackgroundMusic(bool releaseData) {
    JniHelper::callStaticVoidMethod(helperClassName, "stopBackgroundMusic");
}

void AndroidJavaEngine::pauseBackgroundMusic() {
    JniHelper::callStaticVoidMethod(helperClassName, "pauseBackgroundMusic");

}

void AndroidJavaEngine::resumeBackgroundMusic() {
    JniHelper::callStaticVoidMethod(helperClassName, "resumeBackgroundMusic");
}

void AndroidJavaEngine::rewindBackgroundMusic() {
    JniHelper::callStaticVoidMethod(helperClassName, "rewindBackgroundMusic");
}

bool AndroidJavaEngine::willPlayBackgroundMusic() {
    return true;
}

bool AndroidJavaEngine::isBackgroundMusicPlaying() {
    return JniHelper::callStaticBooleanMethod(helperClassName, "isBackgroundMusicPlaying");
}

float AndroidJavaEngine::getBackgroundMusicVolume() {
    return JniHelper::callStaticFloatMethod(helperClassName, "getBackgroundMusicVolume");
}

void AndroidJavaEngine::setBackgroundMusicVolume(float volume) {
    JniHelper::callStaticVoidMethod(helperClassName, "setBackgroundMusicVolume", volume);
}

float AndroidJavaEngine::getEffectsVolume()
{
    if (_implementBaseOnAudioEngine)
    {
        return _effectVolume;
    }
    else
    {
        return JniHelper::callStaticFloatMethod(helperClassName, "getEffectsVolume");
    }
}

void AndroidJavaEngine::setEffectsVolume(float volume)
{
    if (_implementBaseOnAudioEngine)
    {
        if (volume > 1.f)
        {
            volume = 1.f;
        }
        else if (volume < 0.f)
        {
            volume = 0.f;
        }

        if (_effectVolume != volume)
        {
            _effectVolume = volume;
            for (auto it : _soundIDs)
            {
                AudioEngine::setVolume(it, volume);
            }
        }
    }
    else
    {
        JniHelper::callStaticVoidMethod(helperClassName, "setEffectsVolume", volume);
    }
}

unsigned int AndroidJavaEngine::playEffect(const char* filePath, bool loop,
    float pitch, float pan, float gain)
{
    if (_implementBaseOnAudioEngine)
    {
        auto soundID = AudioEngine::play2d(filePath, loop, _effectVolume);
        if (soundID != AudioEngine::INVALID_AUDIO_ID)
        {
            _soundIDs.push_back(soundID);

            AudioEngine::setFinishCallback(soundID, [this](int id, const std::string& filePath){
                _soundIDs.remove(id);
            });
        }

        return soundID;
    }
    else
    {
        std::string fullPath = CocosDenshion::android::getFullPathWithoutAssetsPrefix(filePath);
        int ret = JniHelper::callStaticIntMethod(helperClassName, "playEffect", fullPath, loop, pitch, pan, gain);
        return (unsigned int)ret;
    }
}

void AndroidJavaEngine::pauseEffect(unsigned int soundID)
{
    if (_implementBaseOnAudioEngine)
    {
        AudioEngine::pause(soundID);
    }
    else
    {
        JniHelper::callStaticVoidMethod(helperClassName, "pauseEffect", (int)soundID);
    }
}

void AndroidJavaEngine::resumeEffect(unsigned int soundID)
{
    if (_implementBaseOnAudioEngine)
    {
        AudioEngine::resume(soundID);
    }
    else
    {
        JniHelper::callStaticVoidMethod(helperClassName, "resumeEffect", (int)soundID);
    }
}

void AndroidJavaEngine::stopEffect(unsigned int soundID)
{
    if (_implementBaseOnAudioEngine)
    {
        AudioEngine::stop(soundID);
        _soundIDs.remove(soundID);
    }
    else
    {
        JniHelper::callStaticVoidMethod(helperClassName, "stopEffect", (int)soundID);
    }
}

void AndroidJavaEngine::pauseAllEffects()
{
    if (_implementBaseOnAudioEngine)
    {
        for (auto it : _soundIDs)
        {
            AudioEngine::pause(it);
        }
    }
    else
    {
        JniHelper::callStaticVoidMethod(helperClassName, "pauseAllEffects");
    }
}

void AndroidJavaEngine::resumeAllEffects()
{
    if (_implementBaseOnAudioEngine)
    {
        for (auto it : _soundIDs)
        {
            AudioEngine::resume(it);
        }
    }
    else
    {
        JniHelper::callStaticVoidMethod(helperClassName, "resumeAllEffects");
    }
}

void AndroidJavaEngine::stopAllEffects()
{
    if (_implementBaseOnAudioEngine)
    {
        for (auto it : _soundIDs)
        {
            AudioEngine::stop(it);
        }
        _soundIDs.clear();
    }
    else
    {
        JniHelper::callStaticVoidMethod(helperClassName, "stopAllEffects");
    }
}

void AndroidJavaEngine::preloadEffect(const char* filePath)
{
    if (!_implementBaseOnAudioEngine)
    {
        std::string fullPath = CocosDenshion::android::getFullPathWithoutAssetsPrefix(filePath);
        JniHelper::callStaticVoidMethod(helperClassName, "preloadEffect", fullPath);
    }
}

void AndroidJavaEngine::unloadEffect(const char* filePath)
{
    if (!_implementBaseOnAudioEngine)
    {
        std::string fullPath = CocosDenshion::android::getFullPathWithoutAssetsPrefix(filePath);
        JniHelper::callStaticVoidMethod(helperClassName, "unloadEffect", fullPath);
    }
}
