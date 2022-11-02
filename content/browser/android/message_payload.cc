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
#include "base/notreached.h"
#include "content/public/android/content_jni_headers/MessagePayloadJni_jni.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"

namespace content::android {

base::android::ScopedJavaLocalRef<jobject> ConvertWebMessagePayloadToJava(
    const blink::WebMessagePayloadView& payload) {
  JNIEnv* env = base::android::AttachCurrentThread();
  switch (payload.GetType()) {
    case blink::WebMessagePayloadType::kString:
      return Java_MessagePayloadJni_createFromString(
          env,
          base::android::ConvertUTF16ToJavaString(env, payload.GetString()));
    case blink::WebMessagePayloadType::kArrayBuffer: {
      return Java_MessagePayloadJni_createFromArrayBuffer(
          env, payload.GetOrCreateArrayBufferJavaArray());
    }
    case blink::WebMessagePayloadType::kInvalid:
      break;
  }
  NOTREACHED() << "Unsupported payload type: "
               << static_cast<int>(payload.GetType());
  return base::android::ScopedJavaLocalRef<jobject>();
}

blink::WebMessagePayloadView ConvertToWebMessagePayloadFromJava(
    const base::android::ScopedJavaLocalRef<jobject>& java_message) {
  CHECK(java_message);
  JNIEnv* env = base::android::AttachCurrentThread();
  const MessagePayloadType type = static_cast<MessagePayloadType>(
      Java_MessagePayloadJni_getType(env, java_message));
  switch (type) {
    case MessagePayloadType::kString: {
      return blink::WebMessagePayloadView::NewString(
          base::android::ConvertJavaStringToUTF16(
              Java_MessagePayloadJni_getAsString(env, java_message)));
    }
    case MessagePayloadType::kArrayBuffer: {
      auto byte_array =
          Java_MessagePayloadJni_getAsArrayBuffer(env, java_message);
      return blink::WebMessagePayloadView::NewArrayBuffer(byte_array);
    }
    case MessagePayloadType::kInvalid:
      break;
  }
  NOTREACHED() << "Unsupported or invalid Java MessagePayload type.";
  return blink::WebMessagePayloadView::NewString(u"");
}

}  // namespace content::android
