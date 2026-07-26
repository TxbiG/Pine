# Native IR handoff comparison

`native_ir_handoff.pine` exercises expression stack operations, calls,
control-flow labels, and frame slots through the shared Pine IR lowering.

Generate and compare both debug artifacts:

```sh
pine ir examples/native_ir_handoff.pine
pine native examples/native_ir_handoff.pine --target x86_64
pine native examples/native_ir_handoff.pine --target aarch64
pine native examples/native_ir_handoff.pine --target riscv64
```

The checked snapshots are:

- `native_ir_handoff.expected.ir`
- `native_ir_handoff.expected.native`

The checked native snapshot uses the default `x86_64` target. The native
header intentionally reports `instruction_selection unsupported`, `abi
unsupported`, and `object_emission unsupported`. Its target triple, offsets,
sizes, alignment, frame size, target stack size, and operand-stack maximum are
compiler-development metadata, not a platform ABI.
