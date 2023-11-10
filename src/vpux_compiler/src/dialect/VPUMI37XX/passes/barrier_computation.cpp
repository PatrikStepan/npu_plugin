//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include <mlir/Transforms/DialectConversion.h>
#include <vector>
#include "vpux/compiler/dialect/VPUIP/utils.hpp"
#include "vpux/compiler/dialect/VPUMI37XX/passes.hpp"
#include "vpux/compiler/dialect/VPUMI37XX/utils.hpp"

#include <vpu_nnrt_api_37xx.h>

using namespace vpux;

namespace {

struct VirtualDependencyTracker {
    using Range = std::pair<unsigned int, unsigned int>;

    struct Dependency {
        Range consumer_;
        Range producer_;
    };

    VirtualDependencyTracker(): ids_(), deps_(1) {
    }

    template <typename TaskOpType>
    unsigned int add(TaskOpType taskOp) {
        auto extract = [&](Range& range, ::mlir::ValueRange barriers) -> bool {
            range.first = ids_.size();

            if (barriers.empty()) {
                return true;
            }

            for (auto bv : barriers) {
                auto vv = mlir::dyn_cast<VPUMI37XX::ConfigureBarrierOp>(bv.getDefiningOp());
                VPUX_THROW_UNLESS(vv, "Encountered unexpected non barrier");

                auto v = vv.getType().getValue();

                VPUX_THROW_UNLESS(v <= checked_cast<uint32_t>(std::numeric_limits<short>::max()),
                                  "Too big virtual id {0}", v);
                ids_.push_back(static_cast<unsigned short>(v));
                ++range.second;
            }

            return true;
        };

        Dependency d{};

        if (!extract(d.consumer_, taskOp.getWaitBarriers()))
            return UINT_MAX;

        if (!extract(d.producer_, taskOp.getUpdateBarriers()))
            return UINT_MAX;

        if (d.consumer_.second || d.producer_.second) {
            deps_.push_back(d);
            return deps_.size() - 1;
        } else
            return 0;

        return 0;
    }

