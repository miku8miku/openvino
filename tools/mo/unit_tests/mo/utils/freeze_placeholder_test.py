# Copyright (C) 2018-2022 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

import argparse
import os
import unittest
from unittest.mock import patch, Mock

import numpy as np
import onnx
from generator import generator, generate
from onnx.helper import make_graph, make_model, make_tensor_value_info

from openvino.frontend import (
    FrontEndManager,
    FrontEnd,
)  # pylint: disable=no-name-in-module,import-error
from openvino.runtime import Core
from openvino.tools.mo.convert_impl import prepare_ir
from openvino.tools.mo.utils.error import Error


def base_args_config(use_legacy_fe: bool = None, use_new_fe: bool = None):
    args = argparse.Namespace()
    args.feManager = FrontEndManager()
    args.extensions = None
    args.use_legacy_frontend = use_legacy_fe
    args.use_new_frontend = use_new_fe
    args.framework = "onnx"
    args.model_name = None
    args.input_model = None
    args.silent = True
    args.transform = []
    args.scale = None
    args.output = None
    args.input = None
    args.input_shape = None
    args.batch = None
    args.mean_values = None
    args.scale_values = None
    args.output_dir = os.getcwd()
    args.freeze_placeholder_with_value = None
    args.transformations_config = None
    args.disable_fusing = None
    args.finegrain_fusing = None
    args.disable_resnet_optimization = None
    args.enable_concat_optimization = None
    args.static_shape = None
    args.disable_weights_compression = None
    args.reverse_input_channels = None
    args.data_type = None
    args.layout = None
    args.source_layout = None
    args.target_layout = None
    return args


try:
    import openvino_telemetry as tm
except ImportError:
    import openvino.tools.mo.utils.telemetry_stub as tm


def get_test_default_frontends():
    return {"onnx": "new", "tf": "legacy"}


