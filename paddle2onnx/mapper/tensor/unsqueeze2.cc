// Copyright (c) 2022 PaddlePaddle Authors. All Rights Reserved.
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

#include "paddle2onnx/mapper/tensor/unsqueeze2.h"
#include <string>
#include <vector>

namespace paddle2onnx {
REGISTER_MAPPER(unsqueeze2, UnSqueeze2Mapper)

int32_t UnSqueeze2Mapper::GetMinOpset(bool verbose) {
  auto op = parser_->GetOpDesc(block_idx_, op_idx_);
  bool has_attr = parser_->OpHasAttr(op, "axes");
  if (has_attr) {
    return 7;
  }
  bool has_axis_tensor_info =
      parser_->OpHasInput(block_idx_, op_idx_, "AxesTensor");
  if (!has_axis_tensor_info) {
    if (verbose) {
      std::cerr << " Can not find Axes as input or attribute in op "
                << op.type() << "." << std::endl;
    }
    return -1;
  }
  std::vector<TensorInfo> axes_info =
      parser_->GetOpInput(block_idx_, op_idx_, "AxesTensor");
  std::vector<int64_t> index = parser_->GetBlockOpIdx(axes_info[0].name);
  bool found_value = parser_->GetValueFromTensor(index[0], index[1]);
  if (!found_value) {
    return 13;
  } else {
    std::vector<int64_t> axes = ComputeAxes();
    bool sorted = std::is_sorted(axes.begin(), axes.end());
    if (!sorted) {
      if (verbose) {
        std::cerr << " axes must be arranged in the following order in op "
                  << op.type() << "." << std::endl;
      }
      return -1;
    }
  }
  return 7;
}

std::vector<int64_t> UnSqueeze2Mapper::ComputeAxes() {
  auto op = parser_->GetOpDesc(block_idx_, op_idx_);

  bool has_attr = parser_->OpHasAttr(op, "axes");
  std::vector<int64_t> axes;
  if (has_attr) {
    parser_->GetOpAttr(op, "axes", &axes);
  } else {
    std::vector<TensorInfo> axes_info =
        parser_->GetOpInput(block_idx_, op_idx_, "AxesTensor");
    std::vector<int64_t> index = parser_->GetBlockOpIdx(axes_info[0].name);
    Weight value;
    bool get_value = parser_->GetValueFromTensor(index[0], index[1], &value);
    if (get_value) {
      value.get(&axes);
    }
  }
  std::vector<TensorInfo> input_info =
      parser_->GetOpInput(block_idx_, op_idx_, "X");
  for (auto i = 0; i < axes.size(); ++i) {
    if (axes[i] < 0) {
      axes[i] = axes[i] + input_info[0].Rank() + i + 1;
    }
  }
  return axes;
}

void UnSqueeze2Mapper::Opset7(OnnxHelper* helper) {
  auto op = parser_->GetOpDesc(block_idx_, op_idx_);
  std::vector<TensorInfo> input_info =
      parser_->GetOpInput(block_idx_, op_idx_, "X");
  std::vector<TensorInfo> output_info =
      parser_->GetOpOutput(block_idx_, op_idx_, "Out");

  auto axes = ComputeAxes();
  if (axes.empty()) {
    std::vector<TensorInfo> axes_tensor_info =
        parser_->GetOpInput(block_idx_, op_idx_, "AxesTensor");
    helper->MakeNode("Unsqueeze",
                     {input_info[0].name, axes_tensor_info[0].name},
                     {output_info[0].name});
  } else {
    helper->Unsqueeze(input_info[0].name, output_info[0].name, axes);
  }
}

}  // namespace paddle2onnx