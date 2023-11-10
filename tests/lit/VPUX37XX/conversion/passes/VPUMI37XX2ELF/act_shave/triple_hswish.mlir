//
// Copyright (C) 2022-2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

// RUN: vpux-opt --init-compiler="vpu-arch=VPUX37XX allow-custom-values=true" --convert-VPUMI37XX-to-ELF %s | FileCheck %s

module @Test attributes {VPU.arch = #VPU.arch_kind<VPUX37XX>, VPU.compilationMode = #VPU.compilation_mode<ReferenceHW>} {
  IE.MemoryResource 31457280 bytes of @DDR {VPU.bandwidth = 8 : i64, VPU.derateFactor = 6.000000e-01 : f64}
  IE.ExecutorResource 1 of @DMA_NN
  IE.ExecutorResource 1 of @NCE {
    IE.MemoryResource 2097152 bytes of @CMX_NN {VPU.bandwidth = 32 : i64, VPU.derateFactor = 1.000000e+00 : f64}
    IE.ExecutorResource 1 of @SHAVE_UPA
    IE.ExecutorResource 1 of @SHAVE_ACT
    IE.ExecutorResource 1 of @DPU
  }
  IE.CNNNetwork entryPoint : @main inputsInfo : {
    DataInfo "input" : tensor<1x1000xf16>
  } outputsInfo : {
    DataInfo "hswish" : tensor<1x1000xf16>
  }
  VPURT.SW.Runtime entryPoint : @VPU.SW::@runtime stack_configuration : [4096, 4096]
  module @VPU.SW {
    func.func private @builtin_hswish(memref<*xf16>, memref<*xf16>) attributes {VPU.kernel_code = "hswish_fp16.cpp", VPU.kernel_entry = "hswish_fp16"}
    func.func private @runtime() attributes {VPU.kernel_code = "nnActEntry"}
  }
  func.func @main(%arg0: memref<1x1x1x1000xf16>, %arg1: memref<1x1x1x1000xf16>) -> memref<1x1x1x1000xf16> {
    %0 = VPURT.DeclareBuffer <CMX_NN> [0] <0> -> memref<1x1x1x1000xf16, [@CMX_NN, 0]>
    %1 = VPURT.DeclareBuffer <CMX_NN> [0] <2000> -> memref<1x1x1x1000xf16, [@CMX_NN, 0]>
    %2 = VPUMI37XX.ConfigureBarrier {consumer_count = 1 : ui8, producer_count = 1 : ui8}<0, -1> -> !VPURegMapped.Index<0:0:0>
    %3 = VPUMI37XX.ConfigureBarrier {consumer_count = 1 : ui8, producer_count = 1 : ui8}<1, -1> -> !VPURegMapped.Index<0:0:1>
    %4 = VPUMI37XX.NNDMA {port = 0 : i64} inputs(%arg0 : memref<1x1x1x1000xf16>) outputs(%0 : memref<1x1x1x1000xf16, [@CMX_NN, 0]>) updates(%2 : !VPURegMapped.Index<0:0:0>) start_after(0) clean_after(0) -> !VPURegMapped.Index<0:0:0>
    %5 = VPUMI37XX.DeclareKernelText kernel_path("hswish_fp16") -> !VPURegMapped.Index<0:0:0>
    %6 = VPUMI37XX.DeclareKernelArgs kernel_path("hswish_fp16") -> !VPURegMapped.Index<0:0:0>
    %7 = VPUMI37XX.DeclareKernelEntry kernel_path("hswish_fp16") -> !VPURegMapped.Index<0:0:0>
    %8 = VPUMI37XX.ActKernelRange kernel_text_index(%5 : <0:0:0>) kernel_args_index(%6 : <0:0:0>) kernel_entry_index(%7 : <0:0:0>) -> !VPURegMapped.Index<0:0:0>
    %9 = VPUMI37XX.ActKernelInvocation range_index(%8 : <0:0:0>) waits(%2 : !VPURegMapped.Index<0:0:0>) updates(%3 : !VPURegMapped.Index<0:0:1>) tile(0) start_after(0) clean_after(0) -> !VPURegMapped.Index<0:0:0>
    %10 = VPUMI37XX.KernelParams inputs(%0 : memref<1x1x1x1000xf16, [@CMX_NN, 0]>) outputs(%1 : memref<1x1x1x1000xf16, [@CMX_NN, 0]>) kernel_type("hswish_fp16") kernel_params(dense<[0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 33, 67, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 33, 67, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0]> : vector<72xui8>) -> !VPURegMapped.Index<0:0:0>
    %11 = VPUMI37XX.NNDMA {port = 0 : i64} inputs(%1 : memref<1x1x1x1000xf16, [@CMX_NN, 0]>) outputs(%arg1 : memref<1x1x1x1000xf16>) previousDMA(%4 : !VPURegMapped.Index<0:0:0>) waits(%3 : !VPURegMapped.Index<0:0:1>) start_after(0) clean_after(0) -> !VPURegMapped.Index<0:0:1>
    %12 = VPURT.DeclareBuffer <CMX_NN> [0] <2000> -> memref<1x1x1x1000xf16, [@CMX_NN, 0]>
    %13 = VPURT.DeclareBuffer <CMX_NN> [0] <4000> -> memref<1x1x1x1000xf16, [@CMX_NN, 0]>
    %14 = VPUMI37XX.ConfigureBarrier {consumer_count = 1 : ui8, producer_count = 1 : ui8}<0, -1> -> !VPURegMapped.Index<0:0:2>
    %15 = VPUMI37XX.ConfigureBarrier {consumer_count = 1 : ui8, producer_count = 1 : ui8}<1, -1> -> !VPURegMapped.Index<0:0:3>
    %16 = VPUMI37XX.NNDMA {port = 0 : i64} inputs(%arg1 : memref<1x1x1x1000xf16>) outputs(%12 : memref<1x1x1x1000xf16, [@CMX_NN, 0]>) previousDMA(%11 : !VPURegMapped.Index<0:0:1>) updates(%14 : !VPURegMapped.Index<0:0:2>) start_after(0) clean_after(0) -> !VPURegMapped.Index<0:0:2>
    %17 = VPUMI37XX.DeclareKernelText kernel_path("hswish_fp16") -> !VPURegMapped.Index<0:0:1>
    %18 = VPUMI37XX.DeclareKernelArgs kernel_path("hswish_fp16") -> !VPURegMapped.Index<0:0:1>
    %19 = VPUMI37XX.DeclareKernelEntry kernel_path("hswish_fp16") -> !VPURegMapped.Index<0:0:1>
    %20 = VPUMI37XX.ActKernelRange kernel_text_index(%17 : <0:0:1>) kernel_args_index(%18 : <0:0:1>) kernel_entry_index(%19 : <0:0:1>) -> !VPURegMapped.Index<0:0:1>
    %21 = VPUMI37XX.ActKernelInvocation range_index(%20 : <0:0:1>) waits(%14 : !VPURegMapped.Index<0:0:2>) updates(%15 : !VPURegMapped.Index<0:0:3>) tile(0) start_after(0) clean_after(0) -> !VPURegMapped.Index<0:0:1>
    %22 = VPUMI37XX.KernelParams inputs(%12 : memref<1x1x1x1000xf16, [@CMX_NN, 0]>) outputs(%13 : memref<1x1x1x1000xf16, [@CMX_NN, 0]>) kernel_type("hswish_fp16") kernel_params(dense<[0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 33, 67, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 33, 67, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0]> : vector<72xui8>) -> !VPURegMapped.Index<0:0:1>
    %23 = VPUMI37XX.NNDMA {port = 0 : i64} inputs(%13 : memref<1x1x1x1000xf16, [@CMX_NN, 0]>) outputs(%arg1 : memref<1x1x1x1000xf16>) previousDMA(%16 : !VPURegMapped.Index<0:0:2>) waits(%15 : !VPURegMapped.Index<0:0:3>) start_after(0) clean_after(0) -> !VPURegMapped.Index<0:0:3>
    %24 = VPURT.DeclareBuffer <CMX_NN> [0] <6000> -> memref<1x1x1x1000xf16, [@CMX_NN, 0]>
    %25 = VPURT.DeclareBuffer <CMX_NN> [0] <8000> -> memref<1x1x1x1000xf16, [@CMX_NN, 0]>
    %26 = VPUMI37XX.ConfigureBarrier {consumer_count = 1 : ui8, producer_count = 1 : ui8}<0, -1> -> !VPURegMapped.Index<0:0:4>
    %27 = VPUMI37XX.ConfigureBarrier {consumer_count = 1 : ui8, producer_count = 1 : ui8}<1, -1> -> !VPURegMapped.Index<0:0:5>
    %28 = VPUMI37XX.NNDMA {port = 0 : i64} inputs(%arg1 : memref<1x1x1x1000xf16>) outputs(%24 : memref<1x1x1x1000xf16, [@CMX_NN, 0]>) previousDMA(%23 : !VPURegMapped.Index<0:0:3>) updates(%26 : !VPURegMapped.Index<0:0:4>) start_after(0) clean_after(0) -> !VPURegMapped.Index<0:0:4>
    %29 = VPUMI37XX.DeclareKernelText kernel_path("hswish_fp16") -> !VPURegMapped.Index<0:0:2>
    %30 = VPUMI37XX.DeclareKernelArgs kernel_path("hswish_fp16") -> !VPURegMapped.Index<0:0:2>
    %31 = VPUMI37XX.DeclareKernelEntry kernel_path("hswish_fp16") -> !VPURegMapped.Index<0:0:2>
    %32 = VPUMI37XX.ActKernelRange kernel_text_index(%29 : <0:0:2>) kernel_args_index(%30 : <0:0:2>) kernel_entry_index(%31 : <0:0:2>) -> !VPURegMapped.Index<0:0:2>
    %33 = VPUMI37XX.ActKernelInvocation range_index(%32 : <0:0:2>) waits(%26 : !VPURegMapped.Index<0:0:4>) updates(%27 : !VPURegMapped.Index<0:0:5>) tile(0) start_after(0) clean_after(0) -> !VPURegMapped.Index<0:0:2>
    %34 = VPUMI37XX.KernelParams inputs(%24 : memref<1x1x1x1000xf16, [@CMX_NN, 0]>) outputs(%25 : memref<1x1x1x1000xf16, [@CMX_NN, 0]>) kernel_type("hswish_fp16") kernel_params(dense<[0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 33, 67, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 33, 67, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0]> : vector<72xui8>) -> !VPURegMapped.Index<0:0:2>
    %35 = VPUMI37XX.NNDMA {port = 0 : i64} inputs(%25 : memref<1x1x1x1000xf16, [@CMX_NN, 0]>) outputs(%arg1 : memref<1x1x1x1000xf16>) previousDMA(%28 : !VPURegMapped.Index<0:0:4>) waits(%27 : !VPURegMapped.Index<0:0:5>) start_after(0) clean_after(0) -> !VPURegMapped.Index<0:0:5>
    %36 = VPUMI37XX.MappedInference dmas(%4 : !VPURegMapped.Index<0:0:0>) actKernelRanges(%8 : !VPURegMapped.Index<0:0:0>) actKernelInvocations(%9 : !VPURegMapped.Index<0:0:0>) barriers(%2 : !VPURegMapped.Index<0:0:0>) dmaCount([6]) invariantCount(0) variantCount(0) actKernelRangesCount(3) actKernelInvocationsCount(3) barrierCount(6) -> !VPURegMapped.Index<0:0:0>
    return %arg1 : memref<1x1x1x1000xf16>
  }
}

