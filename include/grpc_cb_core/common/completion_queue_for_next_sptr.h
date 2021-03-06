// Shared pointer to complete queue for next.
#ifndef GRPC_CB_CORE_COMPLETION_QUEUE_FOR_NEXT_SPTR_H
#define GRPC_CB_CORE_COMPLETION_QUEUE_FOR_NEXT_SPTR_H

#include <memory>

namespace grpc_cb_core {

class CompletionQueueForNext;
using CompletionQueueForNextSptr = std::shared_ptr<CompletionQueueForNext>;

}  // namespace grpc_cb_core
#endif  // GRPC_CB_CORE_COMPLETION_QUEUE_FOR_NEXT_SPTR_H
