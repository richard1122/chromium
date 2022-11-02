// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/messaging/string_message_codec.h"

#include <vector>

#include "base/containers/buffer_iterator.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/blink/public/mojom/array_buffer/array_buffer_contents.mojom.h"

namespace blink {
namespace {

const uint32_t kVarIntShift = 7;
const uint32_t kVarIntMask = (1 << kVarIntShift) - 1;

const uint8_t kVersionTag = 0xFF;
const uint8_t kPaddingTag = '\0';
// serialization_tag, see v8/src/objects/value-serializer.cc
const uint8_t kOneByteStringTag = '"';
const uint8_t kTwoByteStringTag = 'c';
const uint8_t kArrayBuffer = 'B';
const uint8_t kArrayBufferTransferTag = 't';

const uint32_t kVersion = 10;

static size_t BytesNeededForUint32(uint32_t value) {
  size_t result = 0;
  do {
    result++;
    value >>= kVarIntShift;
  } while (value);
  return result;
}

void WriteUint8(uint8_t value, std::vector<uint8_t>* buffer) {
  buffer->push_back(value);
}

void WriteUint32(uint32_t value, std::vector<uint8_t>* buffer) {
  for (;;) {
    uint8_t b = (value & kVarIntMask);
    value >>= kVarIntShift;
    if (!value) {
      WriteUint8(b, buffer);
      break;
    }
    WriteUint8(b | (1 << kVarIntShift), buffer);
  }
}

void WriteBytes(const char* bytes,
                size_t num_bytes,
                std::vector<uint8_t>* buffer) {
  buffer->insert(buffer->end(), bytes, bytes + num_bytes);
}

bool ReadUint8(base::BufferIterator<const uint8_t>& iter, uint8_t* value) {
  if (const uint8_t* ptr = iter.Object<uint8_t>()) {
    *value = *ptr;
    return true;
  }
  return false;
}

bool ReadUint32(base::BufferIterator<const uint8_t>& iter, uint32_t* value) {
  *value = 0;
  uint8_t current_byte;
  int shift = 0;
  do {
    if (!ReadUint8(iter, &current_byte))
      return false;

    *value |= (static_cast<uint32_t>(current_byte & kVarIntMask) << shift);
    shift += kVarIntShift;
  } while (current_byte & (1 << kVarIntShift));
  return true;
}

bool ContainsOnlyLatin1(const std::u16string& data) {
  char16_t x = 0;
  for (char16_t c : data)
    x |= c;
  return !(x & 0xFF00);
}

}  // namespace

// static
WebMessagePayloadView WebMessagePayloadView::NewArrayBuffer(
    TransferableMessage&& message,
    base::span<const uint8_t> data) {
  WebMessagePayloadView view;
  view.message_.emplace(std::move(message));
  view.type_ = WebMessagePayloadType::kArrayBuffer;
  view.array_buffer_storage_type_ =
      ArrayBufferStorageType::kTransferableMessage;
  view.array_buffer_data_ = data;
  return view;
}

#if BUILDFLAG(IS_ANDROID)
// static
WebMessagePayloadView WebMessagePayloadView::NewArrayBuffer(
    base::android::ScopedJavaLocalRef<jbyteArray> java_ref) {
  WebMessagePayloadView view;
  view.type_ = WebMessagePayloadType::kArrayBuffer;
  view.array_buffer_storage_type_ = ArrayBufferStorageType::kJavaArray;
  view.array_buffer_java_ref_ = java_ref;
  return view;
}
#endif

// static
WebMessagePayloadView WebMessagePayloadView::NewString(std::u16string string) {
  WebMessagePayloadView view;
  view.type_ = WebMessagePayloadType::kString;
  view.string_value_ = std::move(string);
  return view;
}

WebMessagePayloadView::WebMessagePayloadView(WebMessagePayloadView&& other) {
  switch (other.type_) {
    case WebMessagePayloadType::kString:
      type_ = WebMessagePayloadType::kString;
      string_value_ = std::move(other.string_value_);
      break;
    case WebMessagePayloadType::kArrayBuffer:
      type_ = WebMessagePayloadType::kArrayBuffer;
      array_buffer_storage_type_ = other.array_buffer_storage_type_;
      switch (array_buffer_storage_type_) {
        case ArrayBufferStorageType::kTransferableMessage:
          message_ = std::move(other.message_);
          array_buffer_data_ = other.array_buffer_data_;
          break;
#if BUILDFLAG(IS_ANDROID)
        case ArrayBufferStorageType::kJavaArray:
          array_buffer_java_ref_ = std::move(other.array_buffer_java_ref_);
          break;
#endif
      }
      break;
    default:
      NOTREACHED() << "Invalid type: " << static_cast<int>(type_);
      break;
  }
  other.type_ = WebMessagePayloadType::kInvalid;
}

TransferableMessage EncodeWebMessagePayload(WebMessagePayloadView payload) {
  TransferableMessage message;
  std::vector<uint8_t> buffer;
  WriteUint8(kVersionTag, &buffer);
  WriteUint32(kVersion, &buffer);

  if (payload.GetType() == WebMessagePayloadType::kString) {
    auto& str = payload.GetString();
    if (ContainsOnlyLatin1(str)) {
      std::string data_latin1(str.cbegin(), str.cend());
      WriteUint8(kOneByteStringTag, &buffer);
      WriteUint32(data_latin1.size(), &buffer);
      WriteBytes(data_latin1.c_str(), data_latin1.size(), &buffer);
    } else {
      size_t num_bytes = str.size() * sizeof(char16_t);
      if ((buffer.size() + 1 + BytesNeededForUint32(num_bytes)) & 1)
        WriteUint8(kPaddingTag, &buffer);
      WriteUint8(kTwoByteStringTag, &buffer);
      WriteUint32(num_bytes, &buffer);
      WriteBytes(reinterpret_cast<const char*>(str.data()), num_bytes, &buffer);
    }
  } else if (payload.GetType() == WebMessagePayloadType::kArrayBuffer) {
    WriteUint8(kArrayBufferTransferTag, &buffer);
    // Write at the first slot.
    WriteUint32(0, &buffer);

    mojo_base::BigBuffer big_buffer(payload.GetArrayBufferSize());
    auto span = base::make_span(big_buffer.data(), big_buffer.size());
    payload.CopyArrayBufferData(span);
    message.array_buffer_contents_array.push_back(
        mojom::SerializedArrayBufferContents::New(std::move(big_buffer)));
  } else {
    NOTREACHED() << "Invalid payload type.";
  }

  message.owned_encoded_message = std::move(buffer);
  message.encoded_message = message.owned_encoded_message;

  return message;
}

absl::optional<WebMessagePayloadView> DecodeToWebMessagePayload(
    TransferableMessage message) {
  base::BufferIterator<const uint8_t> iter(message.encoded_message);
  uint8_t tag;

  // Discard the outer envelope, including trailer info if applicable.
  if (!ReadUint8(iter, &tag))
    return absl::nullopt;
  if (tag == kVersionTag) {
    uint32_t version = 0;
    if (!ReadUint32(iter, &version))
      return absl::nullopt;
    static constexpr uint32_t kMinWireFormatVersionWithTrailer = 21;
    if (version >= kMinWireFormatVersionWithTrailer) {
      // In these versions, we expect kTrailerOffsetTag (0xFE) followed by an
      // offset and size. See details in
      // third_party/blink/renderer/core/v8/serialization/serialization_tag.h.
      auto span = iter.Span<uint8_t>(1 + sizeof(uint64_t) + sizeof(uint32_t));
      if (span.empty() || span[0] != 0xFE)
        return absl::nullopt;
    }
    if (!ReadUint8(iter, &tag))
      return absl::nullopt;
  }

  // Discard any leading version and padding tags.
  while (tag == kVersionTag || tag == kPaddingTag) {
    uint32_t version;
    if (tag == kVersionTag && !ReadUint32(iter, &version))
      return absl::nullopt;
    if (!ReadUint8(iter, &tag))
      return absl::nullopt;
  }

  switch (tag) {
    case kOneByteStringTag: {
      // Use of unsigned char rather than char here matters, so that Latin-1
      // characters are zero-extended rather than sign-extended
      uint32_t num_bytes;
      if (!ReadUint32(iter, &num_bytes))
        return absl::nullopt;
      auto span = iter.Span<unsigned char>(num_bytes / sizeof(unsigned char));
      std::u16string str(span.begin(), span.end());
      return span.size_bytes() == num_bytes
                 ? absl::make_optional(
                       WebMessagePayloadView::NewString(std::move(str)))
                 : absl::nullopt;
    }
    case kTwoByteStringTag: {
      uint32_t num_bytes;
      if (!ReadUint32(iter, &num_bytes))
        return absl::nullopt;
      auto span = iter.Span<char16_t>(num_bytes / sizeof(char16_t));
      std::u16string str(span.begin(), span.end());
      return span.size_bytes() == num_bytes
                 ? absl::make_optional(
                       WebMessagePayloadView::NewString(std::move(str)))
                 : absl::nullopt;
    }
    case kArrayBuffer: {
      uint32_t num_bytes;
      if (!ReadUint32(iter, &num_bytes))
        return absl::nullopt;
      auto span = iter.Span<uint8_t>(num_bytes);
      return span.size_bytes() == num_bytes
                 ? absl::make_optional(WebMessagePayloadView::NewArrayBuffer(
                       std::move(message), span))
                 : absl::nullopt;
    }
    case kArrayBufferTransferTag: {
      uint32_t array_buffer_index;
      if (!ReadUint32(iter, &array_buffer_index))
        return absl::nullopt;
      // We only support transfer ArrayBuffer at the first index.
      if (array_buffer_index != 0)
        return absl::nullopt;
      if (message.array_buffer_contents_array.size() != 1)
        return absl::nullopt;
      const auto& big_buffer = message.array_buffer_contents_array[0]->contents;
      const auto span = base::make_span(big_buffer.data(), big_buffer.size());
      return WebMessagePayloadView::NewArrayBuffer(std::move(message), span);
    }
  }

  DLOG(WARNING) << "Unexpected tag: " << tag;
  return absl::nullopt;
}

}  // namespace blink
