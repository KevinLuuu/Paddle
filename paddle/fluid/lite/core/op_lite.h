// Copyright (c) 2019 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <glog/logging.h>
#include <boost/variant.hpp>
#include <map>
#include <string>
#include "paddle/fluid/framework/lod_tensor.h"
#include "paddle/fluid/framework/op_desc.h"
#include "paddle/fluid/framework/variable.h"
#include "paddle/fluid/lite/core/context.h"
#include "paddle/fluid/lite/core/kernel.h"
#include "paddle/fluid/lite/core/scope.h"

namespace paddle {
namespace lite {

using any_t = boost::variant<int, float, framework::Variable *>;
using anys_t = std::map<std::string, any_t>;

// For registry factory.
struct Registry {
  void Touch() {}
};

/**
 * The base class of an light-weight operators, currently just used in inference
 * to eliminate overhead of some operations in current framework.
 *
 * The Operator are designed as follows:
 * - it can has some members to hold the argument and some other computation
 * resources,
 * - it should act like a function call, no more logic included.
 */
class OpLite : public Registry {
 public:
  // The strategies to pick a kernel from candidates.
  enum class KernelStrategy {
    // Return the user specified one.
    kStatic = 0,
    // Specify the expected kernel externally.
    kSpecified,
    // Run each kernel to evaluate and get the best kernel.
    kRuntime,
  };

  struct Place {
    TargetType target{TARGET(kHost)};
    PrecisionType precision{PRECISION(kFloat)};
  };

  OpLite() = default;
  OpLite(std::unique_ptr<OpContext> &&x) : op_context_(std::move(x)) {}

  // Check the shape.
  virtual bool CheckShape() const { return true; }
  // Inference the outputs' shape.
  virtual bool InferShape() const { return true; }
  // Run this operator.
  virtual bool Run() {
    CHECK(kernel_);
    SyncInputEvents();

    kernel_->Run();

    RecordOutputEvents();
    return true;
  }
  // Build the operator, attach it with the runtime environment.
  virtual bool Build(const framework::OpDesc &opdesc, lite::Scope *scope) = 0;
  // Human-readable information.
  virtual std::string DebugString() const = 0;

  const Place &kernel_place() const { return kernel_place_; }

 protected:
  void PickKernel(const std::vector<Place> &valid_places,
                  KernelStrategy kernel_strategy = KernelStrategy::kStatic);

  // Specify the kernel to run by default. This will specify the value of
  // `kernel_place_`.
  virtual void StaticPickKernel(const std::vector<Place> &valid_targets) = 0;

  // Wait until all the inputs' events are ready.
  void SyncInputEvents() {}

  // Record the output events, and that will tell all the dependent operators
  // some inputs are ready.
  void RecordOutputEvents() {}

  // Create all the kernels for the valid targets.
  std::vector<std::unique_ptr<KernelBase>> CreateKernels(
      const std::vector<Place> &places);

  virtual ~OpLite() = default;

 protected:
  std::unique_ptr<OpContext> op_context_;
  Place kernel_place_;
  std::unique_ptr<KernelBase> kernel_;
  std::string op_type_;
};

}  // namespace lite
}  // namespace paddle