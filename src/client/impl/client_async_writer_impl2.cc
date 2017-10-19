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
  SetInternalError("Failed to init client stream.");
  // Call close handler when Close(CloseHandler)
}

ClientAsyncWriterImpl2::~ClientAsyncWriterImpl2() {
  // Have done CallCloseCb().
}

bool ClientAsyncWriterImpl2::Write(const std::string& request) {
  Guard g(mtx_);
  if (!status_.ok())
    return false;
  if (writing_closing_ || writing_ended_)
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
  // DEL
  //auto writer_sptr(writer_sptr_);
  //writer_sptr_.reset();  // always stop circular sharing

  if (!status_.ok()) {
    CallCloseCb();
    return;
  }

  // XXX
  //if (writer_) {
  //  writer_->SetClosing();  // May trigger OnEndOfWriting().  XXX
  //} else {
  //  writing_ended_ = true;  // Ended without start.
  //  SendCloseIfNot();
  //}
}  // Close()

void ClientAsyncWriterImpl2::OnWritten(bool success) {
    // XXX
  //assert(writer_);
  //writer_->OnWritten(success);
  // TryToWriteNext();
  // XXX
}  // OnWritten()

// Finally close...
void ClientAsyncWriterImpl2::SendCloseIfNot() {
  // private function need no Guard.
  assert(writing_ended_);  // Must be ended.
  if (!status_.ok())
    return;

  if (has_sent_close_) return;
  has_sent_close_ = true;
  auto sptr = shared_from_this();
  auto* tag = new ClientWriterCloseCqTag(call_sptr_);
  tag->SetCompleteCb([sptr, tag](bool success) {
    sptr->OnClosed(success, *tag);
  });
  if (tag->Start())
    return;

  delete tag;
  SetInternalError("Failed to close client stream.");  // Calls CallCloseCb();
}  // SendCloseIfNot()

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

void ClientAsyncWriterImpl2::OnEndOfWriting() {
  Guard g(mtx_);  // Callback need Guard.
  // XXX assert(writer_);
  assert(!writing_ended_);  // call OnEndOfWriting() only once
  writing_ended_ = true;

  if (!status_.ok()) return;
  // XXX status_ = writer_->GetStatus();  // XXX
  if (status_.ok())
    SendCloseIfNot();
  else
    CallCloseCb();
}  // OnEndOfWriting()

void ClientAsyncWriterImpl2::SetInternalError(const std::string& sError) {
  // private function need no Guard.
  status_.SetInternalError(sError);
  CallCloseCb();
  writing_ended_ = true;
  // XXX
  //if (writer_)
  //  writer_->Abort();  // XXX
}

bool ClientAsyncWriterImpl2::TryToWriteNext() {
  assert(!is_writing_);
  assert(!msg_queue_.empty());
  assert(status_.ok());

  is_writing_ = true;
  auto* tag = new ClientSendMsgCqTag(call_sptr_);
  auto sptr = shared_from_this();  // CqTag will keep sptr
  CompleteCb complete_cb = [sptr](bool success) {
    sptr->OnWritten(success);
  };
  tag->SetCompleteCb(complete_cb);

  bool ok = tag->Start(msg_queue_.front());
  msg_queue_.pop();  // may empty now but is_writing_
  if (ok) return true;

  delete tag;
  status_.SetInternalError("Failed to write client-side streaming.");
  // XXX CallEndCb();  // error end  XXX OnEndOfWriting()?
  return false;
}

}  // namespace grpc_cb_core
