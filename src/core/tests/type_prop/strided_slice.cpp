// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <dimension_tracker.hpp>
#include <memory>
#include <strided_slice_shape_inference.hpp>

#include "gtest/gtest.h"
#include "ngraph/ngraph.hpp"
#include "openvino/opsets/opset9.hpp"
#include "util/type_prop.hpp"

using namespace std;
using namespace ngraph;

TEST(type_prop, strided_slice_begin_incorrect_type) {
    auto data = make_shared<op::Parameter>(element::f32, Shape{2, 4, 6, 8});
    auto begin = make_shared<op::Parameter>(element::f16, Shape{4});
    auto end = make_shared<op::Parameter>(element::i64, Shape{4});
    try {
        auto strided_slice = make_shared<op::v1::StridedSlice>(data,
                                                               begin,
                                                               end,
                                                               vector<int64_t>{1, 0, 1, 0},
                                                               vector<int64_t>{1, 0, 1, 0});
        // Should have thrown, so fail if it didn't
        FAIL() << "Incorrect begin type exception not thrown.";
    } catch (const NodeValidationFailure& error) {
        EXPECT_HAS_SUBSTRING(error.what(), std::string("Begin mask must be an integral number"));
    } catch (...) {
        FAIL() << "Deduced type check failed for unexpected reason";
    }
}

TEST(type_prop, strided_slice_end_incorrect_type) {
    auto data = make_shared<op::Parameter>(element::f32, Shape{2, 4, 6, 8});
    auto begin = make_shared<op::Parameter>(element::i64, Shape{4});
    auto end = make_shared<op::Parameter>(element::boolean, Shape{4});
    try {
        auto strided_slice = make_shared<op::v1::StridedSlice>(data,
                                                               begin,
                                                               end,
                                                               vector<int64_t>{1, 0, 1, 0},
                                                               vector<int64_t>{1, 0, 1, 0});
        // Should have thrown, so fail if it didn't
        FAIL() << "Incorrect end type exception not thrown.";
    } catch (const NodeValidationFailure& error) {
        EXPECT_HAS_SUBSTRING(error.what(), std::string("End mask must be an integral number"));
    } catch (...) {
        FAIL() << "Deduced type check failed for unexpected reason";
    }
}

TEST(type_prop, strided_slice_incompatible_size_of_masks_attr) {
    auto data = make_shared<op::Parameter>(element::f32, Shape{2, 4, 6, 8});
    auto begin = make_shared<op::Parameter>(element::i64, Shape{4});
    auto end = make_shared<op::Parameter>(element::i64, Shape{4});
    try {
        auto strided_slice = make_shared<op::v1::StridedSlice>(data,
                                                               begin,
                                                               end,
                                                               vector<int64_t>{1, 0, 1, 0},
                                                               vector<int64_t>{1, 0, 1, 0},
                                                               vector<int64_t>{1, 0, 1, 0, 1});
        // Should have thrown, so fail if it didn't
        FAIL() << "Incompatible size od masks exception not thrown.";
    } catch (const NodeValidationFailure& error) {
        EXPECT_HAS_SUBSTRING(error.what(), std::string("All masks of StridedSlice must have the same size"));
    } catch (...) {
        FAIL() << "Deduced type check failed for unexpected reason";
    }
}

TEST(type_prop, strided_slice_mask_incorrect_value) {
    auto data = make_shared<op::Parameter>(element::f32, Shape{2, 4, 6, 8});
    auto begin = make_shared<op::Parameter>(element::i64, Shape{4, 5});
    auto end = make_shared<op::Parameter>(element::i64, Shape{4});
    try {
        auto strided_slice = make_shared<op::v1::StridedSlice>(data,
                                                               begin,
                                                               end,
                                                               vector<int64_t>{1, 0, 1, 0},
                                                               vector<int64_t>{1, 0, 1, 2});
        // Should have thrown, so fail if it didn't
        FAIL() << "Incorrect values of StridedSlice mask exception not thrown.";
    } catch (const NodeValidationFailure& error) {
        EXPECT_HAS_SUBSTRING(error.what(), std::string("All masks of StridedSlice must have be 0 or 1"));
    } catch (...) {
        FAIL() << "Deduced type check failed for unexpected reason";
    }
}