//CHECK: %[[VAL0:.*]] = VPURT.DeclareBuffer
//CHECK: %[[VAL1:.*]] = VPURT.DeclareBuffer
//CHECK: %[[VAL2:.*]] = VPUMI37XX.ConfigureBarrier
//CHECK: %[[VAL3:.*]] = VPUMI37XX.ConfigureBarrier
//CHECK: %[[VAL4:.*]] = VPUMI37XX.NNDMA
//CHECK: %[[VAL5:.*]] = VPUMI37XX.DeclareKernelText
//CHECK: %[[VAL6:.*]] = VPUMI37XX.DeclareKernelArgs
//CHECK: %[[VAL7:.*]] = VPUMI37XX.DeclareKernelEntry
//CHECK: %[[VAL8:.*]] = VPUMI37XX.ActKernelRange
//CHECK: %[[VAL9:.*]] = VPUMI37XX.ActKernelInvocation
//CHECK: %[[VAL10:.*]] = VPUMI37XX.KernelParams
//CHECK: %[[VAL11:.*]] = VPUMI37XX.NNDMA

//CHECK: %[[VAL12:.*]] = VPURT.DeclareBuffer
//CHECK: %[[VAL13:.*]] = VPURT.DeclareBuffer
//CHECK: %[[VAL14:.*]] = VPUMI37XX.ConfigureBarrier
//CHECK: %[[VAL15:.*]] = VPUMI37XX.ConfigureBarrier
//CHECK: %[[VAL16:.*]] = VPUMI37XX.NNDMA
//CHECK: %[[VAL17:.*]] = VPUMI37XX.DeclareKernelText
//CHECK: %[[VAL18:.*]] = VPUMI37XX.DeclareKernelArgs
//CHECK: %[[VAL19:.*]] = VPUMI37XX.DeclareKernelEntry
//CHECK: %[[VAL20:.*]] = VPUMI37XX.ActKernelRange
//CHECK: %[[VAL21:.*]] = VPUMI37XX.ActKernelInvocation
//CHECK: %[[VAL22:.*]] = VPUMI37XX.KernelParams
//CHECK: %[[VAL23:.*]] = VPUMI37XX.NNDMA

