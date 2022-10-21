// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JS_INJECTION_COMMON_WEB_MESSAGE_MOJOM_TRAITS_H_
#define COMPONENTS_JS_INJECTION_COMMON_WEB_MESSAGE_MOJOM_TRAITS_H_

#include <string>

#include "components/js_injection/common/interfaces.mojom.h"
#include "components/js_injection/common/web_message.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/union_traits.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace mojo {

template <>
struct UnionTraits<js_injection::mojom::JsWebMessageDataView,
                   js_injection::JsWebMessage> {
  static std::u16string string_value(
      const js_injection::JsWebMessage& message) {
    return absl::get<std::u16string>(message.payload);
  }

  static mojo_base::BigBuffer array_buffer_value(
      const js_injection::JsWebMessage& message) {
    LOG(ERROR) << __PRETTY_FUNCTION__;
    auto& buffer = absl::get<std::vector<uint8_t>>(message.payload);
    LOG(ERROR) << __FUNCTION__ << " absl::get done";
    auto big_buffer = mojo_base::BigBuffer(buffer);
    LOG(ERROR) << __FUNCTION__ << " bigbuffer constructed, type: "
               << static_cast<int>(big_buffer.storage_type());
    return big_buffer;
  }

  static js_injection::mojom::JsWebMessageDataView::Tag GetTag(
      const js_injection::JsWebMessage& input) {
    if (absl::holds_alternative<std::u16string>(input.payload)) {
      return js_injection::mojom::JsWebMessageDataView::Tag::kStringValue;
    } else if (absl::holds_alternative<std::vector<uint8_t>>(input.payload)) {
      return js_injection::mojom::JsWebMessageDataView::Tag::kArrayBufferValue;
    }
    NOTREACHED() << "Unknown type for JsWebMessage.";
    return js_injection::mojom::JsWebMessageDataView::Tag::kStringValue;
  }

  static bool Read(js_injection::mojom::JsWebMessageDataView r,
                   js_injection::JsWebMessage* out);
};

}  // namespace mojo

#endif
