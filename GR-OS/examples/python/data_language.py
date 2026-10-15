"""A reusable bounded expression template lowered through the native runtime.

Set GR_OS_LIBRARY and install the Python facade (or set PYTHONPATH). Each chunk
compiles a separately pinned program; this example is not a streaming engine.
"""

from contextlib import ExitStack
import json

import gr_os as gr
from gr_os import data


def main():
    with ExitStack() as owned:
        context = owned.enter_context(
            data.Context(modules=("gr.data.standard@1", "gr.data.numeric@1"))
        )
        runtime = owned.enter_context(gr.Runtime(workers=2))
        prototype = owned.enter_context(context.column([1, None, 3]))
        source = owned.enter_context(context.input("chunk", prototype.spec))
        two = owned.enter_context(context.scalar(2))
        one = owned.enter_context(context.scalar(1))
        multiplier = owned.enter_context(context.literal(two))
        addend = owned.enter_context(context.literal(one))
        transformed = owned.enter_context(
            context.apply("gr.data.affine@1", [source, multiplier, addend])
        )
        total = owned.enter_context(
            context.apply(
                "gr.data.sum@1",
                [transformed],
                attributes={"nulls": "skip", "min_count": 1},
            )
        )
        function = owned.enter_context(total.function())
        for items in ([1, None, 3], [4, 5, None]):
            with context.column(items, nullable=True) as chunk:
                with function.bind({"chunk": chunk}) as program:
                    with program.run(runtime) as result:
                        result.raise_for_status()
                        print(
                            json.dumps(
                                {
                                    "sum": result.outputs[0].tolist(),
                                    "lineage": result.outputs[0].provenance,
                                }
                            )
                        )
        left = owned.enter_context(context.column([2**63, 7], dtype="uint64"))
        right = owned.enter_context(context.column([1, 2], dtype="uint64"))
        operands = [
            owned.enter_context(context.literal(value)) for value in (left, right)
        ]
        addition = owned.enter_context(context.apply("gr.numeric.add@1", operands))
        with addition.function() as arithmetic:
            with arithmetic.bind() as program:
                with program.run(runtime) as result:
                    result.raise_for_status()
                    assert result.outputs[0].tolist() == [2**63 + 1, 9]
                    print(
                        json.dumps(
                            {
                                "uint64": result.outputs[0].tolist(),
                                "floating_capability": context.numeric_capabilities(),
                            }
                        )
                    )
                    try:
                        import numpy as np
                    except ImportError:
                        pass
                    else:
                        exported = result.outputs[0].numpy(copy=False)
                        assert (
                            exported.dtype == np.dtype("<u8")
                            and not exported.flags.writeable
                        )
                        print("NumPy readonly native export:", exported)


if __name__ == "__main__":
    main()