//CHECK: %[[VAL24:.*]] = VPURT.DeclareBuffer
//CHECK: %[[VAL25:.*]] = VPURT.DeclareBuffer
//CHECK: %[[VAL26:.*]] = VPUMI37XX.ConfigureBarrier
//CHECK: %[[VAL27:.*]] = VPUMI37XX.ConfigureBarrier
//CHECK: %[[VAL28:.*]] = VPUMI37XX.NNDMA
//CHECK: %[[VAL29:.*]] = VPUMI37XX.DeclareKernelText
//CHECK: %[[VAL30:.*]] = VPUMI37XX.DeclareKernelArgs
//CHECK: %[[VAL31:.*]] = VPUMI37XX.DeclareKernelEntry
//CHECK: %[[VAL32:.*]] = VPUMI37XX.ActKernelRange
//CHECK: %[[VAL33:.*]] = VPUMI37XX.ActKernelInvocation
//CHECK: %[[VAL34:.*]] = VPUMI37XX.KernelParams
//CHECK: %[[VAL35:.*]] = VPUMI37XX.NNDMA

//CHECK-DAG: %[[VAL36:.*]] = VPURT.DeclareBuffer <DDR>
//CHECK-NEXT: %[[VAL37:.*]] = ELF.CreateLogicalSection secType(SHT_NOBITS) secFlags(VPU_SHF_PROC_SHAVE) {secAddrAlign = 1024 : i64, secInfo = 0 : i64, secName = ".bss.actShaveStack_0"} -> !ELF.Section {
//CHECK-NEXT: ELF.PutOpInSection %[[VAL36]] : memref
//CHECK: %[[VAL38:.*]]  = ELF.Symbol %[[VAL37]] name("sym_actShaveStack_0") : !ELF.Section

