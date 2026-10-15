"""Layer 3 Python/C transport and optional real NumPy costs; no v0.4 counterpart.

Each phase gets one warmup and five observations. Validation is outside timing.
Execution includes native scheduling, materialization and handle transport; native
prepared-only costs are measured separately by benchmark_data. No performance gate.
"""

import argparse
from contextlib import ExitStack
import hashlib
import importlib.util
import json
from pathlib import Path
import platform
import statistics
import time

import gr_os as gr
from gr_os import data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    rows = []

    def measure(
        name, count, operation, validate=lambda result: None, close=lambda result: None
    ):
        samples = []
        for index in range(6):
            start = time.perf_counter_ns()
            result = operation()
            elapsed = (time.perf_counter_ns() - start) / 1e6
            validate(result)
            close(result)
            if index:
                samples.append(elapsed)
        rows.append(
            {
                "name": name,
                "elements": count,
                "samples_ms": samples,
                "median_ms": statistics.median(samples),
                "minimum_ms": min(samples),
                "maximum_ms": max(samples),
            }
        )

    native = gr.Native()
    numpy_version = None
    with ExitStack() as owned:
        context = owned.enter_context(data.Context(native=native))
        runtime = owned.enter_context(gr.Runtime(native=native, workers=2))
        for count in (65536, 262144):
            source = [index % 97 if index % 11 else None for index in range(count)]
            expected = sum(value for value in source if value is not None)
            measure(
                "python_nullable_column_copy_import",
                count,
                lambda: context.column(source),
                lambda value: value.tolist() == source or fail("import mismatch"),
                lambda value: value.close(),
            )
            value = owned.enter_context(context.column(source))
            literal = owned.enter_context(context.literal(value))

            def infer():
                return context.apply(
                    "gr.data.sum@1",
                    [literal],
                    attributes={"nulls": "skip", "min_count": 1},
                )

            measure(
                "python_inference_call", count, infer, close=lambda expr: expr.close()
            )
            total = owned.enter_context(infer())
            measure(
                "python_compile_warm_literals",
                count,
                total.compile,
                close=lambda program: program.close(),
            )
            program = owned.enter_context(total.compile())
            measure(
                "python_prepared_program_run_and_materialize",
                count,
                lambda: program.run(runtime),
                lambda result: (
                    result.raise_for_status().outputs[0].tolist() == expected
                    or fail("sum mismatch")
                ),
                lambda result: result.close(),
            )
            measure(
                "c_json_value_items_to_python_list",
                count,
                lambda: native.call(
                    "data.value.items", handle=value._resource_handle()
                ),
                lambda result: result == source or fail("transport mismatch"),
            )
            if importlib.util.find_spec("numpy"):
                import numpy as np

                numpy_version = np.__version__
                array = np.arange(count, dtype=np.int64)
                measure(
                    "numpy_native_copy_import",
                    count,
                    lambda: context.from_numpy(array),
                    lambda value: (
                        value.tolist() == array.tolist()
                        or fail("numpy import mismatch")
                    ),
                    lambda value: value.close(),
                )
                native_array = owned.enter_context(context.from_numpy(array))
                measure(
                    "numpy_readonly_export",
                    count,
                    lambda: native_array.numpy(copy=False),
                    lambda exported: (
                        not exported.flags.writeable
                        and np.array_equal(exported, array)
                        or fail("numpy export mismatch")
                    ),
                )
                measure(
                    "numpy_writable_copy_export",
                    count,
                    lambda: native_array.numpy(copy=True),
                    lambda exported: (
                        exported.flags.writeable
                        and np.array_equal(exported, array)
                        or fail("numpy copy mismatch")
                    ),
                )
    output = {
        "version": "0.5.0",
        "python": platform.python_version(),
        "numpy": numpy_version,
        "platform": platform.platform(),
        "native_library": str(native.path),
        "native_sha256": hashlib.sha256(native.path.read_bytes()).hexdigest(),
        "source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "warmups": 1,
        "repeats": 5,
        "cases": rows,
        "scope": "End-to-end Python/C ABI phases; cleanup and output checks outside timing. No v0.4 language API equivalent. Uncontrolled host background/power state.",
    }
    args.output.write_text(json.dumps(output, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"cases": len(rows), "output": str(args.output)}))


def fail(message):
    raise AssertionError(message)


if __name__ == "__main__":
    main()
