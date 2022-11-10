// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/android/message_payload.h"
#include <cstddef>
#include <string>

#include "base/containers/span.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/mojom/array_buffer/array_buffer_contents.mojom.h"

namespace content {
namespace {

TEST(MessagePayloadTest, SelfTest_String) {
  std::u16string string = u"Hello";

  auto generated_message = android::ConvertToWebMessagePayloadFromJava(
      android::ConvertWebMessagePayloadToJava(
          blink::WebMessagePayloadView::NewString(string)));
  EXPECT_EQ(string, generated_message.GetString());
}

TEST(MessagePayloadTest, SelfTest_ArrayBuffer) {
  std::vector<uint8_t> data(200, 0XFF);
  blink::TransferableMessage message;
  message.array_buffer_contents_array.emplace_back(
      blink::mojom::SerializedArrayBufferContents::New(
          mojo_base::BigBuffer(data)));
  auto generated_message = android::ConvertToWebMessagePayloadFromJava(
      android::ConvertWebMessagePayloadToJava(
          blink::WebMessagePayloadView::NewArrayBuffer(
              std::move(message),
              base::make_span(
                  message.array_buffer_contents_array[0]->contents.data(),
                  message.array_buffer_contents_array[0]->contents.size()))));

  std::vector<uint8_t> test_result(200);
  generated_message.CopyArrayBufferData(base::make_span(test_result));
  EXPECT_EQ(data, test_result);
}

}  // namespace
}  // namespace content
