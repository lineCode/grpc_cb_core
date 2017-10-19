// Licensed under the Apache License, Version 2.0.
// Author: Jin Qing (http://blog.csdn.net/jq0123)

#include "client_async_writer_impl2.h"

#include <cassert>  // for assert()

#include <grpc_cb_core/client/channel.h>  // for MakeSharedCall()

#include "client_send_init_md_cqtag.h"  // for ClientSendInitMdCqTag
#include "client_send_msg_cqtag.h"      // for ClientSendMsgCqTag
#include "client_writer_close_cqtag.h"  // for ClientWriterCloseCqTag
#include "common/impl/complete_cb.h"    // for CompleteCb

namespace grpc_cb_core {

ClientAsyncWriterImpl2::ClientAsyncWriterImpl2(
    const ChannelSptr& channel, const std::string& method,
    const CompletionQueueSptr& cq_sptr, int64_t timeout_ms)
    : cq_sptr_(cq_sptr),
      call_sptr_(channel->MakeSharedCall(method, *cq_sptr, timeout_ms)) {
  assert(cq_sptr);
  assert(channel);
  assert(call_sptr_);

  ClientSendInitMdCqTag* tag = new ClientSendInitMdCqTag(call_sptr_);
  if (tag->Start()) return;
  delete tag;
  status_.SetInternalError("Failed to init client stream.");
  // Call close handler when Close(CloseHandler)
}

ClientAsyncWriterImpl2::~ClientAsyncWriterImpl2() {
  // Have done CallCloseCb().
}

// Queue messages -> Send messages one by one
// Close writing -> Send messages -> Send close

bool ClientAsyncWriterImpl2::Write(const std::string& request) {
  Guard g(mtx_);
  if (!status_.ok())
    return false;
  if (writing_closing_)
    return false;

  msg_queue_.push(request);
  if (is_writing_) return true;
  return TryToWriteNext();
}  // Write()

void ClientAsyncWriterImpl2::Close(const CloseCb& close_cb/* = nullptr*/) {
  Guard g(mtx_);
  if (writing_closing_) return;  // already done
  writing_closing_ = true;
  close_cb_ = close_cb;  // reset in CallCloseCb()

  if (!status_.ok()) {
    CallCloseCb();
    return;
  }

  if (is_writing_) return;
  assert(msg_queue_.empty());
  SendClose();
}  // Close()

void ClientAsyncWriterImpl2::OnWritten(bool success) {
  Guard g(mtx_);  // Callback needs Guard.
  assert(is_writing_);
  is_writing_ = false;

  if (!status_.ok()) return;  // XXX
  if (!success) {
    status_.SetInternalError("Failed to send message.");
    // XXX CallEndCb();  // error end  XXX
    return;
  }

  if (!msg_queue_.empty()) {
    TryToWriteNext();  // XXX check return?
    return;
  }

  // All messages are sent. Wait for Close()
  if (writing_closing_)
    SendClose();  // normal end
}  // OnWritten()

// Finally close...
void ClientAsyncWriterImpl2::SendClose() {
  // private function need no Guard.
  assert(writing_closing_);  // Must after Close().
  assert(status_.ok());
  assert(!has_sent_close_);
  has_sent_close_ = true;

  auto sptr = shared_from_this();
  auto* tag = new ClientWriterCloseCqTag(call_sptr_);
  tag->SetCompleteCb([sptr, tag](bool success) {
    sptr->OnClosed(success, *tag);
  });
  if (tag->Start())
    return;

  delete tag;
  status_.SetInternalError("Failed to close client stream.");
  CallCloseCb();
}  // SendClose()

void ClientAsyncWriterImpl2::CallCloseCb(const std::string& sMsg/* = ""*/) {
  // private function need no Guard.
  if (!close_cb_) return;
  close_cb_(status_, sMsg);
  close_cb_ = nullptr;
}

// Callback of ClientWriterCloseCqTag::OnComplete()
void ClientAsyncWriterImpl2::OnClosed(bool success, ClientWriterCloseCqTag& tag) {
  Guard g(mtx_);  // Callback need Guard.

  if (!tag.IsStatusOk()) {
    status_ = tag.GetStatus();
    CallCloseCb();
    return;
  }

  // Todo: Get trailing metadata.
  std::string sMsg;
  status_ = tag.GetResponse(sMsg);
  CallCloseCb(sMsg);
}  // OnClosed()

bool ClientAsyncWriterImpl2::TryToWriteNext() {
  assert(!is_writing_);
  assert(!msg_queue_.empty());
  assert(status_.ok());

  is_writing_ = true;
  auto* tag = new ClientSendMsgCqTag(call_sptr_);
  auto sptr = shared_from_this();  // CqTag will keep sptr
  CompleteCb complete_cb = [sptr](bool success) {
    sptr->OnWritten(success);  // XXX Rename to OnSent
  };
  tag->SetCompleteCb(complete_cb);

  bool ok = tag->Start(msg_queue_.front());
  msg_queue_.pop();  // may empty now but is_writing_
  if (ok) return true;

  delete tag;
  status_.SetInternalError("Failed to write client-side streaming.");
  if (writing_closing_) CallCloseCb();
  return false;
}

}  // namespace grpc_cb_core