TEST(type_prop, strided_slice_begin_incorrect_shape) {
    auto data = make_shared<op::Parameter>(element::f32, Shape{2, 4, 6, 8});
    auto begin = make_shared<op::Parameter>(element::i64, Shape{4, 5});
    auto end = make_shared<op::Parameter>(element::i64, Shape{4});
    try {
        auto strided_slice = make_shared<op::v1::StridedSlice>(data,
                                                               begin,
                                                               end,
                                                               vector<int64_t>{1, 0, 1, 0},
                                                               vector<int64_t>{1, 0, 1, 0});
        // Should have thrown, so fail if it didn't
        FAIL() << "Incorrect shape of begin exception not thrown.";
    } catch (const NodeValidationFailure& error) {
        EXPECT_HAS_SUBSTRING(error.what(), std::string("Begin input must be 1D (begin rank:"));
    } catch (...) {
        FAIL() << "Deduced type check failed for unexpected reason";
    }
}

TEST(type_prop, strided_slice_end_incorrect_shape) {
    auto data = make_shared<op::Parameter>(element::f32, Shape{2, 4, 6, 8});
    auto begin = make_shared<op::Parameter>(element::i64, Shape{4});
    auto end = make_shared<op::Parameter>(element::i64, Shape{4, 5});
    try {
        auto strided_slice = make_shared<op::v1::StridedSlice>(data,
                                                               begin,
                                                               end,
                                                               vector<int64_t>{1, 0, 1, 0},
                                                               vector<int64_t>{1, 0, 1, 0});
        // Should have thrown, so fail if it didn't
        FAIL() << "Incorrect shape of end exception not thrown.";
    } catch (const NodeValidationFailure& error) {
        EXPECT_HAS_SUBSTRING(error.what(), std::string("End input must be 1D (end rank:"));
    } catch (...) {
        FAIL() << "Deduced type check failed for unexpected reason";
    }
}

TEST(type_prop, strided_slice_default_stride_dynamic_shape_input) {
    auto data = make_shared<op::Parameter>(element::f32, Shape{2, 4, 6, 8});
    auto begin = make_shared<op::Parameter>(element::i64, PartialShape::dynamic());
    auto end = make_shared<op::Parameter>(element::i64, Shape{2});
    auto strided_slice =
        make_shared<op::v1::StridedSlice>(data, begin, end, vector<int64_t>{0, 0}, vector<int64_t>{0, 0});

    ASSERT_TRUE(strided_slice->input_value(3).get_partial_shape().compatible(PartialShape{2}));

    try {
        end = make_shared<op::Parameter>(element::i64, PartialShape::dynamic());
        strided_slice =
            make_shared<op::v1::StridedSlice>(data, begin, end, vector<int64_t>{0, 0}, vector<int64_t>{0, 0});
        // Should have thrown, so fail if it didn't
        FAIL() << "Unknown data to calculate default strides exception not thrown.";
    } catch (const CheckFailure& error) {
        EXPECT_HAS_SUBSTRING(error.what(), std::string("Begin input must be 1D"));
    } catch (...) {
        FAIL() << "Deduced type check failed for unexpected reason";
    }
}

TEST(type_prop, strided_slice_reverse_out_of_bounds) {
    auto data = std::make_shared<op::Parameter>(ngraph::element::f32, ngraph::Shape{3, 4, 5});
    auto begin = op::Constant::create(ngraph::element::i64, ngraph::Shape{3}, {100});
    auto end = op::Constant::create(ngraph::element::i64, ngraph::Shape{3}, {-100});
    auto stride = op::Constant::create(ngraph::element::i64, ngraph::Shape{3}, {-1});

    std::vector<int64_t> begin_mask = {0, 0, 0, 0};
    std::vector<int64_t> end_mask = {0, 0, 0, 0};

    auto ss = std::make_shared<op::v1::StridedSlice>(data, begin, end, stride, begin_mask, end_mask);

    Shape expected{3, 4, 5};
    EXPECT_EQ(ss->get_output_shape(0), expected);
}

