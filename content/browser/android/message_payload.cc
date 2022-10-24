// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/public/browser/android/message_payload.h"

#include <string>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/overloaded.h"
#include "base/notreached.h"
#include "components/js_injection/common/web_message.h"
#include "content/public/android/content_jni_headers/MessagePayloadJni_jni.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"

namespace content::android {

base::android::ScopedJavaLocalRef<jobject> ConvertWebMessagePayloadToJava(
    const blink::WebMessagePayload& payload) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return absl::visit(
      base::Overloaded{
          [env](const std::u16string& str) {
            return Java_MessagePayloadJni_createFromString(
                env, base::android::ConvertUTF16ToJavaString(env, str));
          },
          [env](const mojo_base::BigBuffer& big_buffer) {
            return Java_MessagePayloadJni_createFromArrayBuffer(
                env, base::android::ToJavaByteArray(env, big_buffer.data(),
                                                    big_buffer.size()));
          }},
      payload);
}

base::android::ScopedJavaLocalRef<jobject> ConvertJsWebMessageToJava(
    const js_injection::JsWebMessage& message) {
  return ConvertWebMessagePayloadToJava(std::move(message.payload));
}

blink::WebMessagePayload ConvertToWebMessagePayloadFromJava(
    const base::android::ScopedJavaLocalRef<jobject>& java_message) {
  CHECK(java_message);
  JNIEnv* env = base::android::AttachCurrentThread();
  const MessagePayloadType type = static_cast<MessagePayloadType>(
      Java_MessagePayloadJni_getType(env, java_message));
  switch (type) {
    case MessagePayloadType::kString: {
      return base::android::ConvertJavaStringToUTF16(
          Java_MessagePayloadJni_getAsString(env, java_message));
    }
    case MessagePayloadType::kArrayBuffer: {
      auto byte_array =
          Java_MessagePayloadJni_getAsArrayBuffer(env, java_message);
      mojo_base::BigBuffer buffer(env->GetArrayLength(byte_array.obj()));
      env->GetByteArrayRegion(byte_array.obj(), 0, buffer.size(),
                              reinterpret_cast<jbyte*>(buffer.data()));
      return buffer;
    }
    default:
      NOTREACHED() << "Unsupported or invalid Java MessagePayload type.";
  }
  return u"";
}

js_injection::JsWebMessage ConvertToJsWebMessageFromJava(
    const base::android::ScopedJavaLocalRef<jobject>& java_message) {
  js_injection::JsWebMessage js_web_message;
  js_web_message.payload = ConvertToWebMessagePayloadFromJava(java_message);
  return js_web_message;
}

}  // namespace content::android
