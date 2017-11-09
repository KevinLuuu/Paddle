/* Copyright (c) 2016 PaddlePaddle Authors. All Rights Reserve.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#include "paddle/operators/expand_op.h"

namespace paddle {
namespace operators {

using framework::Tensor;

class ExpandOp : public framework::OperatorWithKernel {
 public:
  using framework::OperatorWithKernel::OperatorWithKernel;

 protected:
  void InferShape(framework::InferShapeContext* ctx) const override {
    PADDLE_ENFORCE(ctx->HasInput("X"), "Input(X) must be initialized.");
    std::vector<int> expand_times =
        ctx->Attrs().Get<std::vector<int>>("expandTimes");
    auto x_dims = ctx->GetInputDim("X");

    PADDLE_ENFORCE_EQ(static_cast<size_t>(x_dims.size()), expand_times.size(),
                      "The number of Attr(expandTimes)'s value must be equal "
                      "to the rank of Input(X).");
    PADDLE_ENFORCE_LE(x_dims.size(), 6,
                      "The rank of Input(X) must not be greater than 6.");

    std::vector<int64_t> out_shape(x_dims.size());
    for (size_t i = 0; i < expand_times.size(); ++i) {
      PADDLE_ENFORCE_GE(expand_times[i], 1,
                        "Each value of Attr(expandTimes) should not be "
                        "less than 1.");
      out_shape[i] = x_dims[i] * expand_times[i];
    }

    ctx->SetOutputDim("Out", framework::make_ddim(out_shape));
    ctx->ShareLoD("X", "Out");
  }
};

class ExpandOpMaker : public framework::OpProtoAndCheckerMaker {
 public:
  ExpandOpMaker(framework::OpProto* proto, framework::OpAttrChecker* op_checker)
      : OpProtoAndCheckerMaker(proto, op_checker) {
    AddInput("X",
             "(Tensor, default Tensor<float>) A tensor with rank in [1, 6]."
             "X is the input tensor to be expanded.");
    AddOutput("Out",
              "(Tensor, default Tensor<float>) A tensor with rank in [1, 6]."
              "The rank of Output(Out) is same as Input(X) except that each "
              "dimension size of Output(Out) is equal to corresponding "
              "dimension size of Input(X) multiplying corresponding value of "
              "Attr(expandTimes).");
    AddAttr<std::vector<int>>("expandTimes",
                              "Expand times number for each dimension.");
    AddComment(R"DOC(
Expand operator tiles the input by given times number. You should set times
number for each dimension by providing attribute 'expandTimes'. The rank of X
should be in [1, 6]. Please notice that size of 'expandTimes' must be same with
X's rank.
)DOC");
  }
};

class ExpandGradOp : public framework::OperatorWithKernel {
 public:
  using framework::OperatorWithKernel::OperatorWithKernel;

 protected:
  void InferShape(framework::InferShapeContext* ctx) const override {
    PADDLE_ENFORCE(ctx->HasInput("X"), "Input(X) should not be null.");
    PADDLE_ENFORCE(ctx->HasInput(framework::GradVarName("Out")),
                   "Input(Out@GRAD) should not be null.");
    auto x_dims = ctx->GetInputDim("X");
    std::vector<int> expand_times =
        ctx->Attrs().Get<std::vector<int>>("expandTimes");
    auto out_dims = ctx->GetInputDim(framework::GradVarName("Out"));

    for (size_t i = 0; i < expand_times.size(); ++i) {
      PADDLE_ENFORCE_EQ(x_dims[i] * expand_times[i], out_dims[i],
                        "Each dimension size of Input(Out@GRAD) should be "
                        "equal to multiplication of crroresponding dimension "
                        "size of Input(X) and Attr(expandTimes) value.");
    }

    auto x_grad_name = framework::GradVarName("X");

    if (ctx->HasOutput(x_grad_name)) {
      ctx->SetOutputDim(x_grad_name, x_dims);
    }
  }
};

}  // namespace operators
}  // namespace paddle

namespace ops = paddle::operators;
REGISTER_OP(expand, ops::ExpandOp, ops::ExpandOpMaker, expand_grad,
            ops::ExpandGradOp);
REGISTER_OP_CPU_KERNEL(expand,
                       ops::ExpandKernel<paddle::platform::CPUPlace, float>);
REGISTER_OP_CPU_KERNEL(
    expand_grad, ops::ExpandGradKernel<paddle::platform::CPUPlace, float>);