//CHECK-DAG: %[[VAL39:.*]] = VPURT.DeclareBuffer <DDR>
//CHECK-NEXT: %[[VAL40:.*]] = ELF.CreateLogicalSection secType(SHT_NOBITS) secFlags(VPU_SHF_PROC_SHAVE) {secAddrAlign = 1024 : i64, secInfo = 0 : i64, secName = ".bss.actShaveStack_1"} -> !ELF.Section {
//CHECK-NEXT: ELF.PutOpInSection %[[VAL39]] : memref
//CHECK: %[[VAL41:.*]]  = ELF.Symbol %[[VAL40]] name("sym_actShaveStack_1") : !ELF.Section

//CHECK-DAG: %[[VAL42:.*]] = VPURT.DeclareBuffer <DDR>
//CHECK-NEXT: %[[VAL43:.*]] = ELF.CreateLogicalSection secType(SHT_NOBITS) secFlags(VPU_SHF_PROC_SHAVE) {secAddrAlign = 1024 : i64, secInfo = 0 : i64, secName = ".bss.actShaveStack_2"} -> !ELF.Section {
//CHECK-NEXT: ELF.PutOpInSection %[[VAL42]] : memref
//CHECK: %[[VAL44:.*]]  = ELF.Symbol %[[VAL43]] name("sym_actShaveStack_2") : !ELF.Section

//CHECK-DAG: %[[VAL45:.*]] = VPURT.DeclareBuffer <DDR>
//CHECK-NEXT: %[[VAL46:.*]] = ELF.CreateLogicalSection secType(SHT_NOBITS) secFlags(VPU_SHF_PROC_SHAVE) {secAddrAlign = 1024 : i64, secInfo = 0 : i64, secName = ".bss.actShaveStack_3"} -> !ELF.Section {
//CHECK-NEXT: ELF.PutOpInSection %[[VAL45]] : memref
//CHECK: %[[VAL47:.*]]  = ELF.Symbol %[[VAL46]] name("sym_actShaveStack_3") : !ELF.Section