@generator
class TestMoFreezePlaceholder(unittest.TestCase):
    def setUp(self):
        tm.Telemetry.__init__ = Mock(return_value=None)
        tm.Telemetry.send_event = Mock()
        FrontEnd.add_extension = Mock()

        self.models = {}
        add = onnx.helper.make_node("Add", inputs=["in1", "in2"], outputs=["add_out"])
        input_tensors = [
            make_tensor_value_info("in1", onnx.TensorProto.FLOAT, (2, 2)),
            make_tensor_value_info("in2", onnx.TensorProto.FLOAT, (2, 2)),
        ]
        output_tensors = [
            make_tensor_value_info("add_out", onnx.TensorProto.FLOAT, (2, 2)),
        ]
        graph = make_graph([add], "test_graph", input_tensors, output_tensors)
        model = make_model(
            graph,
            producer_name="MO tests",
            opset_imports=[onnx.helper.make_opsetid("", 13)],
        )
        self.models["test_model.onnx"] = model

        input_tensors_2 = [
            make_tensor_value_info("in1", onnx.TensorProto.FLOAT, (1, 1, 3)),
            make_tensor_value_info("in2", onnx.TensorProto.FLOAT, (1,)),
        ]
        output_tensors_2 = [
            make_tensor_value_info("mul_out", onnx.TensorProto.FLOAT, (1, 1, 3)),
        ]
        mul = onnx.helper.make_node("Mul", inputs=["in1", "in2"], outputs=["mul_out"])
        graph_2 = make_graph([mul], "test_graph_2", input_tensors_2, output_tensors_2)
        model_2 = make_model(
            graph_2,
            producer_name="MO tests",
            opset_imports=[onnx.helper.make_opsetid("", 13)],
        )
        self.models["test_model_2.onnx"] = model_2

        for name, model in self.models.items():
            onnx.save(model, name)

    def tearDown(self):
        for name in self.models.keys():
            os.remove(name)

    @generate(
        *[
            (
                    "in1[1 4]{f32}->[1.0 2.0 3.0 4.0],in2[1 4]{f32}->[1.0 2.0 3.0 4.0]",
                    True,
                    {},
                    np.array([2.0, 4.0, 6.0, 8.0]),
                    np.float32,
            ),
            (
                    "in2{f32}->[0.0 0.0 0.0 0.0]",
                    True,
                    {"in1": np.array([[1.0, 2.0], [3.0, 4.0]])},
                    np.array([[1.0, 2.0], [3.0, 4.0]]),
                    np.float32,
            ),
            (
                    "in2{f32}->[1.0 15.0 15.5 1.0]",
                    True,
                    {"in1": np.array([[2.0, 4.0], [12.0, 8.0]])},
                    np.array([[3.0, 19.0], [27.5, 9.0]]),
                    np.float32,
            ),
            (
                    "in1[1 4]{i32}->[1 2 3 4],in2[1 4]{i32}->[1 2 3 4]",
                    True,
                    {},
                    np.array([2.0, 4.0, 6.0, 8.0]),
                    np.int32,
            ),
        ],
    )
    def test_freeze_placeholder_with_value_onnx_fe(self, input_freezing_value, use_new_fe, inputs, expected,
                                                   dtype=None):
        with patch("openvino.tools.mo.convert_impl.get_default_frontends") as default_fe:
            default_fe.return_value = get_test_default_frontends()
            args = base_args_config(use_new_fe=use_new_fe)
            args.input_model = "test_model.onnx"
            args.input = input_freezing_value

            _, model = prepare_ir(args)

            ie = Core()
            exec_net = ie.compile_model(model, "CPU")
            req = exec_net.create_infer_request()
            results = req.infer(inputs)
            values = list(results.values())[0]
            if dtype is not None:
                assert values.dtype == dtype
            assert np.allclose(values, expected)

    @generate(
        *[
            (
                    "in1{f32}->[1.0 15.0 1.0]",
                    True,
                    {"in2": np.array([2])},
                    np.array([2.0, 30.0, 2.0]),
                    np.float32,
            ),
            (
                    "in1{f32}->[7.0 11.0 -1.0],in2{f32}->3.0",
                    True,
                    {},
                    np.array([21.0, 33.0, -3.0]),
                    np.float32,
            ),
            (
                    None,
                    True,
                    {
                        "in1": np.array([2.0, 2.0, 2.0]).reshape(1, 1, 3),
                        "in2": np.array([-1.0]),
                    },
                    np.array([-2.0, -2.0, -2.0]),
                    np.float32,
            ),
            (
                    "in1[3 1]{f32}->[7.0 11.0 -1.0],in2{f32}->3.0",
                    True,
                    {},
                    np.array([21.0, 33.0, -3.0]).reshape(3, 1),
                    np.float32,
            ),
            (
                    "in1[3 1]{f16}->[7.0 11.0 -1.0],in2{f16}->3.0",
                    True,
                    {},
                    np.array([21.0, 33.0, -3.0]).reshape(3, 1),
                    np.float16,
            ),
            (
                    "in1[3 1]{i32}->[7 11 -1],in2{i32}->3.0",
                    True,
                    {},
                    np.array([21, 33, -3]).reshape(3, 1),
                    np.int32,
            ),
        ],
    )
    def test_freeze_placeholder_with_value_mul(self, input_freezing_value, use_new_fe, inputs, expected, dtype=None):
        with patch("openvino.tools.mo.convert_impl.get_default_frontends") as default_fe:
            default_fe.return_value = get_test_default_frontends()
            args = base_args_config(use_new_fe=use_new_fe)
            args.input_model = "test_model_2.onnx"
            args.input = input_freezing_value

            _, model = prepare_ir(args)

            ie = Core()
            exec_net = ie.compile_model(model, "CPU")
            req = exec_net.create_infer_request()
            results = req.infer(inputs)
            values = list(results.values())[0]
            if dtype is not None:
                assert values.dtype == dtype
            assert np.allclose(values, expected)

    @generate(
        *[
            (
                    "in1->[1.0 15.0 1.0]",
                    True,
                    {"in2": np.array([2])},
                    np.array([2.0, 30.0, 2.0]),
                    np.float32,
            ),
        ],
    )
    def test_value_without_type(self, input_freezing_value, use_new_fe, inputs, expected,
                                dtype=None):
        with patch("openvino.tools.mo.convert_impl.get_default_frontends") as default_fe:
            default_fe.return_value = get_test_default_frontends()
            args = base_args_config(use_new_fe=use_new_fe)
            args.input_model = "test_model_2.onnx"
            args.input = input_freezing_value
            self.assertRaisesRegex(Error, "Please specify type for value freezing in1 node explicitly "
                                          "because the frontend does not support automatic type detection.",
                                   prepare_ir, args)