    unsigned short& id(unsigned int i) {
        return ids_[i];
    }
    Dependency& dep(unsigned int i) {
        return deps_[i];
    }

private:
    std::vector<unsigned short> ids_;
    std::vector<Dependency> deps_;
};

using TaskVector = std::vector<std::tuple<mlir::Operation*, nn_public::VpuTaskSchedulingBarrierConfig, unsigned int>>;

bool processSim(VirtualDependencyTracker& vdt_, const std::vector<nn_public::VpuBarrierCountConfig>& barriersConfig,
                std::vector<nn_public::VpuBarrierCountConfig>& counts, const VirtualDependencyTracker::Dependency& dep,
                nn_public::VpuTaskSchedulingBarrierConfig& bar_sched, unsigned short count,
                std::vector<short>& to_virtual) {
    auto barrierCheck = [&](bool dynamicCond) {
        return dynamicCond;
    };

    for (unsigned int i = 0; i < dep.consumer_.second; ++i) {
        unsigned v = vdt_.id(dep.consumer_.first + i);
        auto r = barriersConfig[v].real_id_;

        if (barrierCheck(to_virtual.size() <= r)) {
            return false;
        }

        if (counts[v].producer_count_ > 0) {
            return false;
        }
    }

    for (unsigned int i = 0; i < dep.producer_.second; ++i) {
        unsigned v = vdt_.id(dep.producer_.first + i);
        auto r = barriersConfig[v].real_id_;

        if (to_virtual.size() <= r || (to_virtual[r] != static_cast<short>(v))) {
            if (r < to_virtual.size()) {
            } else {
            }
            return false;
        }
    }

    bar_sched.start_after_ = 0;

    for (unsigned int i = 0; i < dep.consumer_.second; ++i) {
        unsigned v = vdt_.id(dep.consumer_.first + i);
        auto r = barriersConfig[v].real_id_;

        if (barrierCheck(r < to_virtual.size())) {
            // barrier not ready to be consumed
            if ((counts[v].producer_count_ != 0) || (counts[v].consumer_count_ < count)) {
                VPUX_THROW("v = {0} counts[v].producer_count_ = {1} counts[v].consumer_count_ = {2}", v,
                           counts[v].producer_count_, counts[v].consumer_count_);
            }

            counts[v].consumer_count_ -= count;
            bar_sched.start_after_ =
                    std::max<unsigned short>(bar_sched.start_after_, static_cast<unsigned short>(v + 1));

            if (counts[v].consumer_count_ == 0) {
                to_virtual[r] = -1;
            }
        } else {
            VPUX_THROW("r = {0} to_virtual.size() = {1}", static_cast<int>(r), to_virtual.size());
        }
    }

    bar_sched.clean_after_ = static_cast<unsigned short>(counts.size());

    for (unsigned int i = 0; i < dep.producer_.second; ++i) {
        unsigned v = vdt_.id(dep.producer_.first + i);
        auto r = barriersConfig[v].real_id_;

        if (barrierCheck(r < to_virtual.size())) {
            if (counts[v].producer_count_ < count) {
                VPUX_THROW("v = {0} counts[v].producer_count_ = {1}", v, counts[v].producer_count_);
            }
            counts[v].producer_count_ -= count;
            bar_sched.start_after_ =
                    std::max<unsigned short>(bar_sched.start_after_, static_cast<unsigned short>(v + 1));
            bar_sched.clean_after_ = std::min<unsigned short>(bar_sched.clean_after_, static_cast<unsigned short>(v));
        } else {
            VPUX_THROW("r = {0} to_virtual.size() = {1}", static_cast<int>(r), to_virtual.size());
        }
    }

    return true;
}

void simulateBarriers(const std::vector<nn_public::VpuBarrierCountConfig>& barriersConfigs, unsigned char nn_barriers_,
                      TaskVector& dmas0, TaskVector& dmas1, TaskVector& dpus, TaskVector& acts,
                      VirtualDependencyTracker& vdt_) {
    auto counts = barriersConfigs;
    std::vector<short> to_virtual(nn_barriers_, -1);

    auto dmaCurr0 = dmas0.begin();
    auto dmaCurr1 = dmas1.begin();
    auto dpuCurr = dpus.begin();
    auto actCurr = acts.begin();

    bool progressed = false;

    auto processTasks = [&](auto& currentIterator, auto endIterator) {
        for (; currentIterator != endIterator; ++currentIterator, progressed = true) {
            auto& current = *currentIterator;

            const auto& op = std::get<0>(current);
            auto& barrierConfig = std::get<1>(current);
            const auto dependencyIndex = std::get<2>(current);

            const auto barrierHitsCount =
                    mlir::dyn_cast<vpux::VPUMI37XX::ExecutableTaskOpInterface>(op).getBarrierHitsCount();
            if (!processSim(vdt_, barriersConfigs, counts, vdt_.dep(dependencyIndex), barrierConfig, barrierHitsCount,
                            to_virtual)) {
                break;
            }
        }
    };

    for (unsigned int bar = 0; bar < counts.size() || dmaCurr0 != dmas0.end() || dmaCurr1 != dmas1.end() ||
                               dpuCurr != dpus.end() || actCurr != acts.end();
         progressed = false) {
        // Static vs dynamic barriers need a different loop exit condition
        auto cond = [&]() {
            return to_virtual[barriersConfigs[bar].real_id_] == -1;
        };

        // map new barriers
        for (; bar < counts.size() && cond(); ++bar, progressed = true) {
            to_virtual[barriersConfigs[bar].real_id_] = static_cast<short>(bar);
        }

        processTasks(dmaCurr0, dmas0.end());
        processTasks(dmaCurr1, dmas1.end());
        processTasks(dpuCurr, dpus.end());
        processTasks(actCurr, acts.end());

        if (!progressed) {
            VPUX_THROW("Did not progress");
        }
    }
}

class BarrierComputationPass final : public VPUMI37XX::BarrierComputationBase<BarrierComputationPass> {
public:
    explicit BarrierComputationPass(Logger log) {
        Base::initLogger(log, Base::getArgumentName());
    }

private:
    void setNextSameID(mlir::MLIRContext* ctx, mlir::func::FuncOp funcOp) {
        auto MAX_PID = VPUIP::getNumAvailableBarriers(funcOp);
        std::vector<std::list<size_t>> nextSameID(MAX_PID);

        for (auto op : funcOp.getOps<VPUMI37XX::ConfigureBarrierOp>()) {
            auto opIndexType = op.getOperation()->getResult(0).getType().cast<VPURegMapped::IndexType>();

            auto vid = opIndexType.getValue();

            if (vid >= MAX_PID) {
                nextSameID[op.getId()].push_back(vid);
            }
        }

        auto copyOfNextSameID = nextSameID;
        for (auto op : funcOp.getOps<VPUMI37XX::ConfigureBarrierOp>()) {
            auto newNextSameID = -1;

            if (!copyOfNextSameID[op.getId()].empty()) {
                newNextSameID = copyOfNextSameID[op.getId()].front();
                copyOfNextSameID[op.getId()].pop_front();
            }

            auto newNextSameIDAttr =
                    mlir::IntegerAttr::get(mlir::IntegerType::get(ctx, 32, mlir::IntegerType::Signed), newNextSameID);

            op.setNextSameIdAttr(newNextSameIDAttr);
        }
    }

