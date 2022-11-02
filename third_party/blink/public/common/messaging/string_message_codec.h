// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_STRING_MESSAGE_CODEC_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_STRING_MESSAGE_CODEC_H_

#include <jni.h>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"

namespace blink {

// Represent WebMessage payload type between browser and renderer process.
// std::vector<uint8_t>: the ArrayBuffer.
using WebMessagePayload = absl::variant<std::u16string, std::vector<uint8_t>>;

enum class WebMessagePayloadType {
  kInvalid = 0,
  kString,
  kArrayBuffer,
};

class WebMessagePayloadView {
 private:
  enum class ArrayBufferStorageType {
    kInvalid = 0,
    kTransferableMessage,
    kJavaArray,
  };

 public:
  WebMessagePayloadView() = default;
  WebMessagePayloadView(WebMessagePayloadView&& other)
      : type_(other.type_), message_(std::move(other.message_)) {
    switch (type_) {
      case WebMessagePayloadType::kInvalid:
        return;
      case WebMessagePayloadType::kString:
        string_value_ = std::move(other.string_value_);
        return;
      case WebMessagePayloadType::kArrayBuffer:
        array_buffer_storage_type_ = other.array_buffer_storage_type_;
        switch (array_buffer_storage_type_) {
          case ArrayBufferStorageType::kInvalid:
            return;
          case ArrayBufferStorageType::kTransferableMessage:
            array_buffer_data_ = other.array_buffer_data_;
            return;
          case ArrayBufferStorageType::kJavaArray:
            array_buffer_java_ref_ = std::move(other.array_buffer_java_ref_);
            return;
        }
    }
    NOTREACHED() << "Invalid type: " << static_cast<int>(type_);
  }

  WebMessagePayloadView(const WebMessagePayloadView&) = delete;
  void operator=(const WebMessagePayloadView&) = delete;

  static WebMessagePayloadView NewArrayBuffer(TransferableMessage&& message,
                                              base::span<const uint8_t> data) {
    WebMessagePayloadView view;
    view.message_.emplace(std::move(message));
    view.type_ = WebMessagePayloadType::kArrayBuffer;
    view.array_buffer_storage_type_ =
        ArrayBufferStorageType::kTransferableMessage;
    view.array_buffer_data_ = data;
    return view;
  }

  static WebMessagePayloadView NewArrayBuffer(
      base::android::ScopedJavaLocalRef<jbyteArray> java_ref) {
    WebMessagePayloadView view;
    view.type_ = WebMessagePayloadType::kArrayBuffer;
    view.array_buffer_storage_type_ = ArrayBufferStorageType::kJavaArray;
    view.array_buffer_java_ref_ = java_ref;
    return view;
  }

  static WebMessagePayloadView NewString(std::u16string string) {
    WebMessagePayloadView view;
    view.type_ = WebMessagePayloadType::kString;
    view.string_value_ = std::move(string);
    return view;
  }

  WebMessagePayloadType GetType() const { return type_; }
  std::u16string& GetString() {
    CHECK_EQ(type_, WebMessagePayloadType::kString);
    CHECK(string_value_.has_value());
    return string_value_.value();
  }
  size_t CopyArrayBufferData(base::span<uint8_t> dest) const {
    CHECK_EQ(type_, WebMessagePayloadType::kArrayBuffer);
    switch (array_buffer_storage_type_) {
      case ArrayBufferStorageType::kInvalid:
        return 0;
      case ArrayBufferStorageType::kTransferableMessage:
        CHECK_NE(array_buffer_data_.data(), nullptr);
        if (array_buffer_data_.size() > dest.size() ||
            array_buffer_data_.size() == 0) {
          return 0;
        }
        memcpy(dest.data(), array_buffer_data_.data(),
               array_buffer_data_.size());
        return array_buffer_data_.size();
      case ArrayBufferStorageType::kJavaArray:
        JNIEnv* env = base::android::AttachCurrentThread();
        jbyteArray j_byte_array = array_buffer_java_ref_.obj();
        if (!j_byte_array) {
          return 0;
        }
        size_t j_size = env->GetArrayLength(j_byte_array);
        if (j_size > dest.size() || j_size == 0) {
          return 0;
        }
        env->GetByteArrayRegion(j_byte_array, 0, j_size,
                                reinterpret_cast<jbyte*>(dest.data()));
        return j_size;
    }
  }

 private:
  WebMessagePayloadType type_;
  absl::optional<TransferableMessage> message_;

  // String
  absl::optional<std::u16string> string_value_;

  // ArrayBuffer
  ArrayBufferStorageType array_buffer_storage_type_{
      ArrayBufferStorageType::kInvalid};
  base::span<const uint8_t> array_buffer_data_;
  base::android::ScopedJavaGlobalRef<jbyteArray> array_buffer_java_ref_;
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
EncodeWebMessagePayload(const WebMessagePayload& payload);

BLINK_COMMON_EXPORT absl::optional<WebMessagePayload> DecodeToWebMessagePayload(
    const TransferableMessage& message);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_STRING_MESSAGE_CODEC_H_
