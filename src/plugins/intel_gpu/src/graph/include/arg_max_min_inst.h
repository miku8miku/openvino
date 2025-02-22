// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "intel_gpu/primitives/arg_max_min.hpp"
#include "primitive_inst.h"

#include <memory>
#include <string>

namespace cldnn {

template <>
struct typed_program_node<arg_max_min> : public typed_program_node_base<arg_max_min> {
    using parent = typed_program_node_base<arg_max_min>;

public:
    typed_program_node(std::shared_ptr<primitive> prim, program& prog) : parent(prim, prog) {}
    program_node& input() const { return get_dependency(0); }

    uint32_t get_output_nums() const {
        return (get_primitive()->input_size() == 3 ? 2 : get_primitive()->output_size());
    }
    bool has_second_output() const { return get_output_nums() == 2; }
    bool use_multiple_outputs() const { return get_primitive()->input_size() != 3; }
};

using arg_max_min_node = typed_program_node<arg_max_min>;

template <>
class typed_primitive_inst<arg_max_min> : public typed_primitive_inst_base<arg_max_min> {
    using parent = typed_primitive_inst_base<arg_max_min>;
    using parent::parent;

public:
    template<typename ShapeType>
    static std::vector<layout> calc_output_layouts(arg_max_min_node const& /*node*/, const kernel_impl_params& impl_param);
    static layout calc_output_layout(arg_max_min_node const& node, kernel_impl_params const& impl_param);
    static std::string to_string(arg_max_min_node const& node);

public:
    typed_primitive_inst(network& network, arg_max_min_node const& node);
};

using arg_max_min_inst = typed_primitive_inst<arg_max_min>;

}  // namespace cldnn
