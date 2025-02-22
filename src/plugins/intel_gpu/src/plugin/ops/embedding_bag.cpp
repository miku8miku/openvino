// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "intel_gpu/plugin/program.hpp"
#include "intel_gpu/plugin/common_utils.hpp"

#include "ngraph/op/embedding_segments_sum.hpp"
#include "ngraph/op/embeddingbag_offsets_sum.hpp"
#include "ngraph/op/embeddingbag_packedsum.hpp"

#include "intel_gpu/primitives/embedding_bag.hpp"
#include "intel_gpu/primitives/reorder.hpp"

#include "transformations/utils/utils.hpp"

namespace ov {
namespace intel_gpu {

static void CreateEmbeddingBagOffsetsSumOp(Program& p, const std::shared_ptr<ngraph::op::v3::EmbeddingBagOffsetsSum>& op) {
    validate_inputs_count(op, {3, 4, 5});
    auto inputs = p.GetInputInfo(op);
    std::string layerName = layer_type_name_ID(op);

    int32_t defaultIndex = -1;
    if (inputs.size() > 3) {
        auto index_node = std::dynamic_pointer_cast<ngraph::op::v0::Constant>(op->get_input_node_shared_ptr(3));
        if (!index_node) {
            IE_THROW() << "Unsupported parameter nodes type in " << op->get_friendly_name() << " (" << op->get_type_name() << ")";
        }

        float val;
        if (ngraph::shape_size(index_node->get_output_shape(0)) != 1 || !ngraph::op::util::get_single_value(index_node, val))
             IE_THROW() << "Unsupported parameter size in " << op->get_friendly_name() << " (" << op->get_type_name() << ")";

        defaultIndex = static_cast<int32_t>(val);
        inputs.erase(inputs.begin() + 3); // Remove "default_index"
    }

    std::vector<cldnn::input_info> reordered_inputs;
    reordered_inputs.resize(inputs.size());

    for (size_t portIndex = 0; portIndex < inputs.size(); portIndex++) {
        auto inputDataType = cldnn::element_type_to_data_type(op->get_input_element_type(portIndex));
        if (((portIndex == 1) || (portIndex == 2)) && (inputDataType == cldnn::data_types::i64)) {
            // GPU primitive supports only i32 data type for indices inputs,
            // so we need additional reorders if they are provided as i64
            auto reorderPrimName = inputs[portIndex].pid + "_" + op->get_friendly_name() + Program::m_preProcessTag;
            auto targetFormat = cldnn::format::get_default_format(op->get_input_shape(portIndex).size());
            auto preprocessPrim = cldnn::reorder(reorderPrimName,
                                                 inputs[portIndex],
                                                 targetFormat,
                                                 cldnn::data_types::i32,
                                                 std::vector<float>(),
                                                 cldnn::reorder_mean_mode::subtract);
            p.add_primitive(*op, preprocessPrim);
            reordered_inputs[portIndex] = cldnn::input_info(reorderPrimName);
        } else {
            reordered_inputs[portIndex] = inputs[portIndex];
        }
    }

    auto embeddingBagPrim = cldnn::embedding_bag(layerName,
                                                 reordered_inputs,
                                                 cldnn::embedding_bag::offsets_sum,
                                                 tensor_from_dims(op->get_output_shape(0)),
                                                 defaultIndex);

    p.add_primitive(*op, embeddingBagPrim);
}

static void CreateEmbeddingBagPackedSumOp(Program& p, const std::shared_ptr<ngraph::op::v3::EmbeddingBagPackedSum>& op) {
    validate_inputs_count(op, {2, 3});
    auto inputs = p.GetInputInfo(op);
    std::string layerName = layer_type_name_ID(op);

    std::vector<cldnn::input_info> reordered_inputs;
    reordered_inputs.resize(inputs.size());

    for (size_t portIndex = 0; portIndex < inputs.size(); portIndex++) {
        auto inputDataType = cldnn::element_type_to_data_type(op->get_input_element_type(portIndex));
        if ((portIndex == 1) && (inputDataType == cldnn::data_types::i64)) {
            // GPU primitive supports only i32 data type for indices input,
            // so we need additional reorder if it's provided as i64
            auto reorderPrimName = inputs[portIndex].pid + "_" + op->get_friendly_name() + Program::m_preProcessTag;
            auto targetFormat = cldnn::format::get_default_format(op->get_input_shape(portIndex).size());
            auto preprocessPrim = cldnn::reorder(reorderPrimName,
                                                 inputs[portIndex],
                                                 targetFormat,
                                                 cldnn::data_types::i32,
                                                 std::vector<float>(),
                                                 cldnn::reorder_mean_mode::subtract);
            p.add_primitive(*op, preprocessPrim);
            reordered_inputs[portIndex] = cldnn::input_info(reorderPrimName);
        } else {
            reordered_inputs[portIndex] = inputs[portIndex];
        }
    }

    auto embeddingBagPrim = cldnn::embedding_bag(layerName,
                                                 reordered_inputs,
                                                 cldnn::embedding_bag::packed_sum,
                                                 tensor_from_dims(op->get_output_shape(0)),
                                                 -1);

    p.add_primitive(*op, embeddingBagPrim);
}

static void CreateEmbeddingSegmentsSumOp(Program& p, const std::shared_ptr<ngraph::op::v3::EmbeddingSegmentsSum>& op) {
    validate_inputs_count(op, {4, 5, 6});
    auto inputs = p.GetInputInfo(op);
    std::string layerName = layer_type_name_ID(op);

    inputs.erase(inputs.begin() + 3); // Remove "num_segments"

    int32_t defaultIndex = -1;
    // port of default_index is 4 by default, but we removed "num_segments" above, so now it's equal to 3
    if (inputs.size() > 3) {
        auto index_node = std::dynamic_pointer_cast<ngraph::op::v0::Constant>(op->get_input_node_shared_ptr(4));
        if (!index_node) {
            IE_THROW() << "Unsupported parameter nodes type in " << op->get_friendly_name() << " (" << op->get_type_name() << ")";
        }

        float val;
        if (ngraph::shape_size(index_node->get_output_shape(0)) != 1 || !ngraph::op::util::get_single_value(index_node, val))
             IE_THROW() << "Unsupported parameter size in " << op->get_friendly_name() << " (" << op->get_type_name() << ")";

        defaultIndex = static_cast<int32_t>(val);
        inputs.erase(inputs.begin() + 3); // Remove "default_index"
    }

    std::vector<cldnn::input_info> reordered_inputs;
    reordered_inputs.resize(inputs.size());

    for (size_t portIndex = 0; portIndex < inputs.size(); portIndex++) {
        auto inputDataType = cldnn::element_type_to_data_type(op->get_input_element_type(portIndex));
        if (((portIndex == 1) || (portIndex == 2)) && (inputDataType == cldnn::data_types::i64)) {
            // GPU primitive supports only i32 data type for indices inputs,
            // so we need additional reorders if they are provided as i64
            auto reorderPrimName = inputs[portIndex].pid + "_" + op->get_friendly_name() + Program::m_preProcessTag;
            auto targetFormat = cldnn::format::get_default_format(op->get_input_shape(portIndex).size());
            auto preprocessPrim = cldnn::reorder(reorderPrimName,
                                                 inputs[portIndex],
                                                 targetFormat,
                                                 cldnn::data_types::i32,
                                                 std::vector<float>(),
                                                 cldnn::reorder_mean_mode::subtract);
            p.add_primitive(*op, preprocessPrim);
            reordered_inputs[portIndex] = cldnn::input_info(reorderPrimName);
        } else {
            reordered_inputs[portIndex] = inputs[portIndex];
        }
    }

    auto embeddingBagPrim = cldnn::embedding_bag(layerName,
                                                 reordered_inputs,
                                                 cldnn::embedding_bag::segments_sum,
                                                 tensor_from_dims(op->get_output_shape(0)),
                                                 defaultIndex);

    p.add_primitive(*op, embeddingBagPrim);
}

REGISTER_FACTORY_IMPL(v3, EmbeddingBagOffsetsSum);
REGISTER_FACTORY_IMPL(v3, EmbeddingBagPackedSum);
REGISTER_FACTORY_IMPL(v3, EmbeddingSegmentsSum);

}  // namespace intel_gpu
}  // namespace ov