//CHECK-DAG: %[[VAL48:.*]] = VPUMI37XX.ActShaveRt kernel("nnActEntry") -> !VPURegMapped.Index<0:0:0>
//CHECK-NEXT: %[[VAL49:.*]] = ELF.CreateSection {{.*}} {secAddrAlign = 1024 : i64, secInfo = 0 : i64, secName = ".text.actKernelRtConfigSec"} -> !ELF.Section {
//CHECK-NEXT: ELF.PutOpInSection %[[VAL48]] : !VPURegMapped.Index<0:0:0>
//CHECK: %[[VAL50:.*]]  = ELF.Symbol %[[VAL49]] name("sym_actKernelRtConfigsSec") : !ELF.Section

//CHECK-DAG: %[[VAL51:.*]] = ELF.CreateSymbolTableSection secName(".symtab.actKernelRtConfig")
//CHECK-NEXT: ELF.PutOpInSection %[[VAL50]] : !ELF.Symbol
//CHECK-NEXT: ELF.PutOpInSection %[[VAL38]] : !ELF.Symbol
//CHECK-NEXT: ELF.PutOpInSection %[[VAL41]] : !ELF.Symbol
//CHECK-NEXT: ELF.PutOpInSection %[[VAL44]] : !ELF.Symbol
//CHECK-NEXT: ELF.PutOpInSection %[[VAL47]] : !ELF.Symbol

//CHECK: %[[VAL52:.*]] = VPUMI37XX.MappedInference
//CHECK-SAME: dmas(%[[VAL4]] : !VPURegMapped.Index<0:0:0>)
//CHECK-SAME: actKernelRanges(%[[VAL8]] : !VPURegMapped.Index<0:0:0>)
//CHECK-SAME: actKernelInvocations(%[[VAL9]] : !VPURegMapped.Index<0:0:0>)
//CHECK-SAME: barriers(%[[VAL2]] : !VPURegMapped.Index<0:0:0>)
//CHECK-SAME: actShaveRt(%[[VAL48]] : !VPURegMapped.Index<0:0:0>)
//CHECK-SAME: actShaveStacks(%[[VAL36]], %[[VAL39]], %[[VAL42]], %[[VAL45]] : {{.*}}>)
//CHECK-SAME: dmaCount([6]) invariantCount(0) variantCount(0) actKernelRangesCount(3) actKernelInvocationsCount(3) barrierCount(6)
//CHECK-SAME: -> !VPURegMapped.Index<0:0:0>


//CHECK-DAG: %[[DMASEC:.*]] = ELF.CreateSection {{.*}} secName = ".text.dmaTasks0"
//CHECK-NEXT: ELF.PutOpInSection %[[VAL4]] : !VPURegMapped.Index<0:0:0>
//CHECK-NEXT: ELF.PutOpInSection %[[VAL11]] : !VPURegMapped.Index<0:0:1>
//CHECK-NEXT: ELF.PutOpInSection %[[VAL16]] : !VPURegMapped.Index<0:0:2>
//CHECK-NEXT: ELF.PutOpInSection %[[VAL23]] : !VPURegMapped.Index<0:0:3>
//CHECK-NEXT: ELF.PutOpInSection %[[VAL28]] : !VPURegMapped.Index<0:0:4>
//CHECK-NEXT: ELF.PutOpInSection %[[VAL35]] : !VPURegMapped.Index<0:0:5>

//CHECK-DAG: %[[BARSEC:.*]] = ELF.CreateSection {{.*}} secName = ".text.BarrierConfigs"
//CHECK-NEXT: ELF.PutOpInSection %[[VAL2]] : !VPURegMapped.Index<0:0:0>
//CHECK-NEXT: ELF.PutOpInSection %[[VAL3]] : !VPURegMapped.Index<0:0:1>
//CHECK-NEXT: ELF.PutOpInSection %[[VAL14]] : !VPURegMapped.Index<0:0:2>
//CHECK-NEXT: ELF.PutOpInSection %[[VAL15]] : !VPURegMapped.Index<0:0:3>
//CHECK-NEXT: ELF.PutOpInSection %[[VAL26]] : !VPURegMapped.Index<0:0:4>
//CHECK-NEXT: ELF.PutOpInSection %[[VAL27]] : !VPURegMapped.Index<0:0:5>

