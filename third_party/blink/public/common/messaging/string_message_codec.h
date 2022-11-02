// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_STRING_MESSAGE_CODEC_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_STRING_MESSAGE_CODEC_H_

#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "build/buildflag.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"

#if BUILDFLAG(IS_ANDROID)
#include <jni.h>
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#endif

namespace blink {

// Payload type of WebMessage.
enum class BLINK_COMMON_EXPORT WebMessagePayloadType {
  // Only when the WebMessagePayloadView is moved.
  kInvalid = 0,
  kString,
  kArrayBuffer,
};

// Represent view of WebMessage Payload between browser and renderer process.
class BLINK_COMMON_EXPORT WebMessagePayloadView {
 private:
  enum class ArrayBufferStorageType {
    kTransferableMessage = 0,
#if BUILDFLAG(IS_ANDROID)
    kJavaArray,
#endif
  };

 public:
  // Construct a ArrayBuffer type of WebMessagePayloadView, which is backed by
  // TransferableMessage. Caller must ensure the |data| is backed by |message|.
  static WebMessagePayloadView NewArrayBuffer(TransferableMessage&& message,
                                              base::span<const uint8_t> data);

#if BUILDFLAG(IS_ANDROID)
  // Construct a ArrayBuffer type of WebMessagePayloadView, which is backed by
  // Java Byte of Array.
  static WebMessagePayloadView NewArrayBuffer(
      base::android::ScopedJavaLocalRef<jbyteArray> java_ref);
#endif

  // Construct a String type of WebMessagePayloadView.
  static WebMessagePayloadView NewString(std::u16string string);

  WebMessagePayloadView() = default;
  WebMessagePayloadView(WebMessagePayloadView&& other);

  WebMessagePayloadView(const WebMessagePayloadView&) = delete;
  void operator=(const WebMessagePayloadView&) = delete;

  // Get type of the payload.
  WebMessagePayloadType GetType() const { return type_; }

  // Get the String payload, only valid when type is kString.
  // Returns a reference to the string payload.
  std::u16string& GetString() {
    CHECK_EQ(type_, WebMessagePayloadType::kString);
    CHECK(string_value_.has_value());
    return string_value_.value();
  }

  // Get the String payload, only valid when type is kString.
  const std::u16string& GetString() const {
    CHECK_EQ(type_, WebMessagePayloadType::kString);
    CHECK(string_value_.has_value());
    return string_value_.value();
  }

  // Get the ArrayBuffer size, only valid when type is kArrayBuffer.
  size_t GetArrayBufferSize() const {
    CHECK_EQ(type_, WebMessagePayloadType::kArrayBuffer);
    switch (array_buffer_storage_type_) {
      case ArrayBufferStorageType::kTransferableMessage:
        return array_buffer_data_.size();
#if BUILDFLAG(IS_ANDROID)
      case ArrayBufferStorageType::kJavaArray:
        JNIEnv* env = base::android::AttachCurrentThread();
        jbyteArray j_byte_array = array_buffer_java_ref_.obj();
        CHECK(j_byte_array);
        size_t j_size = env->GetArrayLength(j_byte_array);
        base::android::CheckException(env);
        return j_size;
#endif
    }
  }

  // Copy ArrayBuffer data to |dest|, only valid when type is kArrayBuffer.
  // The existing JNI API does have a good way to expose content of Java Array
  // to C++ without copy it first.
  // Returns the number of bytes copied, or 0 if copy is not performed.
  size_t CopyArrayBufferData(base::span<uint8_t> dest) const {
    CHECK_EQ(type_, WebMessagePayloadType::kArrayBuffer);
    switch (array_buffer_storage_type_) {
      case ArrayBufferStorageType::kTransferableMessage:
        CHECK_NE(array_buffer_data_.data(), nullptr);
        if (array_buffer_data_.size() > dest.size() ||
            array_buffer_data_.size() == 0) {
          return 0;
        }
        memcpy(dest.data(), array_buffer_data_.data(),
               array_buffer_data_.size());
        return array_buffer_data_.size();
#if BUILDFLAG(IS_ANDROID)
      case ArrayBufferStorageType::kJavaArray:
        JNIEnv* env = base::android::AttachCurrentThread();
        jbyteArray j_byte_array = array_buffer_java_ref_.obj();
        CHECK(j_byte_array);
        size_t j_size = env->GetArrayLength(j_byte_array);
        if (j_size > dest.size() || j_size == 0) {
          return 0;
        }
        base::android::CheckException(env);
        env->GetByteArrayRegion(j_byte_array, 0, j_size,
                                reinterpret_cast<jbyte*>(dest.data()));
        base::android::CheckException(env);
        return j_size;
#endif
    }
  }

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaLocalRef<jbyteArray>
  GetOrCreateArrayBufferJavaArray() const {
    CHECK_EQ(type_, WebMessagePayloadType::kArrayBuffer);
    switch (array_buffer_storage_type_) {
      case ArrayBufferStorageType::kTransferableMessage: {
        JNIEnv* env = base::android::AttachCurrentThread();
        jbyteArray j_byte_array = env->NewByteArray(array_buffer_data_.size());
        base::android::CheckException(env);
        env->SetByteArrayRegion(
            j_byte_array, 0, array_buffer_data_.size(),
            reinterpret_cast<const jbyte*>(array_buffer_data_.data()));
        base::android::CheckException(env);
        return base::android::ScopedJavaLocalRef<jbyteArray>(env, j_byte_array);
      }
      case ArrayBufferStorageType::kJavaArray:
        return base::android::ScopedJavaLocalRef<jbyteArray>(
            array_buffer_java_ref_);
    }
  }
#endif

 private:
  WebMessagePayloadType type_;
  absl::optional<TransferableMessage> message_;

  // String
  absl::optional<std::u16string> string_value_;

  // ArrayBuffer
  ArrayBufferStorageType array_buffer_storage_type_;
  base::span<const uint8_t> array_buffer_data_;
#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaGlobalRef<jbyteArray> array_buffer_java_ref_;
#endif
};

// To support exposing HTML message ports to Java, it is necessary to be able
// to encode and decode message data using the same serialization format as V8.
// That format is an implementation detail of V8, but we cannot invoke V8 in
// the browser process. Rather than IPC over to the renderer process to execute
// the V8 serialization code, we duplicate some of the serialization logic
// (just for simple string or array buffer messages) here. This is
// a trade-off between overall complexity / performance and code duplication.
// Fortunately, we only need to handle string messages and this serialization
// format is static, as it is a format we currently persist to disk via
// IndexedDB.

BLINK_COMMON_EXPORT TransferableMessage
EncodeWebMessagePayload(WebMessagePayloadView payload);

BLINK_COMMON_EXPORT absl::optional<WebMessagePayloadView>
DecodeToWebMessagePayload(TransferableMessage message);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_STRING_MESSAGE_CODEC_H_
