// Copyright 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#pragma once

#ifdef TRITON_ENABLE_CUDA_CTX_SHARING
#include <cuda.h>
#endif  // TRITON_ENABLE_CUDA_CTX_SHARING
#include <sstream>

#include "triton/backend/backend_model.h"

namespace triton { namespace backend { namespace tensorrt {

class TensorRTModel : public BackendModel {
 public:
  TensorRTModel(TRITONBACKEND_Model* triton_model);
  virtual ~TensorRTModel() = default;

  template <typename T>
  TRITONSERVER_Error* GetParameter(std::string const& name, T& value)
  {
    assert(false);
    auto dummy = T();
    return dummy;
  }

  TRITONSERVER_Error* SetTensorRTModelConfig();

  TRITONSERVER_Error* ParseModelConfig();

  // The model configuration.
  common::TritonJson::Value& GraphSpecs() { return graph_specs_; }

  enum Priority { DEFAULT = 0, MIN = 1, MAX = 2 };
  Priority ModelPriority() { return priority_; }
  int GetCudaStreamPriority();
  bool UseCudaGraphs() { return use_cuda_graphs_; }
  size_t GatherKernelBufferThreshold()
  {
    return gather_kernel_buffer_threshold_;
  }
  bool SeparateOutputStream() { return separate_output_stream_; }
  bool EagerBatching() { return eager_batching_; }
  bool BusyWaitEvents() { return busy_wait_events_; }

  void* StringToPointer(std::string& str)
  {
    std::stringstream ss;
    ss << str;

    void* ctx_ptr;
    ss >> ctx_ptr;
    return ctx_ptr;
  }

  //! Following functions are related to custom Cuda context (Cuda in Graphics)
  //! sharing for gaming use case. Creating a shared contexts reduces context
  //! switching overhead and leads to better performance of model execution
  //! along side Graphics workload.

  bool IsCudaContextSharingEnabled()
  {
#ifdef TRITON_ENABLE_CUDA_CTX_SHARING
    return cuda_ctx != nullptr;
#else
    return false;
#endif  // TRITON_ENABLE_CUDA_CTX_SHARING
  }

  inline TRITONSERVER_Error* PushCudaContext()
  {
#ifdef TRITON_ENABLE_CUDA_CTX_SHARING
    if (CUDA_SUCCESS != cuCtxPushCurrent(cuda_ctx)) {
      return TRITONSERVER_ErrorNew(
          TRITONSERVER_ERROR_INTERNAL,
          (std::string("unable to push Cuda context for ") + Name()).c_str());
    }
#endif  // TRITON_ENABLE_CUDA_CTX_SHARING
    return nullptr;
  }

  inline TRITONSERVER_Error* PopCudaContext()
  {
#ifdef TRITON_ENABLE_CUDA_CTX_SHARING
    CUcontext oldCtx{};
    if (CUDA_SUCCESS != cuCtxPopCurrent(&oldCtx)) {
      return TRITONSERVER_ErrorNew(
          TRITONSERVER_ERROR_INTERNAL,
          (std::string("unable to pop Cuda context for ") + Name()).c_str());
    }
    if (oldCtx != cuda_ctx) {
      return TRITONSERVER_ErrorNew(
          TRITONSERVER_ERROR_INTERNAL,
          (std::string("popping the wrong Cuda context for ") + Name())
              .c_str());
    }
#endif  // TRITON_ENABLE_CUDA_CTX_SHARING
    return nullptr;
  }

 protected:
  common::TritonJson::Value graph_specs_;
  Priority priority_;
  bool use_cuda_graphs_;
  size_t gather_kernel_buffer_threshold_;
  bool separate_output_stream_;
  bool eager_batching_;
  bool busy_wait_events_;
#ifdef TRITON_ENABLE_CUDA_CTX_SHARING
  CUcontext cuda_ctx = nullptr;
#endif  // TRITON_ENABLE_CUDA_CTX_SHARING
};

template <>
TRITONSERVER_Error* TensorRTModel::GetParameter<std::string>(
    std::string const& name, std::string& str_value);

struct ScopedRuntimeCudaContext {
  ScopedRuntimeCudaContext(TensorRTModel* model_state)
      : model_state_(model_state)
  {
#ifdef TRITON_ENABLE_CUDA_CTX_SHARING
    if (model_state_->IsCudaContextSharingEnabled()) {
      THROW_IF_BACKEND_MODEL_ERROR(model_state_->PushCudaContext());
    }
#endif  // TRITON_ENABLE_CUDA_CTX_SHARING
  }
  ~ScopedRuntimeCudaContext()
  {
#ifdef TRITON_ENABLE_CUDA_CTX_SHARING
    if (model_state_->IsCudaContextSharingEnabled()) {
      THROW_IF_BACKEND_MODEL_ERROR(model_state_->PopCudaContext());
    }
#endif  // TRITON_ENABLE_CUDA_CTX_SHARING
  }
  TensorRTModel* model_state_;
};

}}}  // namespace triton::backend::tensorrt
