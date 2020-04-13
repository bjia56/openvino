// Copyright (C) 2018-2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "transformations/convert_opset1_to_legacy/reshape_fully_connected.hpp"

#include <memory>
#include <vector>

#include <ngraph/opsets/opset1.hpp>

#include "ngraph_ops/fully_connected.hpp"
#include "transformations/utils/utils.hpp"

void ngraph::pass::ReshapeFullyConnected::reshape_fully_connected() {
    auto input0 = std::make_shared<pattern::op::Label>(element::i64, Shape{1, 1});
    auto input1 = std::make_shared<pattern::op::Label>(element::i64, Shape{1, 1});
    auto input2 = std::make_shared<pattern::op::Label>(element::i64, Shape{1});
    auto fc = std::make_shared<ngraph::op::FullyConnected>(input0, input1, input2, Shape{1, 1});

    ngraph::graph_rewrite_callback callback = [this](pattern::Matcher& m) {
        auto fc = std::dynamic_pointer_cast<ngraph::op::FullyConnected> (m.get_match_root());
        if (!fc || transformation_callback(fc)) {
            return false;
        }

        auto input_shape = fc->input_value(0).get_shape();
        auto output_shape = fc->get_shape();

        if (input_shape.size() == 2) {
            return false;
        }

        std::vector<int64_t> reshape_shape{-1, static_cast<int64_t>(input_shape.back())};
        auto reshape = std::make_shared<opset1::Reshape>(fc->input_value(0),
                                                         opset1::Constant::create(element::i64, Shape{2}, reshape_shape), true);

        reshape->set_friendly_name(fc->get_friendly_name() + "/Reshape");

        // Calculate output shape for new FullyConnected layer
        // [I, K] * [O, K] = [I, O]
        auto I = reshape->get_shape()[0];
        auto O = fc->input_value(1).get_shape()[0];
        Shape output_shape_new{I, O};

        auto fc_new = std::make_shared<op::FullyConnected>(reshape,
                                                           fc->input_value(1),
                                                           fc->input_value(2),
                                                           output_shape_new);

        if (output_shape != output_shape_new) {
            auto reshape_output = op::util::reshapeTo(fc_new, output_shape);
            reshape_output->set_friendly_name(fc->get_friendly_name());
            fc->set_friendly_name(fc->get_friendly_name() + "/FC");
            ngraph::replace_node(fc, reshape_output);
        } else {
            fc_new->set_friendly_name(fc->get_friendly_name());
            ngraph::replace_node(fc, fc_new);
        }

        return true;
    };

    auto m = std::make_shared<ngraph::pattern::Matcher>(fc, "ReshapeFullyConnected");
    this->add_matcher(m, callback, PassProperty::CHANGE_DYNAMIC_STATE);
}