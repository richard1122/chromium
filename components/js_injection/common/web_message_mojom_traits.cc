// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/js_injection/common/web_message_mojom_traits.h"

#include <string>
#include "base/notreached.h"
#include "components/js_injection/common/interfaces.mojom.h"
#include "mojo/public/cpp/base/big_buffer.h"

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
    mojo_base::BigBuffer big_buffer;
    if (!r.ReadArrayBufferValue(&big_buffer))
      return false;
    LOG(ERROR) << __FUNCTION__ << " ReadArrayBufferValue done.";
    out->payload = std::move(big_buffer);
    LOG(ERROR) << __FUNCTION__ << " assign bigbuffer to absl::variant done.";
    return true;
  } else {
    NOTREACHED() << "Unknown type for JsWebMessage mojo.";
    return false;
  }
}

}  // namespace mojo