TEST(type_prop, strided_slice_dynamic_value_and_label_propagation) {
    Dimension marked_0 = Dimension(3);
    ov::DimensionTracker::set_label(marked_0, 10);
    PartialShape target_0 = PartialShape{marked_0, 4};

    auto param = std::make_shared<op::Parameter>(element::f32, Shape{1});
    auto param_0 = std::make_shared<op::Parameter>(element::f32, target_0);
    auto shape_0 = std::make_shared<op::ShapeOf>(param_0);

    const auto& et = element::i64;
    std::vector<int64_t> start_val{0}, stop_val{1}, step_val{1};
    const auto start = std::make_shared<op::v0::Constant>(et, Shape{start_val.size()}, start_val);
    const auto stop = std::make_shared<op::v0::Constant>(et, Shape{stop_val.size()}, stop_val);
    const auto step = std::make_shared<op::v0::Constant>(et, Shape{step_val.size()}, step_val);
    const auto slice = std::make_shared<op::v1::StridedSlice>(shape_0,
                                                              start,
                                                              stop,
                                                              step,
                                                              std::vector<int64_t>{0},
                                                              std::vector<int64_t>{0});

    auto bc = std::make_shared<op::v1::Broadcast>(param, slice);
    ASSERT_EQ(bc->get_shape(), (Shape{3}));

    const auto& output_shape = bc->get_output_partial_shape(0);
    ASSERT_EQ(ov::DimensionTracker::get_label(output_shape[0]), 10);
}

TEST(type_prop, strided_slice_default_shape_inference) {
    auto slice = new op::v1::StridedSlice;
    slice->set_begin_mask({0, 0, 0});
    slice->set_end_mask({0, 0, 0});
    slice->set_new_axis_mask({1, 0, 0});
    slice->set_shrink_axis_mask({0, 0, 0, 1});
    slice->set_ellipsis_mask_mask({0, 0, 0});
    std::vector<ov::PartialShape> in = {{10, 11, 12}, {4}, {4}, {4}}, out = {PartialShape()};
    int64_t begin_data[] = {0, 0, 0, 0}, end_data[] = {1, 1, 5, 1}, stride_data[] = {1, 1, 1, 1};
    const std::map<size_t, std::shared_ptr<ngraph::runtime::HostTensor>> const_data = {
        {1, std::make_shared<ov::HostTensor>(element::i64, Shape{4}, begin_data)},
        {2, std::make_shared<ov::HostTensor>(element::i64, Shape{4}, end_data)},
        {3, std::make_shared<ov::HostTensor>(element::i64, Shape{4}, stride_data)}};
    ov::op::v1::shape_infer(slice, in, out, const_data);
    ASSERT_EQ(out[0], PartialShape({1, 1, 5}));
}

struct StridedSliceTestParams {
    PartialShape input_shape;
    PartialShape begin_shape;
    PartialShape end_shape;
    PartialShape strides_shape;
    std::vector<int64_t> begin_mask;
    std::vector<int64_t> end_mask;
    std::vector<int64_t> new_axis_mask;
    std::vector<int64_t> shrink_axis_mask;
    std::vector<int64_t> ellipsis_mask;

    PartialShape ref_shape;
    ov::element::Type ref_type;
};

struct StridedSliceShapeInferTest : ::testing::TestWithParam<StridedSliceTestParams> {};

