// Licensed under the Apache License, Version 2.0.
// Author: Jin Qing (http://blog.csdn.net/jq0123)

#ifndef GRPC_CB_CORE_CLIENT_ASYNC_CALL_CQTAG_H
#define GRPC_CB_CORE_CLIENT_ASYNC_CALL_CQTAG_H

#include <string>

#include <grpc_cb_core/client/impl/client_call_cqtag.h>  // for ClientCallCqTag
#include <grpc_cb_core/client/status_callback.h>         // for ErrorCallback
#include <grpc_cb_core/common/support/config.h>          // for GRPC_FINAL

namespace grpc_cb_core {

// Completion queue tag (CqTag) for client async call.
// Derived from ClientCallCqTag, adding on_response, on_error.
class ClientAsyncCallCqTag GRPC_FINAL : public ClientCallCqTag {
 public:
  explicit ClientAsyncCallCqTag(const CallSptr call_sptr)
     : ClientCallCqTag(call_sptr) {}

 public:
  using OnResponse = std::function<void (const std::string&)>;
  void SetOnResponse(const OnResponse& on_response) {
    on_response_ = on_response;
  }
  void SetOnError(const ErrorCallback& on_error) {
    on_error_ = on_error;
  }

 public:
  void DoComplete(bool success) GRPC_OVERRIDE {
    if (!success) {
      CallOnError(Status::InternalError("ClientAsyncCallCqTag failed."));
      return;
    }

    std::string resp;
    Status status = GetResponse(resp);
    if (status.ok()) {
      if (on_response_)
        on_response_(resp);
      return;
    }
    CallOnError(status);
  };

 private:
  void CallOnError(const Status& status) const {
    if (on_error_)
      on_error_(status);
  }

 private:
  OnResponse on_response_;
  ErrorCallback on_error_;
};  // class ClientAsyncCallCqTag

}  // namespace grpc_cb_core
#endif  // GRPC_CB_CORE_CLIENT_ASYNC_CALL_CQTAG_H