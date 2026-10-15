"""Cross-language Layer3 contracts through the real native implementation."""

import asyncio
from contextlib import ExitStack
import importlib.util
import json
import math
import os
import struct
import subprocess
import unittest
from unittest.mock import patch

import gr_os as gr
from gr_os import data


class DataTests(unittest.TestCase):
    def setUp(self):
        self.stack = ExitStack()
        self.addCleanup(self.stack.close)
        self.native = gr.Native()
        self.context = self.own(
            data.Context(
                native=self.native,
                modules=(
                    "gr.data.standard@1",
                    "gr.data.sparse@1",
                    "gr.data.relational@1",
                    "gr.data.numeric@1",
                ),
            )
        )
        self.runtime = self.own(gr.Runtime(native=self.native, workers=2))

    def own(self, value):
        return self.stack.enter_context(value)

    def literal(self, value):
        self.own(value)
        return self.own(self.context.literal(value))

    def apply(self, name, inputs, attributes=None, **options):
        return self.own(
            self.context.apply(name, inputs, attributes=attributes, **options)
        )

    def run_outputs(self, outputs, **options):
        program = self.own(data.compile(outputs))
        return self.own(program.run(self.runtime, **options)).raise_for_status()

    def test_function_binding_composition_and_lifetime(self):
        value = self.own(self.context.column([2, 3, 4]))
        symbolic = self.own(self.context.input("chunk", value.spec))
        twice = self.apply(
            "gr.data.affine@1",
            [
                symbolic,
                self.literal(self.context.scalar(2)),
                self.literal(self.context.scalar(0)),
            ],
        )
        function = self.own(data.Function([twice, symbolic]))
        self.assertEqual(set(function.inputs), {"chunk"})
        literal = self.literal(value)
        instantiated = function.instantiate({"chunk": literal})
        for expression in instantiated:
            self.own(expression)
        total = self.apply(
            "gr.data.sum@1", [instantiated[0]], {"nulls": "skip", "min_count": 1}
        )
        self.assertEqual(self.run_outputs([total]).outputs[0].tolist(), 18)
        program = self.own(function.bind({"chunk": value}))
        function.close()
        symbolic.close()
        twice.close()
        self.context.close()
        self.assertEqual(
            [
                output.tolist()
                for output in self.own(program.run(self.runtime))
                .raise_for_status()
                .outputs
            ],
            [[4, 6, 8], [2, 3, 4]],
        )

    def test_function_partial_wrap_rolls_back_all_exports(self):
        expression = self.literal(self.context.column([1]))
        function = self.own(data.Function([expression, expression, expression]))
        exports = []
        real_call = function._call
        real_expr = data.Expr

        def tracked(*args, **kwargs):
            result = real_call(*args, **kwargs)
            exports.extend(value["handle"] for value in result["outputs"])
            return result

        count = 0

        def wrapping(*args, **kwargs):
            nonlocal count
            count += 1
            if count == 2:
                raise RuntimeError("injected wrapping interruption")
            return real_expr(*args, **kwargs)

        with (
            patch.object(function, "_call", side_effect=tracked),
            patch.object(data, "Expr", side_effect=wrapping),
        ):
            with self.assertRaisesRegex(RuntimeError, "injected"):
                function.instantiate({})
        self.assertEqual(len(exports), 3)
        for handle in exports:
            with self.assertRaises(gr.NativeError):
                self.native.call("data.expr.describe", handle=handle)

    def test_validation_cache_scope_and_numeric_dependency(self):
        value = self.own(self.context.column([1, 2]))
        self.literal(value.retain())
        before = self.context.validation_statistics()
        self.literal(value.retain())
        after = self.context.validation_statistics()
        self.assertGreater(after["hits"], before["hits"])
        self.context.clear_validation_cache()
        self.assertEqual(self.context.validation_statistics()["entries"], 0)
        disabled = self.own(
            data.Context(native=self.native, validation_cache={"max_entries": 0})
        )
        for _ in range(2):
            self.own(disabled.literal(value))
        self.assertEqual(disabled.validation_statistics()["hits"], 0)
        with self.assertRaises(gr.NativeError):
            data.Context(native=self.native, modules=("gr.data.numeric@1",))

    def test_numeric_full_width_nullable_and_providers(self):
        self.assertEqual(
            self.own(self.context.scalar(2**64 - 1, dtype="uint64")).tolist(), 2**64 - 1
        )
        self.assertEqual(
            self.own(self.context.scalar(1.5 - 2j, dtype="complex128")).tolist(),
            1.5 - 2j,
        )
        left = self.literal(self.context.column([2**63, None, 7], dtype="uint64"))
        right = self.literal(
            self.context.column([1, 2, 3], dtype="uint64", nullable=True)
        )
        results = []
        for suffix in ("batch", "scalar"):
            add = self.apply(
                "gr.numeric.add@1",
                [left, right],
                selection={"implementation": f"gr.numeric.cpu.add-{suffix}@1"},
            )
            results.append(self.run_outputs([add]).outputs[0].tolist())
        self.assertEqual(results, [[2**63 + 1, None, 10]] * 2)
        complex_left = self.literal(
            self.context.column([1 + 2j, None], dtype="complex128")
        )
        complex_right = self.literal(
            self.context.column([3 - 1j, 1j], dtype="complex128", nullable=True)
        )
        product = self.apply("gr.numeric.multiply@1", [complex_left, complex_right])
        if self.context.numeric_capabilities()["supported"]:
            self.assertEqual(
                self.run_outputs([product]).outputs[0].tolist(), [5 + 5j, None]
            )
        else:
            with product.compile() as program:
                with program.run(self.runtime) as result:
                    self.assertEqual(result.status, "failed")
        with self.assertRaises(gr.NativeError):
            self.apply("gr.numeric.add@1", [left, complex_left])
        with self.assertRaises(ValueError):
            self.context.column([complex(float("inf"), 0)], dtype="complex128")

    @unittest.skipUnless(importlib.util.find_spec("numpy"), "NumPy is optional")
    def test_numpy_uint64_complex_copy_and_export_lifetime(self):
        import numpy as np

        for array in (
            np.array([0, 2**64 - 1], dtype=np.uint64),
            np.array([1 + 2j, -3j], dtype=np.complex128),
        ):
            expected = array.copy()
            value = self.own(self.context.from_numpy(array))
            array[:] = 0
            result = value.numpy(copy=False)
            value.close()
            self.assertFalse(result.flags.writeable)
            np.testing.assert_array_equal(result, expected)
            with self.assertRaises(ValueError):
                result[0] = 0

    def test_nullable_table_transform_filter_reduce(self):
        numbers = self.own(self.context.column([1, None, 3, 4]))
        enabled = self.own(
            self.context.column([True, False, True, False], dtype="bool")
        )
        table = self.literal(
            self.context.table({"number": numbers, "enabled": enabled})
        )
        selected = self.apply("gr.data.table-column@1", [table], {"name": "number"})
        two = self.literal(self.context.scalar(2))
        one = self.literal(self.context.scalar(1))
        mapped = self.apply("gr.data.affine@1", [selected, two, one])
        predicate = self.apply(
            "gr.data.compare@1",
            [mapped, self.literal(self.context.scalar(5))],
            {"relation": "gt"},
        )
        filtered = self.apply(
            "gr.data.filter@1", [mapped, predicate], {"nulls": "drop"}
        )
        summed = self.apply(
            "gr.data.sum@1", [filtered], {"nulls": "skip", "min_count": 1}
        )
        result = self.run_outputs([mapped, filtered, summed])
        self.assertEqual(
            [v.tolist() for v in result.outputs], [[3, None, 7, 9], [7, 9], 16]
        )
        self.assertTrue(result.outputs[2].provenance)
        self.assertEqual(
            self.run_outputs([summed], serial=True).outputs[0].tolist(), 16
        )

    def test_explicit_missing_policies_and_overflow(self):
        column = self.literal(self.context.column([2**63 - 1, 1, None]))
        missing = self.apply(
            "gr.data.sum@1", [column], {"nulls": "propagate", "min_count": 1}
        )
        self.assertIsNone(self.run_outputs([missing]).outputs[0].tolist())
        overflow = self.apply(
            "gr.data.sum@1", [column], {"nulls": "skip", "min_count": 1}
        )
        program = self.own(overflow.compile())
        failed = self.own(program.run(self.runtime))
        self.assertEqual(failed.status, "failed")
        self.assertEqual(failed.outputs, [])
        with self.assertRaises(gr.NativeError) as caught:
            failed.raise_for_status()
        self.assertEqual(caught.exception.details, failed.error.get("details", {}))
        self.assertIs(caught.exception.diagnostics, failed.diagnostics)
        with self.assertRaises(gr.NativeError):
            self.context.apply("gr.data.sum@1", [column])
        with self.assertRaises(TypeError):
            self.context.scalar(True)

    def test_named_chunk_binding_ownership_and_rebinding(self):
        first = self.own(self.context.column([1, 2, 3]))
        second = self.own(self.context.column([10, 20]))
        source = self.own(self.context.input("chunk", first.spec))
        total = self.apply("gr.data.sum@1", [source], {"nulls": "skip", "min_count": 0})
        plans = [
            self.own(total.compile(bindings={"chunk": chunk}))
            for chunk in (first, second)
        ]
        first.close()
        second.close()
        source.close()
        total.close()
        self.context.close()
        self.assertEqual(
            [self.own(p.run(self.runtime)).outputs[0].tolist() for p in plans], [6, 30]
        )

    def test_sparse_two_physical_providers(self):
        matrix = self.literal(self.context.csr(2, 3, [0, 2, 3], [0, 2, 1], [2, 3, 4]))
        vector = self.literal(self.context.column([10, 20, 30]))
        outputs = [
            self.apply(
                "gr.sparse.matvec@1",
                [matrix, vector],
                selection={"implementation": provider},
            )
            for provider in (
                "gr.sparse.cpu.matvec-direct@1",
                "gr.sparse.cpu.matvec-decoded@1",
            )
        ]
        self.assertEqual(
            [v.tolist() for v in self.run_outputs(outputs).outputs],
            [[110, 80], [110, 80]],
        )
        with self.assertRaises(gr.NativeError):
            self.context.csr(1, 2, [0, 2], [1, 1], [2, 3])

    def test_uninstalled_extension_type_errors_and_strict_wire(self):
        with self.assertRaises(gr.NativeError) as missing:
            data.Context(native=self.native, modules=["gr.data.sparse@1"])
        self.assertEqual(missing.exception.code, "missing_module")
        with gr.Buffer(
            struct.pack(">q", 1),
            native=self.native,
            layout={
                "element": {
                    "format_id": "gr.int64@1",
                    "item_size": 8,
                    "alignment": 8,
                    "byte_order": "big",
                },
                "shape": [1],
            },
        ) as typed:
            with self.assertRaises(gr.NativeError):
                self.native.call(
                    "data.create",
                    handle=self.context._resource_handle(),
                    kind="scalar_i64",
                    values=typed._resource_handle(),
                )
        with data.Context(native=self.native) as standard:
            with self.assertRaises(gr.NativeError):
                standard.csr(1, 1, [0, 1], [0], [1])
        column = self.literal(self.context.column([1, 2]))
        with self.assertRaises(gr.NativeError):
            self.context.apply("gr.data.affine@1", [column, column, column])
        with self.assertRaises(gr.NativeError):
            self.native.call(
                "data.input",
                handle=self.context._resource_handle(),
                name="bad",
                spec={
                    "type": {
                        "family": "gr.data.int64@1",
                        "arguments": [],
                        "parameters": {},
                    },
                    "representation": "gr.data.scalar-le@1",
                },
                typo=True,
            )
        with self.assertRaises(gr.NativeError):
            self.context.input(
                "bad", data.ValueSpec(data.Type("unknown.type@1"), "unknown.layout@1")
            )

    def test_native_freeze_and_generic_import(self):
        parameters = {"nullable": False, "nested": {"names": ["a"]}}
        descriptor = data.Type("test.type@1", [], parameters)
        parameters["nested"]["names"].append("b")
        self.assertEqual(descriptor._encode()["parameters"]["nested"]["names"], ["a"])
        with self.assertRaises(TypeError):
            descriptor.parameters["nullable"] = True
        source = self.own(self.context.column([9, 8]))
        exported = source.payload()
        self.addCleanup(exported.close)
        provenance = {"agent": "test", "uncertainty": "unassessed"}
        copied = self.own(
            self.context.import_value(source.spec, exported, provenance=provenance)
        )
        expression = self.own(self.context.literal(copied))
        provenance["agent"] = "changed"
        source.close()
        copied.close()
        exported.close()
        result = self.run_outputs([expression])
        self.assertEqual(result.outputs[0].tolist(), [9, 8])
        self.assertNotIn('"agent": "changed"', json.dumps(result.outputs[0].provenance))

    def test_async_lifetimes_and_actual_plan_identity(self):
        column = self.literal(self.context.column(range(1000)))
        total = self.apply("gr.data.sum@1", [column], {"nulls": "skip", "min_count": 1})
        program = self.own(total.compile())
        job = self.own(program.submit(self.runtime))
        program.close()
        self.context.close()

        async def wait():
            return await job

        result = self.own(asyncio.run(wait())).raise_for_status()
        self.assertEqual(result.outputs[0].tolist(), 499500)
        self.runtime.close()
        self.assertEqual(result.outputs[0].tolist(), 499500)
        with data.Context(native=self.native) as other:
            with (
                other.column([7]) as value,
                other.literal(value) as expression,
                expression.compile() as wrong,
            ):
                with self.assertRaises(gr.NativeError):
                    self.native.call(
                        "data.result",
                        handle=wrong._resource_handle(),
                        job=job.job._resource_handle(),
                    )

    def test_partial_result_wrapping_releases_all_handles(self):
        first = self.literal(self.context.column([1]))
        second = self.literal(self.context.column([2]))
        program = self.own(data.compile([first, second]))
        job = self.own(program.submit(self.runtime))
        response = job._poll(60000)
        self.assertTrue(response["ready"])
        result = response["result"]
        original = data.DataValue
        calls = 0

        def create(native, entry):
            nonlocal calls
            calls += 1
            if calls == 2:
                raise MemoryError("injected wrapper allocation failure")
            return original(native, entry)

        with patch.object(data, "DataValue", side_effect=create):
            with self.assertRaises(MemoryError):
                data.DataResult._decode(self.native, result)
        for entry in result["outputs"]:
            with self.assertRaises(gr.NativeError) as failure:
                self.native.call("data.value.describe", handle=entry["handle"])
            self.assertEqual(failure.exception.code, "invalid_handle")

    @unittest.skipUnless(
        importlib.util.find_spec("numpy"), "Optional NumPy is unavailable"
    )
    def test_numpy_copy_import_readonly_export_lifetime_endian_and_strides(self):
        import numpy as np

        source = np.arange(20, dtype=">i8")[::2]
        value = self.own(self.context.from_numpy(source))
        array = value.numpy(copy=False)
        source[:] = -1
        value.close()
        self.assertEqual(array.tolist(), list(range(0, 20, 2)))
        self.assertFalse(array.flags.writeable)
        with self.assertRaises(ValueError):
            array.setflags(write=True)
        del value
        self.assertEqual(int(array.sum()), 90)
        with self.context.column([1, None]) as missing:
            with self.assertRaises(AttributeError):
                missing.spec = data.ValueSpec(data.Type("forged@1"), "forged@1")
            missing._spec = data.ValueSpec(
                data.Type(
                    "gr.data.column@1",
                    (data.Type("gr.data.float64@1"),),
                    {"nullable": False},
                ),
                "gr.data.column-buffers@1",
            )
            with self.assertRaises(ValueError):
                missing.numpy()
        with self.context.from_numpy(np.array([0.0, -0.0, 1.25])) as finite:
            exported = finite.numpy(copy=True)
            self.assertTrue(exported.flags.writeable)
            self.assertEqual(math.copysign(1.0, exported[1]), -1)
        original = self.literal(self.context.column([0, 1, 2, 3, 4]))
        sliced = self.apply(
            "gr.data.slice@1", [original], {"start": 4, "count": 3, "step": -2}
        )
        sliced_result = self.run_outputs([sliced]).outputs[0]
        with self.assertRaisesRegex(ValueError, "copy=True"):
            sliced_result.numpy(copy=False)
        self.assertEqual(sliced_result.numpy(copy=True).tolist(), [4, 2, 0])
        with self.assertRaises(gr.NativeError):
            self.context.from_numpy(np.array([float("nan")]))
        with self.assertRaises(TypeError):
            self.context.from_numpy(np.array([1], dtype="int32"))

    def test_full_int64_binary_and_finite_double_storage(self):
        with self.context.column([-(2**63), 2**63 - 1]) as value:
            self.assertEqual(value.tolist(), [-(2**63), 2**63 - 1])
        with self.context.column([1.25, None, -0.0], dtype="float64") as value:
            self.assertEqual(value.tolist()[:2], [1.25, None])
            self.assertEqual(math.copysign(1, value.tolist()[2]), -1)
        for bad in (float("inf"), float("nan")):
            with self.assertRaises(ValueError):
                self.context.column([bad], dtype="float64")

    def test_qualified_historical_revision_selection(self):
        with gr.Storage(native=self.native, backend="memory") as store:
            store.create("history")

            def record(roles):
                return gr.construct(
                    {
                        "format": "gr.record/1",
                        "id": "claim",
                        "type": "gr.relation@1",
                        "payload": {"status": "asserted"},
                        "participants": [
                            {
                                "role": role,
                                "reference": {"scope": "subject", "id": "person"},
                            }
                            for role in roles
                        ],
                        "attribution": {"agent": "observer", "method": "test"},
                    },
                    native=self.native,
                )

            with store.begin_transaction(
                "history",
                expected_revision="0",
                transaction_id="first",
                actor="test",
                reason="fixture",
                track_history=True,
            ) as tx:
                tx.stage([record(["author", "reviewer", "author"])])
                first = tx.commit()["publication"]["revision"]
            selected = self.own(
                self.context.history(store, "history", physical_revision=first)
            )
            source = self.own(self.context.literal(selected))
            incidence = self.apply("gr.data.incidence@1", [source])
            authors = self.apply(
                "gr.data.select-incidence-roles@1", [incidence], {"roles": ["author"]}
            )
            counts = self.apply("gr.data.count-incidences@1", [authors])
            with store.begin_transaction(
                "history",
                expected_revision=first,
                transaction_id="second",
                actor="test",
                reason="revision",
            ) as tx:
                tx.stage([record(["reviewer"])])
                tx.commit()
            result = self.run_outputs([incidence, authors, counts])
            containers = [v.inspect() for v in result.outputs]
            self.assertIn("author", json.dumps(containers))
            self.assertIn(first, json.dumps(result.outputs[2].provenance))
            self.assertIn("reviewer", json.dumps(containers[0]))

    @unittest.skipUnless(os.environ.get("GR_OS_CLI"), "CLI path unavailable")
    def test_cli_uses_same_language_handles(self):
        process = subprocess.Popen(
            [os.environ["GR_OS_CLI"], "session"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
        )

        def call(operation, /, **parameters):
            process.stdin.write(
                json.dumps({"abi": 1, "op": operation, **parameters}) + "\n"
            )
            process.stdin.flush()
            result = json.loads(process.stdout.readline())
            self.assertTrue(result["ok"], result)
            return result["result"]

        try:
            import base64

            context = call("data.open", modules=["gr.data.standard@1"])["handle"]
            buffer = call(
                "buffer.import_base64",
                data_base64=base64.b64encode(struct.pack("<3q", 4, 5, 6)).decode(),
            )["handle"]
            value = call(
                "data.create", handle=context, kind="column_i64", values=buffer
            )["handle"]
            expression = call("data.literal", handle=context, value=value)["handle"]
            total = call(
                "data.call",
                handle=context,
                operation="gr.data.sum@1",
                inputs=[expression],
                attributes={"nulls": "skip", "min_count": 1},
            )["outputs"][0]["handle"]
            program = call("data.compile", outputs=[total])
            runtime = call("runtime.open", config={"workers": 1})["handle"]
            job = call(
                "runtime.submit", handle=runtime, plan=program["plan"]["handle"]
            )["handle"]
            result = call(
                "data.result", handle=program["handle"], job=job, timeout_ms=60000
            )
            self.assertTrue(result["ready"])
            output = result["result"]["outputs"][0]["handle"]
            self.assertEqual(call("data.value.items", handle=output), 15)
            call("runtime.close", handle=runtime)
            process.stdin.close()
            self.assertEqual(process.wait(timeout=10), 0)
        finally:
            if not process.stdin.closed:
                process.stdin.close()
            if process.poll() is None:
                process.kill()
                process.wait()
            process.stdout.close()
            process.stderr.close()


if __name__ == "__main__":
    unittest.main()