TEST_P(StridedSliceShapeInferTest, non_const_begin) {
    auto params = GetParam();

    auto input_data = std::make_shared<ov::opset9::Parameter>(ov::element::f32, params.input_shape);
    auto begin = std::make_shared<ov::opset9::Parameter>(ov::element::i32, params.begin_shape);
    auto end = std::make_shared<ov::opset9::Parameter>(ov::element::i32, params.end_shape);
    auto strides = std::make_shared<ov::opset9::Parameter>(ov::element::i32, params.strides_shape);
    std::vector<int64_t> begin_mask = params.begin_mask;
    std::vector<int64_t> end_mask = params.end_mask;
    std::vector<int64_t> new_axis_mask = params.new_axis_mask;
    std::vector<int64_t> shrink_axis_mask = params.shrink_axis_mask;
    std::vector<int64_t> ellipsis_mask = params.ellipsis_mask;

    auto strided_slice = std::make_shared<ov::opset9::StridedSlice>(input_data,
                                                                    begin,
                                                                    end,
                                                                    strides,
                                                                    begin_mask,
                                                                    end_mask,
                                                                    new_axis_mask,
                                                                    shrink_axis_mask,
                                                                    ellipsis_mask);

    EXPECT_EQ(strided_slice->get_element_type(), params.ref_type);
    auto res_shape = strided_slice->get_output_partial_shape(0);
    EXPECT_EQ(res_shape, params.ref_shape);
}

INSTANTIATE_TEST_SUITE_P(type_prop,
                         StridedSliceShapeInferTest,
                         ::testing::Values(StridedSliceTestParams{{1, 200, 300, 3},  // input_shape
                                                                  {4},               // begin shape
                                                                  {4},               // end shape
                                                                  {4},               // strides shape
                                                                  {0, 0, 0, 0},      // begin mask
                                                                  {0, 0, 0, 0},      // end mask
                                                                  {0, 0, 0, 0},      // new axis mask
                                                                  {0, 1, 0, 0},      // shrink axis mask
                                                                  {0, 0, 0, 0},      // ellipsis mask
                                                                  PartialShape{
                                                                      Dimension(0, 1),
                                                                      Dimension(0, 300),
                                                                      Dimension(0, 3),
                                                                  },                 // reference shape
                                                                  element::f32},     // reference type
                                           StridedSliceTestParams{{1, 200, 300, 3},  // input_shape
                                                                  {4},               // begin shape
                                                                  {4},               // end shape
                                                                  {4},               // strides shape
                                                                  {1, 0, 0, 0},      // begin mask
                                                                  {1, 0, 0, 0},      // end mask
                                                                  {0, 0, 0, 0},      // new axis mask
                                                                  {0, 1, 0, 1},      // shrink axis mask
                                                                  {0, 0, 0, 0},      // ellipsis mask
                                                                  PartialShape{
                                                                      Dimension(0, 1),
                                                                      Dimension(0, 300),
                                                                  },                 // reference shape
                                                                  element::f32},     // reference type
                                           StridedSliceTestParams{{1, 200, 200, 3},  // input_shape
                                                                  {4},               // begin shape
                                                                  {4},               // end shape
                                                                  {4},               // strides shape
                                                                  {1, 0, 0, 0},      // begin mask
                                                                  {1, 0, 0, 0},      // end mask
                                                                  {0, 0, 1, 0},      // new axis mask
                                                                  {0, 0, 0, 0},      // shrink axis mask
                                                                  {0, 0, 0, 0},      // ellipsis mask
                                                                  PartialShape{
                                                                      Dimension(0, 1),
                                                                      Dimension(0, 200),
                                                                      1,
                                                                      Dimension(0, 200),
                                                                      3,
                                                                  },              // reference shape
                                                                  element::f32},  // reference type
                                           // case when input has dynamic dimensions
                                           StridedSliceTestParams{
                                               PartialShape{Dimension(8, -1), 200, 200, 3},  // input_shape
                                               {4},                                          // begin shape
                                               {4},                                          // end shape
                                               {4},                                          // strides shape
                                               {0, 0, 0, 0},                                 // begin mask
                                               {0, 0, 0, 0},                                 // end mask
                                               {0, 0, 0, 0},                                 // new axis mask
                                               {0, 0, 0, 0},                                 // shrink axis mask
                                               {0, 0, 0, 0},                                 // ellipsis mask
                                               PartialShape{
                                                   Dimension::dynamic(),
                                                   Dimension(0, 200),
                                                   Dimension(0, 200),
                                                   Dimension(0, 3),
                                               },             // reference shape
                                               element::f32}  // reference type
                                           ),
                         PrintToDummyParamName());
