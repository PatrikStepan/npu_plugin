//
// Copyright (C) 2022-2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

// RUN: vpux-opt --mlir-print-debuginfo --init-compiler="vpu-arch=VPUX37XX allow-custom-values=true" --lower-VPUIP-to-ELF %data_path_37XX%/profiling.mlir.txt | vpux-translate --vpu-arch=VPUX37XX --export-ELF -o %t
// RUN: prof_parser -b %t -p %data_path_37XX%/profiling-0-37XX.bin -f json | FileCheck %s

// CHECK: {"traceEvents":[
// CHECK: {"name": "process_name", "ph": "M", "pid":0, "args": {"name" : "DMA"}},
// CHECK: {"name": "process_sort_index", "ph": "M", "pid":0, "args": {"sort_index" : "0"}},
// CHECK: {"name": "thread_name", "ph": "M", "pid":0, "tid":0, "args": {"name" : "DMA"}},
// CHECK: {"name": "thread_name", "ph": "M", "pid":0, "tid":1, "args": {"name" : "DMA"}},
// CHECK: {"name": "process_name", "ph": "M", "pid":1, "args": {"name" : "Cluster (0)"}},
// CHECK: {"name": "process_sort_index", "ph": "M", "pid":1, "args": {"sort_index" : "1"}},
// CHECK: {"name": "thread_name", "ph": "M", "pid":1, "tid":0, "args": {"name" : "DPU"}},
// CHECK: {"name": "thread_name", "ph": "M", "pid":1, "tid":1, "args": {"name" : "SW / Shave"}},
// CHECK: {"name": "thread_name", "ph": "M", "pid":1, "tid":2, "args": {"name" : "SW / Shave"}},
// CHECK: {"name": "process_name", "ph": "M", "pid":2, "args": {"name" : "Cluster (1)"}},
// CHECK: {"name": "process_sort_index", "ph": "M", "pid":2, "args": {"sort_index" : "2"}},
// CHECK: {"name": "thread_name", "ph": "M", "pid":2, "tid":0, "args": {"name" : "DPU"}},
// CHECK: {"name": "thread_name", "ph": "M", "pid":2, "tid":1, "args": {"name" : "SW / Shave"}},
// CHECK: {"name": "thread_name", "ph": "M", "pid":2, "tid":2, "args": {"name" : "SW / Shave"}},
// CHECK: {"name": "process_name", "ph": "M", "pid":3, "args": {"name" : "Layers"}},
// CHECK: {"name": "process_sort_index", "ph": "M", "pid":3, "args": {"sort_index" : "3"}},
// CHECK: {"name": "thread_name", "ph": "M", "pid":3, "tid":0, "args": {"name" : "Layers"}},
// CHECK: {"name": "thread_name", "ph": "M", "pid":3, "tid":1, "args": {"name" : "Layers"}},
// CHECK: {"name":"conv1/WithoutBiases?t_Convolution", "cat":"DMA", "ph":"X", "ts":0.000, "dur":2.005, "pid":0, "tid":0},
// CHECK: {"name":"conv1/WithoutBiases?t_Convolution/_expand_copy_3_2", "cat":"DMA", "ph":"X", "ts":4.193, "dur":1.562, "pid":0, "tid":0},
// CHECK: {"name":"conv1/WithoutBiases?t_Convolution", "cat":"DMA", "ph":"X", "ts":24.037, "dur":1.171, "pid":0, "tid":0},
// CHECK: {"name":"conv1/WithoutBiases?t_Convolution", "cat":"DMA", "ph":"X", "ts":25.443, "dur":1.015, "pid":0, "tid":0},
// CHECK: {"name":"conv1/WithoutBiases?t_Convolution", "cat":"DMA", "ph":"X", "ts":25.599, "dur":1.015, "pid":0, "tid":1},
// CHECK: {"name":"conv1/WithoutBiases?t_Convolution/output tile [0, 0, 0, 0]/_fused_constant/_fused_tile/_broadcast_copy_to_CMX[0,1]", "cat":"DMA", "ph":"X", "ts":26.771, "dur":0.651, "pid":0, "tid":0},
// CHECK: {"name":"conv1/WithoutBiases?t_Convolution", "cat":"DMA", "ph":"X", "ts":27.839, "dur":2.890, "pid":0, "tid":0},
// CHECK: {"name":"conv1/WithoutBiases?t_Convolution", "cat":"DMA", "ph":"X", "ts":27.995, "dur":2.578, "pid":0, "tid":1},
// CHECK: {"name":"conv1/WithoutBiases?t_Convolution/output tile [0, 0, 0, 0]", "cat":"DMA", "ph":"X", "ts":30.964, "dur":2.630, "pid":0, "tid":0},
// CHECK: {"name":"conv1/WithoutBiases?t_Convolution/output tile [0, 0, 0, 0]", "cat":"DMA", "ph":"X", "ts":31.120, "dur":2.630, "pid":0, "tid":1},
// CHECK: {"name":"pool1?t_MaxPool/_fused_constant/_fused_tile/_broadcast_copy_to_CMX[0,1]", "cat":"DMA", "ph":"X", "ts":33.907, "dur":0.755, "pid":0, "tid":0},
// CHECK: {"name":"pool1?t_MaxPool", "cat":"DMA", "ph":"X", "ts":53.073, "dur":2.213, "pid":0, "tid":0},
// CHECK: {"name":"pool1?t_MaxPool", "cat":"DMA", "ph":"X", "ts":53.230, "dur":2.213, "pid":0, "tid":1},
// CHECK: {"name":"pool1?t_MaxPool", "cat":"DMA", "ph":"X", "ts":55.599, "dur":2.005, "pid":0, "tid":0},
// CHECK: {"name":"pool1?t_MaxPool", "cat":"DMA", "ph":"X", "ts":55.756, "dur":2.005, "pid":0, "tid":1},
// CHECK: {"name":"pool1?t_MaxPool", "cat":"DMA", "ph":"X", "ts":57.995, "dur":1.432, "pid":0, "tid":0},
// CHECK: {"name":"pool1?t_MaxPool", "cat":"DMA", "ph":"X", "ts":58.151, "dur":1.432, "pid":0, "tid":1},
// CHECK: {"name":"pool1?t_MaxPool", "cat":"DMA", "ph":"X", "ts":59.740, "dur":1.562, "pid":0, "tid":0},
// CHECK: {"name":"pool1?t_MaxPool", "cat":"DMA", "ph":"X", "ts":59.896, "dur":1.562, "pid":0, "tid":1},
// CHECK: {"name":"output", "cat":"DMA", "ph":"X", "ts":70.677, "dur":2.500, "pid":0, "tid":0},
// CHECK: {"name":"output", "cat":"DMA", "ph":"X", "ts":70.834, "dur":2.187, "pid":0, "tid":1},
// CHECK: {"name":"output", "cat":"DMA", "ph":"X", "ts":73.334, "dur":1.718, "pid":0, "tid":0},
// CHECK: {"name":"output", "cat":"DMA", "ph":"X", "ts":73.542, "dur":1.979, "pid":0, "tid":1},
// CHECK: {"name":"conv1/WithoutBiases?t_Convolution/cluster_0", "cat":"DPU", "ph":"X", "ts":26.617, "dur":0.666, "pid":1, "tid":0},
// CHECK: {"name":"conv1/WithoutBiases?t_Convolution/output tile [0, 0, 0, 0]/cluster_0", "cat":"DPU", "ph":"X", "ts":33.799, "dur":6.749, "pid":1, "tid":0},
// CHECK: {"name":"pool1?t_MaxPool/cluster_0", "cat":"DPU", "ph":"X", "ts":41.825, "dur":10.695, "pid":1, "tid":0},
// CHECK: {"name":"conv1/WithoutBiases?t_Convolution/cluster_0/tile_0", "cat":"SW", "ph":"X", "ts":8.750, "dur":14.765, "pid":1, "tid":1},
// CHECK: {"name":"conv1/WithoutBiases?t_Convolution/cluster_0/tile_1", "cat":"SW", "ph":"X", "ts":8.881, "dur":12.994, "pid":1, "tid":2},
// CHECK: {"name":"output/cluster_0/tile_0", "cat":"SW", "ph":"X", "ts":61.980, "dur":8.046, "pid":1, "tid":1},
// CHECK: {"name":"output/cluster_0/tile_1", "cat":"SW", "ph":"X", "ts":62.110, "dur":7.395, "pid":1, "tid":2},
// CHECK: {"name":"conv1/WithoutBiases?t_Convolution/cluster_1", "cat":"DPU", "ph":"X", "ts":26.708, "dur":0.671, "pid":2, "tid":0},
// CHECK: {"name":"conv1/WithoutBiases?t_Convolution/output tile [0, 0, 0, 0]/cluster_1", "cat":"DPU", "ph":"X", "ts":33.750, "dur":7.792, "pid":2, "tid":0},
// CHECK: {"name":"pool1?t_MaxPool/cluster_1", "cat":"DPU", "ph":"X", "ts":42.244, "dur":9.572, "pid":2, "tid":0},
// CHECK: {"name":"output/cluster_1/tile_1", "cat":"SW", "ph":"X", "ts":61.849, "dur":7.786, "pid":2, "tid":1},
// CHECK: {"name":"output/cluster_1/tile_0", "cat":"SW", "ph":"X", "ts":62.240, "dur":7.916, "pid":2, "tid":2},
// CHECK: {"name":"conv1/WithoutBiases", "cat":"Layer", "ph":"X", "ts":0.000, "dur":41.542, "pid":3, "tid":0, "args":{"Layer type": "Convolution"}},
// CHECK: {"name":"pool1", "cat":"Layer", "ph":"X", "ts":33.907, "dur":27.551, "pid":3, "tid":1, "args":{"Layer type": "MaxPool"}},
// CHECK: {"name":"output", "cat":"Layer", "ph":"X", "ts":61.849, "dur":13.672, "pid":3, "tid":0, "args":{"Layer type": ""}}
// CHECK: ],
// CHECK: "displayTimeUnit": "ns"
// CHECK: }