//CHECK-DAG: %[[KERNELTEXT:.*]] = ELF.CreateSection {{.*}} secName = ".text.KernelText"
//CHECK-NEXT: ELF.PutOpInSection %[[VAL5]] : !VPURegMapped.Index<0:0:0>
//CHECK-NEXT: ELF.Pad {{.*}}
//CHECK-NEXT: ELF.PutOpInSection %[[VAL17]] : !VPURegMapped.Index<0:0:1>
//CHECK-NEXT: ELF.Pad {{.*}}
//CHECK-NEXT: ELF.PutOpInSection %[[VAL29]] : !VPURegMapped.Index<0:0:2>

//CHECK-DAG: %[[KERNELDATA:.*]] = ELF.CreateSection {{.*}} secName = ".text.KernelData"
//CHECK-NEXT: ELF.PutOpInSection %[[VAL6]] : !VPURegMapped.Index<0:0:0>
//CHECK-NEXT: ELF.PutOpInSection %[[VAL18]] : !VPURegMapped.Index<0:0:1>
//CHECK-NEXT: ELF.PutOpInSection %[[VAL30]] : !VPURegMapped.Index<0:0:2>

//CHECK-DAG: %[[KERNELPARAMS:.*]] = ELF.CreateSection {{.*}} secName = ".text.KernelParams"
//CHECK-NEXT: ELF.PutOpInSection %[[VAL10]] : !VPURegMapped.Index<0:0:0>
//CHECK-NEXT: ELF.Pad {{.*}}
//CHECK-NEXT: ELF.PutOpInSection %[[VAL22]] : !VPURegMapped.Index<0:0:1>
//CHECK-NEXT: ELF.Pad {{.*}}
//CHECK-NEXT: ELF.PutOpInSection %[[VAL34]] : !VPURegMapped.Index<0:0:2>

//CHECK-DAG: %[[ACTKERNELR:.*]] = ELF.CreateSection {{.*}} secName = ".text.ActKernelRanges"
//CHECK-NEXT: ELF.PutOpInSection %[[VAL8]] : !VPURegMapped.Index<0:0:0>
//CHECK-NEXT: ELF.PutOpInSection %[[VAL20]] : !VPURegMapped.Index<0:0:1>
//CHECK-NEXT: ELF.PutOpInSection %[[VAL32]] : !VPURegMapped.Index<0:0:2>

//CHECK-DAG: %[[ACTKERNELI:.*]] = ELF.CreateSection {{.*}} secName = ".text.ActKernelInvocations"
//CHECK-NEXT: ELF.PutOpInSection %[[VAL9]] : !VPURegMapped.Index<0:0:0>
//CHECK-NEXT: ELF.PutOpInSection %[[VAL21]] : !VPURegMapped.Index<0:0:1>
//CHECK-NEXT: ELF.PutOpInSection %[[VAL33]] : !VPURegMapped.Index<0:0:2>

//CHECK-DAG: %[[MAPPEDINF:.*]] = ELF.CreateSection {{.*}} secName = ".text.MappedInference"
//CHECK-NEXT: ELF.PutOpInSection %[[VAL52]] : !VPURegMapped.Index<0:0:0>

//CHECK-DAG: %[[INVARSEC:.*]] = ELF.CreateSection {{.*}} secName = ".text.DPUInvariants"

//CHECK-DAG: %[[VARSEC:.*]] = ELF.CreateSection {{.*}} secName = ".text.DPUVariants"

//CHECK-DAG: ELF.CreateMetadataSection {{.*}} secName = ".metadata"
//CHECK-NEXT: VPUMI37XX.NetworkMetadata

//CHECK: %[[BUILTIN_SYMTABSEC:.*]] = ELF.CreateSymbolTableSection secName("VPU_RT_SYMTAB")
//CHECK: %[[SYMTABSEC:.*]] = ELF.CreateSymbolTableSection secName(".symtab.tasks")

