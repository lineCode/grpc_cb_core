// Licensed under the Apache License, Version 2.0.
// Author: Jin Qing (http://blog.csdn.net/jq0123)

#include <grpc_cb_core/server/server_replier.h>

#include <grpc_cb_core/common/call_sptr.h>  // for CallSptr
#include "impl/server_replier_impl.h"       // for ServerReplierImpl

namespace grpc_cb_core {

ServerReplier::ServerReplier(const CallSptr& call_sptr)
    : impl_sptr_(new ServerReplierImpl(call_sptr)) {
  assert(call_sptr);
}

ServerReplier::~ServerReplier() {}

void ServerReplier::ReplyStr(const std::string& response) const {
  impl_sptr_->ReplyStr(response);
}

void ServerReplier::ReplyError(const Status& status) const {
  impl_sptr_->ReplyError(status);
}

}  // namespace grpc_cb_core
