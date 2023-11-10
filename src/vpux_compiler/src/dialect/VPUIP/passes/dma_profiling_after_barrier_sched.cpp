//
// Copyright (C) 2022-2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/core/aliases_info.hpp"
#include "vpux/compiler/core/profiling.hpp"
#include "vpux/compiler/dialect/IE/utils/resources.hpp"
#include "vpux/compiler/dialect/VPUIP/passes.hpp"
#include "vpux/compiler/dialect/VPUIP/utils.hpp"
#include "vpux/compiler/dialect/VPURT/attributes.hpp"
#include "vpux/compiler/dialect/VPURT/ops.hpp"
#include "vpux/compiler/dialect/VPURT/task.hpp"
#include "vpux/compiler/utils/attributes.hpp"
#include "vpux/compiler/utils/dma.hpp"
#include "vpux/compiler/utils/strings.hpp"
#include "vpux/utils/core/profiling.hpp"

using namespace vpux;

namespace {

const char* IS_OUT_OF_ORDER_ATTR_NAME = "is_out_of_order";

//
//  DMATaskProfilingAfterBarrierSchedPass
//

class DMATaskProfilingAfterBarrierSchedPass final :
        public VPUIP::DMATaskProfilingAfterBarrierSchedBase<DMATaskProfilingAfterBarrierSchedPass> {
public:
    explicit DMATaskProfilingAfterBarrierSchedPass(Logger log)
            : _timerType(), _Cmx0MemKind(), _timestampSize(), _profOutputId(), _hwAddr() {
        Base::initLogger(log, Base::getArgumentName());
    }

private:
    void safeRunOnModule() final;

    mlir::Operation* getDmaTask(VPURT::TaskOp taskOp);
    bool isDmaTask(VPURT::TaskOp taskOp);
    int64_t getDMAPortValue(VPURT::TaskOp taskOp);
    uint32_t getHwProfAddress(VPU::ArchKind arch);
    mlir::Type getTimestampType(mlir::MLIRContext* ctx, VPU::ArchKind arch);
    mlir::Location getProfBeginLoc(mlir::Location taskOpLoc);
    mlir::Location getProfEndLoc(mlir::Location taskOpLoc, unsigned dmaId);
    VPURT::TaskOp createProfTask(mlir::OpBuilder& builder, mlir::ValueRange updateBarriers,
                                 mlir::ValueRange waitBarriers, int64_t port, size_t address, mlir::Location loc,
                                 bool isOutOfOrder);

    void createCmx2DdrProfDma(mlir::OpBuilder& builder, mlir::ValueRange updateBarriers, int64_t port,
                              size_t srcCmxAddr, size_t dstDdrAddr, size_t sizeBytes);

    mlir::Type _timerType;
    IndexedSymbolAttr _Cmx0MemKind;
    int64_t _timestampSize;
    int64_t _profOutputId;
    uint32_t _hwAddr;
};

uint32_t DMATaskProfilingAfterBarrierSchedPass::getHwProfAddress(VPU::ArchKind arch) {
    switch (arch) {
    case VPU::ArchKind::VPUX30XX:
        return VPUIP::HW_TIMER_ABSOLUTE_ADDR_30XX;
    case VPU::ArchKind::VPUX37XX:
        return VPUIP::HW_TIMER_ABSOLUTE_ADDR_37XX;
    default:
        VPUX_THROW("Unsuported architecture");
    }
}

mlir::Type DMATaskProfilingAfterBarrierSchedPass::getTimestampType(mlir::MLIRContext* ctx, VPU::ArchKind arch) {
    switch (arch) {
    case VPU::ArchKind::VPUX30XX:
        return getUInt32Type(ctx);
    case VPU::ArchKind::VPUX37XX:
        return getUInt64Type(ctx);
    default:
        VPUX_THROW("Unsuported architecture");
    }
}

int64_t DMATaskProfilingAfterBarrierSchedPass::getDMAPortValue(VPURT::TaskOp taskOp) {
    auto* wrappedTaskOp = taskOp.getInnerTaskOp();

    VPUX_THROW_WHEN(mlir::isa<VPUIP::NCEClusterTilingOp>(wrappedTaskOp),
                    "NCEClusterTiling is not expected at this stage of compilation");

    return vpux::getDMAPortValue(wrappedTaskOp);
}

bool DMATaskProfilingAfterBarrierSchedPass::isDmaTask(VPURT::TaskOp taskOp) {
    auto* wrappedTaskOp = taskOp.getInnerTaskOp();

    VPUX_THROW_WHEN(mlir::isa<VPUIP::NCEClusterTilingOp>(wrappedTaskOp),
                    "NCEClusterTiling is not expected at this stage of compilation");

    return mlir::isa_and_nonnull<VPUIP::DMATypeOpInterface>(wrappedTaskOp);
}

mlir::Location DMATaskProfilingAfterBarrierSchedPass::getProfBeginLoc(mlir::Location taskOpLoc) {
    return appendLoc(taskOpLoc, PROFILING_DMA_TASK_BEGIN_PREFIX);
}

mlir::Location DMATaskProfilingAfterBarrierSchedPass::getProfEndLoc(mlir::Location taskOpLoc, unsigned dmaId) {
    return appendLoc(taskOpLoc, std::string(PROFILING_DMA_TASK_END_PREFIX) + '_' + std::to_string(dmaId));
}

VPURT::TaskOp DMATaskProfilingAfterBarrierSchedPass::createProfTask(mlir::OpBuilder& builder,
                                                                    mlir::ValueRange waitBarriers,
                                                                    mlir::ValueRange updateBarriers, int64_t port,
                                                                    size_t address, mlir::Location loc,
                                                                    bool isOutOfOrder) {
    _log.trace("createProfTask: port '{0}' cmxAddress '{1}' loc '{2}'", port, address, loc);

    // Create declaration for source buffer which corresponds to HW register with free-running counter
    auto hwRegType = getMemRefType(ShapeRef({1}), _timerType, DimsOrder::C, VPU::MemoryKind::Register);
    auto hwRegOp = builder.create<VPURT::DeclareBufferOp>(loc, hwRegType, VPURT::BufferSection::Register, _hwAddr);

    // Create declaration for destination buffer in CMX where timestamp is to be stored
    auto profBufType = getMemRefType(ShapeRef({1}), _timerType, DimsOrder::C, _Cmx0MemKind);
    auto profBufOp = builder.create<VPURT::DeclareBufferOp>(loc, profBufType, VPURT::BufferSection::CMX_NN, 0, address);

    // Insert profiling DMA wrapped in TaskOp with proper barriers settings
    const auto profDMA = VPURT::wrapIntoTaskOp<VPUIP::NNDMAOp>(
            builder, waitBarriers, updateBarriers, loc, hwRegOp.getBuffer(), profBufOp.getBuffer(), port, isOutOfOrder,
            /*is_critical=*/false, /*spillId=*/nullptr);

    return profDMA->getParentOfType<VPURT::TaskOp>();
}

void DMATaskProfilingAfterBarrierSchedPass::createCmx2DdrProfDma(mlir::OpBuilder& builder,
                                                                 mlir::ValueRange updateBarriers, int64_t port,
                                                                 size_t srcCmxAddr, size_t dstDdrAddr,
                                                                 size_t sizeBytes) {
    auto* ctx = builder.getContext();
    auto loc = mlir::NameLoc::get(mlir::StringAttr::get(
            ctx, mlir::StringRef("dma") + PROFILING_CMX_2_DDR_OP_NAME + std::to_string(dstDdrAddr)));

    _log.trace("Create CMX to DDR profiling buffer copy, port '{0}', cmxAddress '{1}' ddrAddress '{2}', "
               "size '{3}' loc '{4}'",
               port, srcCmxAddr, dstDdrAddr, sizeBytes, loc);

    auto numOfElements = static_cast<int64_t>(sizeBytes / _timestampSize);
    auto profilingBufferType = getMemRefType({numOfElements}, _timerType, DimsOrder::C, _Cmx0MemKind);

    // Create declaration for source CMX buffer
    auto srcBufProfCmxOp = builder.create<VPURT::DeclareBufferOp>(loc, profilingBufferType,
                                                                  VPURT::BufferSection::CMX_NN, 0, srcCmxAddr);
    // Create declaration for destination in profiling output
    auto profilingOutputType =
            mlir::MemRefType::get(profilingBufferType.getShape(), profilingBufferType.getElementType());
    auto dstBufProfResultOp = builder.create<VPURT::DeclareBufferOp>(
            loc, profilingOutputType, VPURT::BufferSection::ProfilingOutput, _profOutputId, dstDdrAddr);

    // Insert DMA wrapped in TaskOp with proper barriers settings
    VPURT::wrapIntoTaskOp<VPUIP::NNDMAOp>(builder, {}, updateBarriers, loc, srcBufProfCmxOp.getBuffer(),
                                          dstBufProfResultOp.getBuffer(), port);
}

bool isOutOfOrderSupported(mlir::Operation* dmaOp) {
    using namespace VPUIP;
    return mlir::isa_and_nonnull<NNDMAOp, PermuteDMAOp, ConvertDMAOp, DecompressDMAOp, CompressDMAOp, ExpandDMAOp>(
            dmaOp);
}

void DMATaskProfilingAfterBarrierSchedPass::safeRunOnModule() {
    auto module = getOperation();
    auto* ctx = module->getContext();
    const auto arch = VPU::getArch(module);
    const auto isOooOptimizationAppliable =
            (arch == VPU::ArchKind::VPUX37XX);  // For 37XX PROFBEGIN and profiled DMA may be proceeded by different
                                                // channels, which allow such DMAs issued  out of order
    IE::CNNNetworkOp netOp;
    mlir::func::FuncOp func;
    IE::CNNNetworkOp::getFromModule(module, netOp, func);
    mlir::OpBuilder builder(&func.getBody().front().front());

    auto dmaOp = IE::getAvailableExecutor(module, VPU::ExecutorKind::DMA_NN);
    auto dmaPortCount = dmaOp.count();

    int64_t dmaProfReservedMemSize = 0;
    int64_t dmaProfMemOffset = 0;
    if (auto dmaProfMem = IE::getDmaProfilingReservedMemory(module, VPU::MemoryKind::CMX_NN)) {
        dmaProfReservedMemSize = dmaProfMem.byteSize();
        VPUX_THROW_UNLESS(dmaProfMem.offset().has_value(), "No offset setting provided");
        dmaProfMemOffset = dmaProfMem.offset().value();
    }

    VPUX_THROW_UNLESS(dmaProfReservedMemSize > 0, "No reserved memory for DMA profiling");
    VPUX_THROW_UNLESS((dmaProfReservedMemSize % dmaPortCount) == 0,
                      "Reserved memory for DMA profiling ('{0}') cannot be equally split between ports",
                      dmaProfReservedMemSize);

    const int64_t dmaProfReservedMemSizePerPort = dmaProfReservedMemSize / dmaPortCount;

    // Traverse IR and store information about DMA tasks on each port
    SmallVector<SmallVector<VPURT::TaskOp>> dmaTasksPerPort(dmaPortCount);

    // Gather information about DMA tasks and their port so that total number is known
    // before inserting profiling tasks.
    func->walk([&](VPURT::TaskOp taskOp) {
        if (!isDmaTask(taskOp)) {
            return;
        }

        auto taskName = stringifyLocation(taskOp->getLoc());
        // Skip DMAs which are used for handling profiling. Such DMAs will not be measured.
        if (taskName.find(PROFILING_CMX_2_DDR_OP_NAME) != std::string::npos) {
            return;
        }

        auto portNumber = getDMAPortValue(taskOp);
        dmaTasksPerPort[portNumber].push_back(taskOp);
    });

    int64_t totalNumberOfDmas = 0;
    int64_t ddrProfilingBufferOffset = 0;
    SmallVector<int64_t> cmxProfilingBaseAddress(dmaPortCount);
    SmallVector<int64_t> cmxProfilingBufferOffset(dmaPortCount);

    for (int64_t port = 0; port < dmaPortCount; port++) {
        cmxProfilingBaseAddress[port] = dmaProfMemOffset + port * dmaProfReservedMemSizePerPort;
        totalNumberOfDmas += dmaTasksPerPort[port].size();
    }
    if (totalNumberOfDmas == 0) {
        return;
    }

    // Initialize some common variables which are used when inserting profiling DMAs
    _timerType = getTimestampType(ctx, arch);
    _timestampSize = _timerType.getIntOrFloatBitWidth() / 8;
    _hwAddr = getHwProfAddress(arch);
    _profOutputId = static_cast<int64_t>(netOp.getProfilingOutputsCount());

    // DMA profiling data is stored in CMX slice 0
    _Cmx0MemKind = IndexedSymbolAttr::get(ctx, stringifyEnum(VPU::MemoryKind::CMX_NN), 0);

    const auto outputResult = mlir::MemRefType::get({2 * totalNumberOfDmas}, _timerType);

    // Update network output information to have also new dma profiling result
    auto profilingResult = addNewProfilingOutput(ctx, func, netOp, outputResult, profiling::ExecutorType::DMA_SW);
    auto returnOp = mlir::dyn_cast_or_null<mlir::func::ReturnOp>(func.getBody().front().getTerminator());
    VPUX_THROW_UNLESS(returnOp != nullptr, "No ReturnOp was found");
    builder.setInsertionPoint(returnOp);
    returnOp.operandsMutable().append(profilingResult);

    // Go through DMA tasks on each port and insert profiling tasks
    // Example:
    // Before pass:
    //  <waitBarrier> -> DMA0 -> <updateBarrier>
    // After pass:
    //  <waitBarrier> -> Prof -> DMA0 -> Prof -> <updateBarrier>
    //
    // When profiling buffer is full or there are no more tasks to profile then
    // DMA that copies profiling buffer to DDR is inserted right after last profiling DMA

    unsigned dmaId = 0;
    for (int portNumber = 0; portNumber < dmaPortCount; portNumber++) {
        _log.trace("Handle DMA profiling on port '{0}', number of tasks to profile: '{1}'", portNumber,
                   dmaTasksPerPort[portNumber].size());
        for (auto i : irange(dmaTasksPerPort[portNumber].size())) {
            auto& taskOp = dmaTasksPerPort[portNumber][i];
            auto innerDmaOp = taskOp.getInnerTaskOp();
            if (isOooOptimizationAppliable && isOutOfOrderSupported(innerDmaOp)) {
                // To reduce profiling overhead, DMA may be issued out of order
                innerDmaOp->setAttr(IS_OUT_OF_ORDER_ATTR_NAME, mlir::UnitAttr::get(ctx));
            }

            _log.trace("DMA task to profile: '{0}'", taskOp->getLoc());
            _log = _log.nest();

            auto waitBarriers = taskOp.getWaitBarriers();
            auto updateBarriers = taskOp.getUpdateBarriers();

            // First check if after this task CMX2DDR profiling buffer copy will be needed
            // due to buffer geting full after this task
            auto insertProfCmx2DdrFlag =
                    (dmaProfReservedMemSizePerPort - cmxProfilingBufferOffset[portNumber] == 2 * _timestampSize);
            // Also if this is last DMA to profile on this port then such copy should be done too
            insertProfCmx2DdrFlag = (insertProfCmx2DdrFlag || (i == (dmaTasksPerPort[portNumber].size() - 1)));

            // Handle start time profiling task
            builder.setInsertionPoint(taskOp);
            auto cmxAddress = cmxProfilingBaseAddress[portNumber] + cmxProfilingBufferOffset[portNumber];

            _log.trace("Create task start profiling DMA");
            _log = _log.nest();
            auto profTaskLoc = getProfBeginLoc(taskOp->getLoc());
            createProfTask(builder, waitBarriers, {}, portNumber, cmxAddress, profTaskLoc, isOooOptimizationAppliable);
            _log = _log.unnest();

            cmxProfilingBufferOffset[portNumber] += _timestampSize;

            // Handle end time profiling task
            builder.setInsertionPointAfter(taskOp);
            cmxAddress = cmxProfilingBaseAddress[portNumber] + cmxProfilingBufferOffset[portNumber];

            _log.trace("Create task end profiling DMA");
            _log = _log.nest();
            profTaskLoc = getProfEndLoc(taskOp->getLoc(), dmaId);
            if (insertProfCmx2DdrFlag) {
                // In case after this task DMA to DDR with profiling buffer is inserted then
                // updateBarriers settings will be pushed to it and not needed here
                createProfTask(builder, {}, {}, portNumber, cmxAddress, profTaskLoc, /*outOfOrder=*/false);
            } else {
                createProfTask(builder, {}, updateBarriers, portNumber, cmxAddress, profTaskLoc, /*outOfOrder=*/false);
            }
            dmaId++;
            _log = _log.unnest();
            cmxProfilingBufferOffset[portNumber] += _timestampSize;

            VPUX_THROW_WHEN(cmxProfilingBufferOffset[portNumber] > dmaProfReservedMemSizePerPort,
                            "Profiling buffer beyond allowed space");

            if (insertProfCmx2DdrFlag) {
                // Once profiling buffer instance is full or there are no more tasks to profile
                // insert DMA which copies buffer to ProfilingOutput at a proper offset
                auto size = cmxProfilingBufferOffset[portNumber];

                createCmx2DdrProfDma(builder, updateBarriers, portNumber, cmxProfilingBaseAddress[portNumber],
                                     ddrProfilingBufferOffset, size);

                ddrProfilingBufferOffset += size;
                cmxProfilingBufferOffset[portNumber] = 0;
            }

            // Remove barriers from profiled task as its execution will be guarded by
            // newly inserted profiling DMAs before and after the task
            taskOp.getWaitBarriersMutable().clear();
            taskOp.getUpdateBarriersMutable().clear();

            _log = _log.unnest();
        }
    }
}

}  // namespace

//
// createDMATaskProfilingAfterBarrierSchedPass
//

std::unique_ptr<mlir::Pass> vpux::VPUIP::createDMATaskProfilingAfterBarrierSchedPass(Logger log) {
    return std::make_unique<DMATaskProfilingAfterBarrierSchedPass>(log);
}