//CHECK: ELF.CreateRelocationSection secName(".rlt.text.dmaTasks0") sourceSymbolTableSection(%[[BUILTIN_SYMTABSEC]])
//CHECK-DAG: ELF.Reloc baseOp(%[[VAL4]] : !VPURegMapped.Index<0:0:0>) {{.*}} <R_VPU_64> {{.*}}
//CHECK-DAG: ELF.RelocImmOffset baseOp(%[[VAL4]] : !VPURegMapped.Index<0:0:0>) {{.*}} <R_VPU_32_RTM> {{.*}}
//CHECK-DAG: ELF.Reloc baseOp(%[[VAL11]] : !VPURegMapped.Index<0:0:1>) {{.*}} <R_VPU_64> {{.*}}
//CHECK-DAG: ELF.RelocImmOffset baseOp(%[[VAL11]] : !VPURegMapped.Index<0:0:1>) {{.*}} <R_VPU_32_RTM> {{.*}}
//CHECK-DAG: ELF.Reloc baseOp(%[[VAL16]] : !VPURegMapped.Index<0:0:2>) {{.*}} <R_VPU_64> {{.*}}
//CHECK-DAG: ELF.RelocImmOffset baseOp(%[[VAL16]] : !VPURegMapped.Index<0:0:2>) {{.*}} <R_VPU_32_RTM> {{.*}}
//CHECK-DAG: ELF.Reloc baseOp(%[[VAL23]] : !VPURegMapped.Index<0:0:3>) {{.*}} <R_VPU_64> {{.*}}
//CHECK-DAG: ELF.RelocImmOffset baseOp(%[[VAL23]] : !VPURegMapped.Index<0:0:3>) {{.*}} <R_VPU_32_RTM> {{.*}}
//CHECK-DAG: ELF.Reloc baseOp(%[[VAL28]] : !VPURegMapped.Index<0:0:4>) {{.*}} <R_VPU_64> {{.*}}
//CHECK-DAG: ELF.RelocImmOffset baseOp(%[[VAL28]] : !VPURegMapped.Index<0:0:4>) {{.*}} <R_VPU_32_RTM> {{.*}}
//CHECK-DAG: ELF.Reloc baseOp(%[[VAL35]] : !VPURegMapped.Index<0:0:5>) {{.*}} <R_VPU_64> {{.*}}

//CHECK: ELF.CreateRelocationSection secName(".rlt.text.KernelParams") sourceSymbolTableSection(%[[SYMTABSEC]])
//CHECK-DAG ELF.RelocImmOffset baseOp(%[[VAL10]] : !VPURegMapped.Index<0:0:0>) {{.*}} <R_VPU_32>
//CHECK-DAG ELF.RelocImmOffset baseOp(%[[VAL10]] : !VPURegMapped.Index<0:0:0>) {{.*}} <R_VPU_32>
//CHECK-DAG ELF.RelocImmOffset baseOp(%[[VAL10]] : !VPURegMapped.Index<0:0:0>) {{.*}} <R_VPU_32>
//CHECK-DAG ELF.RelocImmOffset baseOp(%[[VAL10]] : !VPURegMapped.Index<0:0:0>) {{.*}} <R_VPU_32>
//CHECK-DAG ELF.RelocImmOffset baseOp(%[[VAL22]] : !VPURegMapped.Index<0:0:1>) {{.*}} <R_VPU_32>
//CHECK-DAG ELF.RelocImmOffset baseOp(%[[VAL22]] : !VPURegMapped.Index<0:0:1>) {{.*}} <R_VPU_32>
//CHECK-DAG ELF.RelocImmOffset baseOp(%[[VAL22]] : !VPURegMapped.Index<0:0:1>) {{.*}} <R_VPU_32>
//CHECK-DAG ELF.RelocImmOffset baseOp(%[[VAL22]] : !VPURegMapped.Index<0:0:1>) {{.*}} <R_VPU_32>
//CHECK-DAG ELF.RelocImmOffset baseOp(%[[VAL34]] : !VPURegMapped.Index<0:0:2>) {{.*}} <R_VPU_32>
//CHECK-DAG ELF.RelocImmOffset baseOp(%[[VAL34]] : !VPURegMapped.Index<0:0:2>) {{.*}} <R_VPU_32>
//CHECK-DAG ELF.RelocImmOffset baseOp(%[[VAL34]] : !VPURegMapped.Index<0:0:2>) {{.*}} <R_VPU_32>
//CHECK-DAG ELF.RelocImmOffset baseOp(%[[VAL34]] : !VPURegMapped.Index<0:0:2>) {{.*}} <R_VPU_32>

