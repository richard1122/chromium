// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/js_injection/common/web_message_mojom_traits.h"

#include <string>
#include "base/notreached.h"
#include "components/js_injection/common/interfaces.mojom.h"

namespace mojo {

// static
bool UnionTraits<js_injection::mojom::JsWebMessageDataView,
                 js_injection::JsWebMessage>::
    Read(js_injection::mojom::JsWebMessageDataView r,
         js_injection::JsWebMessage* out) {
  LOG(ERROR) << __PRETTY_FUNCTION__;
  if (r.is_string_value()) {
    std::u16string string_value;
    if (!r.ReadStringValue(&string_value))
      return false;
    out->payload = std::move(string_value);
    return true;
  } else if (r.is_array_buffer_value()) {
    LOG(ERROR) << __FUNCTION__ << " is_array_buffer_value";
    mojo_base::BigBufferView array_buffer_view;
    if (!r.ReadArrayBufferValue(&array_buffer_view))
      return false;
    LOG(ERROR) << __FUNCTION__ << " ReadArrayBufferValue done.";
    out->payload = std::vector<uint8_t>(array_buffer_view.data().begin(),
                                        array_buffer_view.data().end());
    LOG(ERROR) << __FUNCTION__ << " assign bigbuffer to absl::variant done.";
    return true;
  } else {
    NOTREACHED() << "Unknown type for JsWebMessage mojo.";
    return false;
  }
}

}  // namespace mojo
