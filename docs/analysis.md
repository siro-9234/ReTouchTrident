# Touch cursor suppression analysis

## Scope

This document records the debugger-confirmed behavior used to design the touch-cursor suppression feature in `TridentCaptureFilter`.

Target used for the validation session:

- Windows 11 25H2
- OS build 26200.8737
- x64
- Module: `win32kfull.sys`
- Symbol: ``win32kfull!`anonymous namespace'::DeferPointerCursorOperation``

The addresses below are examples from one boot only. ASLR makes absolute addresses unsuitable for implementation.

## Confirmed call path

A physical HID touch event reached the target function through this path:

```text
CHidInput::ProcessInput
CTouchProcessor::ProcessInput
CTouchProcessor::ProcessInputPostDelayZonePalmRejection
CTouchProcessor::DoContactVisualizationAndGenerateMessages
CTouchProcessor::DoContactVisualization
EditionContactVisualization
ContactVisualizationWorker
DeferPointerCursorOperation
```

## Entry arguments

At function entry, debugger inspection confirmed:

```text
RCX -> POINTER_INFO-compatible data
RDX -> cursor operation data { PointerFlags, x, y }
```

The first DWORD at `RCX` was `2`, which corresponds to `PT_TOUCH`.

Observed layout at `RCX` matched the public `POINTER_INFO` layout for the fields used by the function:

```text
+0x00 pointerType
+0x04 pointerId
+0x08 frameId
+0x0C pointerFlags
+0x10 sourceDevice
+0x18 hwndTarget
+0x20 ptPixelLocation.x
+0x24 ptPixelLocation.y
```

The cursor operation payload at `RDX` was observed as:

```text
+0x00 PointerFlags
+0x04 x
+0x08 y
```

## Confirmed suppression boundary

For the tested binary, `DeferPointerCursorOperation + 0xEC` was immediately after:

- `ValidateHwnd`
- `CInputDest` construction
- `UpdateGlobalCursorOwner`
- `CInputDest` destruction

and immediately before the deferred cursor-operation queue logic.

At this boundary:

```text
RBX -> original POINTER_INFO-compatible data
RSI -> cursor operation data
```

The first DWORD at `RBX` remained `PT_TOUCH` (`2`).

Skipping from this boundary to the normal epilogue for touch events only was tested repeatedly with WinDbg. During the test:

- touch input continued to work
- tap/click behavior continued to work
- gestures continued to work
- no immediate cursor-state regression was observed

This validates the architectural boundary, not a production patch.

## Safety requirements for implementation

The production implementation must fail closed:

1. Never use absolute kernel addresses.
2. Require an explicitly supported OS/build profile.
3. Locate the module and candidate function without undocumented address assumptions where possible.
4. Verify all expected instruction bytes and control-flow targets before enabling any interception.
5. If any check fails, leave the system untouched and report a diagnostic state.
6. Installation and removal must be reversible and synchronized.
7. The initial scaffold must not modify executable kernel memory.

## Current scaffold status

The first implementation stage intentionally performs only:

- OS/build discovery
- supported-profile selection
- module-location interface setup
- signature-validation interface setup
- hook lifecycle state management
- diagnostic logging

No inline patch, trampoline, executable-memory write, or control-flow redirection is implemented in this stage.

## Chapter II Phase 2: discovery implementation

The safe discovery layer now performs the following without modifying executable memory:

1. Checks for the validated OS build (`26200`).
2. Enumerates loaded kernel modules through `AuxKlibQueryModuleInformation`.
3. Locates `win32kfull.sys` by filename.
4. Validates the mapped PE image and obtains its executable `.text` section.
5. Searches `.text` for a unique, masked x64 signature matching the observed
   `DeferPointerCursorOperation` prologue.
6. Verifies additional fixed instruction sequences at:
   - function `+0x2F` (`RBX <- RCX`, `RSI <- RDX`),
   - suppression boundary `+0xEC` (`xor ebx, ebx`),
   - skip target `+0x164` (validated epilogue entry).
7. Logs the module base, `.text` range, function RVA, suppression RVA, and skip RVA.
8. Configures `HookManager` with the validated addresses, while leaving hook enablement
   intentionally unsupported. No kernel code is patched in Phase 2.

Any missing module, malformed PE image, ambiguous signature, or byte mismatch causes a
fail-closed result. The capture filter continues loading without cursor suppression.