//CHECK: ELF.CreateRelocationSection secName(".rlt.text.KernelParams") sourceSymbolTableSection(%[[BUILTIN_SYMTABSEC]])
//CHECK-DAG ELF.Reloc baseOp(%[[VAL10]] : !VPURegMapped.Index<0:0:0>) {{.*}} <R_VPU_32>
//CHECK-DAG ELF.Reloc baseOp(%[[VAL10]] : !VPURegMapped.Index<0:0:0>) {{.*}} <R_VPU_32>
//CHECK-DAG ELF.Reloc baseOp(%[[VAL22]] : !VPURegMapped.Index<0:0:1>) {{.*}} <R_VPU_32>
//CHECK-DAG ELF.Reloc baseOp(%[[VAL22]] : !VPURegMapped.Index<0:0:1>) {{.*}} <R_VPU_32>
//CHECK-DAG ELF.Reloc baseOp(%[[VAL34]] : !VPURegMapped.Index<0:0:2>) {{.*}} <R_VPU_32>
//CHECK-DAG ELF.Reloc baseOp(%[[VAL34]] : !VPURegMapped.Index<0:0:2>) {{.*}} <R_VPU_32>

//CHECK: ELF.CreateRelocationSection secName(".rlt.text.ActKernelRanges") sourceSymbolTableSection(%[[SYMTABSEC]])
//CHECK-DAG ELF.Reloc baseOp(%[[VAL8]] : !VPURegMapped.Index<0:0:0>) {{.*}} <R_VPU_32>
//CHECK-DAG ELF.Reloc baseOp(%[[VAL20]] : !VPURegMapped.Index<0:0:1>) {{.*}} <R_VPU_32>
//CHECK-DAG ELF.Reloc baseOp(%[[VAL32]] : !VPURegMapped.Index<0:0:2>) {{.*}} <R_VPU_32>

//CHECK: ELF.CreateRelocationSection secName(".rlt.text.ActKernelInvocations") sourceSymbolTableSection(%[[BUILTIN_SYMTABSEC]])
//CHECK-DAG ELF.Reloc baseOp(%[[VAL9]] : !VPURegMapped.Index<0:0:0>) {{.*}} <R_VPU_32_RTM>
//CHECK-DAG ELF.Reloc baseOp(%[[VAL21]] : !VPURegMapped.Index<0:0:1>) {{.*}} <R_VPU_32_RTM>
//CHECK-DAG ELF.Reloc baseOp(%[[VAL33]] : !VPURegMapped.Index<0:0:2>) {{.*}} <R_VPU_32_RTM>

//CHECK: ELF.CreateRelocationSection secName(".rlt.text.ActKernelInvocations") sourceSymbolTableSection(%[[SYMTABSEC]])
//CHECK-DAG ELF.RelocImmOffset baseOp(%[[VAL9]] : !VPURegMapped.Index<0:0:0>) {{.*}} <R_VPU_32>
//CHECK-DAG ELF.RelocImmOffset baseOp(%[[VAL9]] : !VPURegMapped.Index<0:0:0>) {{.*}} <R_VPU_32>
//CHECK-DAG ELF.RelocImmOffset baseOp(%[[VAL21]] : !VPURegMapped.Index<0:0:1>) {{.*}} <R_VPU_32>
//CHECK-DAG ELF.RelocImmOffset baseOp(%[[VAL21]] : !VPURegMapped.Index<0:0:1>) {{.*}} <R_VPU_32>
//CHECK-DAG ELF.RelocImmOffset baseOp(%[[VAL33]] : !VPURegMapped.Index<0:0:2>) {{.*}} <R_VPU_32>
//CHECK-DAG ELF.RelocImmOffset baseOp(%[[VAL33]] : !VPURegMapped.Index<0:0:2>) {{.*}} <R_VPU_32>

//CHECK: ELF.CreateRelocationSection secName(".rlt.text.MappedInference") sourceSymbolTableSection(%[[SYMTABSEC]])
//CHECK-DAG ELF.Reloc baseOp(%[[VAL12]] : !VPURegMapped.Index<0:0:0>) {{.*}} <R_VPU_64>
//CHECK-DAG ELF.Reloc baseOp(%[[VAL12]] : !VPURegMapped.Index<0:0:0>) {{.*}} <R_VPU_64>
//CHECK-DAG ELF.Reloc baseOp(%[[VAL12]] : !VPURegMapped.Index<0:0:0>) {{.*}} <R_VPU_64>
//CHECK-DAG ELF.Reloc baseOp(%[[VAL12]] : !VPURegMapped.Index<0:0:0>) {{.*}} <R_VPU_64>