    template <typename TaskOpType, typename Condition = FuncRef<bool(TaskOpType)>>
    TaskVector buildTaskVector(
            mlir::func::FuncOp funcOp, VirtualDependencyTracker& tracker, Condition&& condition = [](TaskOpType) {
                return true;
            }) {
        TaskVector vector;
        for (auto op : funcOp.getOps<TaskOpType>()) {
            if (condition(op)) {
                vector.emplace_back(op, nn_public::VpuTaskSchedulingBarrierConfig{0, 0}, tracker.add(op));
            }
        }
        return vector;
    }

    void setBarrierAttributes(const TaskVector& tasks, mlir::MLIRContext* ctx) {
        for (auto task : tasks) {
            auto& op = std::get<0>(task);
            const auto& barrierConfig = std::get<1>(task);

            auto newStartAfterAttr = mlir::IntegerAttr::get(
                    mlir::IntegerType::get(ctx, 64, mlir::IntegerType::Unsigned), barrierConfig.start_after_);

            op->setAttr("start_after", newStartAfterAttr);

            auto newCleanAfterAttr = mlir::IntegerAttr::get(
                    mlir::IntegerType::get(ctx, 64, mlir::IntegerType::Unsigned), barrierConfig.clean_after_);

            op->setAttr("clean_after", newCleanAfterAttr);
        }
    }

    void safeRunOnFunc() final {
        auto funcOp = getOperation();
        mlir::MLIRContext* ctx = &(getContext());

        setNextSameID(ctx, funcOp);

        VirtualDependencyTracker vdt_;
        auto dmas0 = buildTaskVector<VPUMI37XX::NNDMAOp>(funcOp, vdt_, [](VPUMI37XX::NNDMAOp dma) {
            return dma.getPort() == 0;
        });
        auto dmas1 = buildTaskVector<VPUMI37XX::NNDMAOp>(funcOp, vdt_, [](VPUMI37XX::NNDMAOp dma) {
            return dma.getPort() == 1;
        });
        auto dpus = buildTaskVector<VPUMI37XX::DPUInvariantOp>(funcOp, vdt_);
        auto acts = buildTaskVector<VPUMI37XX::ActKernelInvocationOp>(funcOp, vdt_);

        std::vector<nn_public::VpuBarrierCountConfig> barriersConfigs;
        unsigned char nn_barriers_ = 0;
        for (auto op : funcOp.getOps<VPUMI37XX::ConfigureBarrierOp>()) {
            barriersConfigs.push_back(nn_public::VpuBarrierCountConfig{std::numeric_limits<uint16_t>::max(),
                                                                       op.getProducerCount().value(),
                                                                       op.getConsumerCount().value(), op.getId(), 0});
            nn_barriers_ = std::max<unsigned char>(nn_barriers_, op.getId() + 1);
        }

        simulateBarriers(barriersConfigs, nn_barriers_, dmas0, dmas1, dpus, acts, vdt_);

        setBarrierAttributes(dmas0, ctx);
        setBarrierAttributes(dmas1, ctx);
        setBarrierAttributes(dpus, ctx);
        setBarrierAttributes(acts, ctx);
    }
};

}  // namespace

//
// createBarrierComputationPass
//

std::unique_ptr<mlir::Pass> vpux::VPUMI37XX::createBarrierComputationPass(Logger log) {
    return std::make_unique<BarrierComputationPass>(log);
}
