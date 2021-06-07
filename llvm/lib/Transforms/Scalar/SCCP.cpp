//===- SCCP.cpp - Sparse Conditional Constant Propagation -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements sparse conditional constant propagation and merging:
//
// Specifically, this:
//   * Assumes values are constant unless proven otherwise
//   * Assumes BasicBlocks are dead unless proven otherwise
//   * Proves values to be constant, and replaces them with constants
//   * Proves conditional branches to be unconditional
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Scalar/SCCP.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/PointerIntPair.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/ConstantFolding.h"
#include "llvm/Analysis/DomTreeUpdater.h"
#include "llvm/Analysis/GlobalsModRef.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/ValueLattice.h"
#include "llvm/Analysis/ValueLatticeUtils.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/PredicateInfo.h"
#include <cassert>
#include <utility>
#include <vector>

using namespace llvm;

#define DEBUG_TYPE "sccp"

STATISTIC(NumInstRemoved, "Number of instructions removed");
STATISTIC(NumDeadBlocks , "Number of basic blocks unreachable");
STATISTIC(NumInstReplaced,
          "Number of instructions replaced with (simpler) instruction");

STATISTIC(IPNumInstRemoved, "Number of instructions removed by IPSCCP");
STATISTIC(IPNumArgsElimed ,"Number of arguments constant propagated by IPSCCP");
STATISTIC(IPNumGlobalConst, "Number of globals found to be constant by IPSCCP");
STATISTIC(
    IPNumInstReplaced,
    "Number of instructions replaced with (simpler) instruction by IPSCCP");

// Helper to check if \p LV is either a constant or a constant
// range with a single element. This should cover exactly the same cases as the
// old ValueLatticeElement::isConstant() and is intended to be used in the
// transition to ValueLatticeElement.
static bool isConstant(const ValueLatticeElement &LV) {
  return LV.isConstant() ||
         (LV.isConstantRange() && LV.getConstantRange().isSingleElement());
}

// Helper to check if \p LV is either overdefined or a constant range with more
// than a single element. This should cover exactly the same cases as the old
// ValueLatticeElement::isOverdefined() and is intended to be used in the
// transition to ValueLatticeElement.
static bool isOverdefined(const ValueLatticeElement &LV) {
  return !LV.isUnknownOrUndef() && !isConstant(LV);
}

<<<<<<< HEAD
=======
//===----------------------------------------------------------------------===//
//
/// SCCPSolver - This class is a general purpose solver for Sparse Conditional
/// Constant Propagation.
///
class SCCPSolver : public InstVisitor<SCCPSolver> {
  const DataLayout &DL;
  std::function<const TargetLibraryInfo &(Function &)> GetTLI;
  SmallPtrSet<BasicBlock *, 8> BBExecutable; // The BBs that are executable.
  DenseMap<Value *, ValueLatticeElement>
      ValueState; // The state each value is in.

  /// StructValueState - This maintains ValueState for values that have
  /// StructType, for example for formal arguments, calls, insertelement, etc.
  DenseMap<std::pair<Value *, unsigned>, ValueLatticeElement> StructValueState;

  /// GlobalValue - If we are tracking any values for the contents of a global
  /// variable, we keep a mapping from the constant accessor to the element of
  /// the global, to the currently known value.  If the value becomes
  /// overdefined, it's entry is simply removed from this map.
  DenseMap<GlobalVariable *, ValueLatticeElement> TrackedGlobals;

  /// TrackedRetVals - If we are tracking arguments into and the return
  /// value out of a function, it will have an entry in this map, indicating
  /// what the known return value for the function is.
  MapVector<Function *, ValueLatticeElement> TrackedRetVals;

  /// TrackedMultipleRetVals - Same as TrackedRetVals, but used for functions
  /// that return multiple values.
  MapVector<std::pair<Function *, unsigned>, ValueLatticeElement>
      TrackedMultipleRetVals;

  /// MRVFunctionsTracked - Each function in TrackedMultipleRetVals is
  /// represented here for efficient lookup.
  SmallPtrSet<Function *, 16> MRVFunctionsTracked;

  /// MustTailFunctions - Each function here is a callee of non-removable
  /// musttail call site.
  SmallPtrSet<Function *, 16> MustTailCallees;

  /// TrackingIncomingArguments - This is the set of functions for whose
  /// arguments we make optimistic assumptions about and try to prove as
  /// constants.
  SmallPtrSet<Function *, 16> TrackingIncomingArguments;

  /// The reason for two worklists is that overdefined is the lowest state
  /// on the lattice, and moving things to overdefined as fast as possible
  /// makes SCCP converge much faster.
  ///
  /// By having a separate worklist, we accomplish this because everything
  /// possibly overdefined will become overdefined at the soonest possible
  /// point.
  SmallVector<Value *, 64> OverdefinedInstWorkList;
  SmallVector<Value *, 64> InstWorkList;

  // The BasicBlock work list
  SmallVector<BasicBlock *, 64>  BBWorkList;

  /// KnownFeasibleEdges - Entries in this set are edges which have already had
  /// PHI nodes retriggered.
  using Edge = std::pair<BasicBlock *, BasicBlock *>;
  DenseSet<Edge> KnownFeasibleEdges;

  DenseMap<Function *, AnalysisResultsForFn> AnalysisResults;
  DenseMap<Value *, SmallPtrSet<User *, 2>> AdditionalUsers;

  LLVMContext &Ctx;

public:
  void addAnalysis(Function &F, AnalysisResultsForFn A) {
    AnalysisResults.insert({&F, std::move(A)});
  }

  const PredicateBase *getPredicateInfoFor(Instruction *I) {
    auto A = AnalysisResults.find(I->getParent()->getParent());
    if (A == AnalysisResults.end())
      return nullptr;
    return A->second.PredInfo->getPredicateInfoFor(I);
  }

  DomTreeUpdater getDTU(Function &F) {
    auto A = AnalysisResults.find(&F);
    assert(A != AnalysisResults.end() && "Need analysis results for function.");
    return {A->second.DT, A->second.PDT, DomTreeUpdater::UpdateStrategy::Lazy};
  }

  SCCPSolver(const DataLayout &DL,
             std::function<const TargetLibraryInfo &(Function &)> GetTLI,
             LLVMContext &Ctx)
      : DL(DL), GetTLI(std::move(GetTLI)), Ctx(Ctx) {}

  /// MarkBlockExecutable - This method can be used by clients to mark all of
  /// the blocks that are known to be intrinsically live in the processed unit.
  ///
  /// This returns true if the block was not considered live before.
  bool MarkBlockExecutable(BasicBlock *BB) {
    if (!BBExecutable.insert(BB).second)
      return false;
    LLVM_DEBUG(dbgs() << "Marking Block Executable: " << BB->getName() << '\n');
    BBWorkList.push_back(BB);  // Add the block to the work list!
    return true;
  }

  /// TrackValueOfGlobalVariable - Clients can use this method to
  /// inform the SCCPSolver that it should track loads and stores to the
  /// specified global variable if it can.  This is only legal to call if
  /// performing Interprocedural SCCP.
  void TrackValueOfGlobalVariable(GlobalVariable *GV) {
    // We only track the contents of scalar globals.
    if (GV->getValueType()->isSingleValueType()) {
      ValueLatticeElement &IV = TrackedGlobals[GV];
      if (!isa<UndefValue>(GV->getInitializer()))
        IV.markConstant(GV->getInitializer());
    }
  }

  /// AddTrackedFunction - If the SCCP solver is supposed to track calls into
  /// and out of the specified function (which cannot have its address taken),
  /// this method must be called.
  void AddTrackedFunction(Function *F) {
    // Add an entry, F -> undef.
    if (auto *STy = dyn_cast<StructType>(F->getReturnType())) {
      MRVFunctionsTracked.insert(F);
      for (unsigned i = 0, e = STy->getNumElements(); i != e; ++i)
        TrackedMultipleRetVals.insert(
            std::make_pair(std::make_pair(F, i), ValueLatticeElement()));
    } else if (!F->getReturnType()->isVoidTy())
      TrackedRetVals.insert(std::make_pair(F, ValueLatticeElement()));
  }

  /// AddMustTailCallee - If the SCCP solver finds that this function is called
  /// from non-removable musttail call site.
  void AddMustTailCallee(Function *F) {
    MustTailCallees.insert(F);
  }

  /// Returns true if the given function is called from non-removable musttail
  /// call site.
  bool isMustTailCallee(Function *F) {
    return MustTailCallees.count(F);
  }

  void AddArgumentTrackedFunction(Function *F) {
    TrackingIncomingArguments.insert(F);
  }

  /// Returns true if the given function is in the solver's set of
  /// argument-tracked functions.
  bool isArgumentTrackedFunction(Function *F) {
    return TrackingIncomingArguments.count(F);
  }

  /// Solve - Solve for constants and executable blocks.
  void Solve();

  /// ResolvedUndefsIn - While solving the dataflow for a function, we assume
  /// that branches on undef values cannot reach any of their successors.
  /// However, this is not a safe assumption.  After we solve dataflow, this
  /// method should be use to handle this.  If this returns true, the solver
  /// should be rerun.
  bool ResolvedUndefsIn(Function &F);

  bool isBlockExecutable(BasicBlock *BB) const {
    return BBExecutable.count(BB);
  }

  // isEdgeFeasible - Return true if the control flow edge from the 'From' basic
  // block to the 'To' basic block is currently feasible.
  bool isEdgeFeasible(BasicBlock *From, BasicBlock *To) const;

  std::vector<ValueLatticeElement> getStructLatticeValueFor(Value *V) const {
    std::vector<ValueLatticeElement> StructValues;
    auto *STy = dyn_cast<StructType>(V->getType());
    assert(STy && "getStructLatticeValueFor() can be called only on structs");
    for (unsigned i = 0, e = STy->getNumElements(); i != e; ++i) {
      auto I = StructValueState.find(std::make_pair(V, i));
      assert(I != StructValueState.end() && "Value not in valuemap!");
      StructValues.push_back(I->second);
    }
    return StructValues;
  }

  void removeLatticeValueFor(Value *V) { ValueState.erase(V); }

  const ValueLatticeElement &getLatticeValueFor(Value *V) const {
    assert(!V->getType()->isStructTy() &&
           "Should use getStructLatticeValueFor");
    DenseMap<Value *, ValueLatticeElement>::const_iterator I =
        ValueState.find(V);
    assert(I != ValueState.end() &&
           "V not found in ValueState nor Paramstate map!");
    return I->second;
  }

  /// getTrackedRetVals - Get the inferred return value map.
  const MapVector<Function *, ValueLatticeElement> &getTrackedRetVals() {
    return TrackedRetVals;
  }

  /// getTrackedGlobals - Get and return the set of inferred initializers for
  /// global variables.
  const DenseMap<GlobalVariable *, ValueLatticeElement> &getTrackedGlobals() {
    return TrackedGlobals;
  }

  /// getMRVFunctionsTracked - Get the set of functions which return multiple
  /// values tracked by the pass.
  const SmallPtrSet<Function *, 16> getMRVFunctionsTracked() {
    return MRVFunctionsTracked;
  }

  /// getMustTailCallees - Get the set of functions which are called
  /// from non-removable musttail call sites.
  const SmallPtrSet<Function *, 16> getMustTailCallees() {
    return MustTailCallees;
  }

  /// markOverdefined - Mark the specified value overdefined.  This
  /// works with both scalars and structs.
  void markOverdefined(Value *V) {
    if (auto *STy = dyn_cast<StructType>(V->getType()))
      for (unsigned i = 0, e = STy->getNumElements(); i != e; ++i)
        markOverdefined(getStructValueState(V, i), V);
    else
      markOverdefined(ValueState[V], V);
  }

  // isStructLatticeConstant - Return true if all the lattice values
  // corresponding to elements of the structure are constants,
  // false otherwise.
  bool isStructLatticeConstant(Function *F, StructType *STy) {
    for (unsigned i = 0, e = STy->getNumElements(); i != e; ++i) {
      const auto &It = TrackedMultipleRetVals.find(std::make_pair(F, i));
      assert(It != TrackedMultipleRetVals.end());
      ValueLatticeElement LV = It->second;
      if (!isConstant(LV))
        return false;
    }
    return true;
  }

  /// Helper to return a Constant if \p LV is either a constant or a constant
  /// range with a single element.
  Constant *getConstant(const ValueLatticeElement &LV) const {
    if (LV.isConstant())
      return LV.getConstant();

    if (LV.isConstantRange()) {
      auto &CR = LV.getConstantRange();
      if (CR.getSingleElement())
        return ConstantInt::get(Ctx, *CR.getSingleElement());
    }
    return nullptr;
  }

private:
  ConstantInt *getConstantInt(const ValueLatticeElement &IV) const {
    return dyn_cast_or_null<ConstantInt>(getConstant(IV));
  }

  // pushToWorkList - Helper for markConstant/markOverdefined
  void pushToWorkList(ValueLatticeElement &IV, Value *V) {
    if (IV.isOverdefined())
      return OverdefinedInstWorkList.push_back(V);
    InstWorkList.push_back(V);
  }

  // Helper to push \p V to the worklist, after updating it to \p IV. Also
  // prints a debug message with the updated value.
  void pushToWorkListMsg(ValueLatticeElement &IV, Value *V) {
    LLVM_DEBUG(dbgs() << "updated " << IV << ": " << *V << '\n');
    pushToWorkList(IV, V);
  }

  // markConstant - Make a value be marked as "constant".  If the value
  // is not already a constant, add it to the instruction work list so that
  // the users of the instruction are updated later.
  bool markConstant(ValueLatticeElement &IV, Value *V, Constant *C,
                    bool MayIncludeUndef = false) {
    if (!IV.markConstant(C, MayIncludeUndef))
      return false;
    LLVM_DEBUG(dbgs() << "markConstant: " << *C << ": " << *V << '\n');
    pushToWorkList(IV, V);
    return true;
  }

  bool markConstant(Value *V, Constant *C) {
    assert(!V->getType()->isStructTy() && "structs should use mergeInValue");
    return markConstant(ValueState[V], V, C);
  }

  // markOverdefined - Make a value be marked as "overdefined". If the
  // value is not already overdefined, add it to the overdefined instruction
  // work list so that the users of the instruction are updated later.
  bool markOverdefined(ValueLatticeElement &IV, Value *V) {
    if (!IV.markOverdefined()) return false;

    LLVM_DEBUG(dbgs() << "markOverdefined: ";
               if (auto *F = dyn_cast<Function>(V)) dbgs()
               << "Function '" << F->getName() << "'\n";
               else dbgs() << *V << '\n');
    // Only instructions go on the work list
    pushToWorkList(IV, V);
    return true;
  }

  /// Merge \p MergeWithV into \p IV and push \p V to the worklist, if \p IV
  /// changes.
  bool mergeInValue(ValueLatticeElement &IV, Value *V,
                    ValueLatticeElement MergeWithV,
                    ValueLatticeElement::MergeOptions Opts = {
                        /*MayIncludeUndef=*/false, /*CheckWiden=*/false}) {
    if (IV.mergeIn(MergeWithV, Opts)) {
      pushToWorkList(IV, V);
      LLVM_DEBUG(dbgs() << "Merged " << MergeWithV << " into " << *V << " : "
                        << IV << "\n");
      return true;
    }
    return false;
  }

  bool mergeInValue(Value *V, ValueLatticeElement MergeWithV,
                    ValueLatticeElement::MergeOptions Opts = {
                        /*MayIncludeUndef=*/false, /*CheckWiden=*/false}) {
    assert(!V->getType()->isStructTy() &&
           "non-structs should use markConstant");
    return mergeInValue(ValueState[V], V, MergeWithV, Opts);
  }

  /// getValueState - Return the ValueLatticeElement object that corresponds to
  /// the value.  This function handles the case when the value hasn't been seen
  /// yet by properly seeding constants etc.
  ValueLatticeElement &getValueState(Value *V) {
    assert(!V->getType()->isStructTy() && "Should use getStructValueState");

    auto I = ValueState.insert(std::make_pair(V, ValueLatticeElement()));
    ValueLatticeElement &LV = I.first->second;

    if (!I.second)
      return LV;  // Common case, already in the map.

    if (auto *C = dyn_cast<Constant>(V))
      LV.markConstant(C);          // Constants are constant

    // All others are unknown by default.
    return LV;
  }

  /// getStructValueState - Return the ValueLatticeElement object that
  /// corresponds to the value/field pair.  This function handles the case when
  /// the value hasn't been seen yet by properly seeding constants etc.
  ValueLatticeElement &getStructValueState(Value *V, unsigned i) {
    assert(V->getType()->isStructTy() && "Should use getValueState");
    assert(i < cast<StructType>(V->getType())->getNumElements() &&
           "Invalid element #");

    auto I = StructValueState.insert(
        std::make_pair(std::make_pair(V, i), ValueLatticeElement()));
    ValueLatticeElement &LV = I.first->second;

    if (!I.second)
      return LV;  // Common case, already in the map.

    if (auto *C = dyn_cast<Constant>(V)) {
      Constant *Elt = C->getAggregateElement(i);

      if (!Elt)
        LV.markOverdefined();      // Unknown sort of constant.
      else if (isa<UndefValue>(Elt))
        ; // Undef values remain unknown.
      else
        LV.markConstant(Elt);      // Constants are constant.
    }

    // All others are underdefined by default.
    return LV;
  }

  /// markEdgeExecutable - Mark a basic block as executable, adding it to the BB
  /// work list if it is not already executable.
  bool markEdgeExecutable(BasicBlock *Source, BasicBlock *Dest) {
    if (!KnownFeasibleEdges.insert(Edge(Source, Dest)).second)
      return false;  // This edge is already known to be executable!

    if (!MarkBlockExecutable(Dest)) {
      // If the destination is already executable, we just made an *edge*
      // feasible that wasn't before.  Revisit the PHI nodes in the block
      // because they have potentially new operands.
      LLVM_DEBUG(dbgs() << "Marking Edge Executable: " << Source->getName()
                        << " -> " << Dest->getName() << '\n');

      for (PHINode &PN : Dest->phis())
        visitPHINode(PN);
    }
    return true;
  }

  // getFeasibleSuccessors - Return a vector of booleans to indicate which
  // successors are reachable from a given terminator instruction.
  void getFeasibleSuccessors(Instruction &TI, SmallVectorImpl<bool> &Succs);

  // OperandChangedState - This method is invoked on all of the users of an
  // instruction that was just changed state somehow.  Based on this
  // information, we need to update the specified user of this instruction.
  void OperandChangedState(Instruction *I) {
    if (BBExecutable.count(I->getParent()))   // Inst is executable?
      visit(*I);
  }

  // Add U as additional user of V.
  void addAdditionalUser(Value *V, User *U) {
    auto Iter = AdditionalUsers.insert({V, {}});
    Iter.first->second.insert(U);
  }

  // Mark I's users as changed, including AdditionalUsers.
  void markUsersAsChanged(Value *I) {
    // Functions include their arguments in the use-list. Changed function
    // values mean that the result of the function changed. We only need to
    // update the call sites with the new function result and do not have to
    // propagate the call arguments.
    if (isa<Function>(I)) {
      for (User *U : I->users()) {
        if (auto *CB = dyn_cast<CallBase>(U))
          handleCallResult(*CB);
      }
    } else {
      for (User *U : I->users())
        if (auto *UI = dyn_cast<Instruction>(U))
          OperandChangedState(UI);
    }

    auto Iter = AdditionalUsers.find(I);
    if (Iter != AdditionalUsers.end()) {
      // Copy additional users before notifying them of changes, because new
      // users may be added, potentially invalidating the iterator.
      SmallVector<Instruction *, 2> ToNotify;
      for (User *U : Iter->second)
        if (auto *UI = dyn_cast<Instruction>(U))
          ToNotify.push_back(UI);
      for (Instruction *UI : ToNotify)
        OperandChangedState(UI);
    }
  }
  void handleCallOverdefined(CallBase &CB);
  void handleCallResult(CallBase &CB);
  void handleCallArguments(CallBase &CB);

private:
  friend class InstVisitor<SCCPSolver>;

  // visit implementations - Something changed in this instruction.  Either an
  // operand made a transition, or the instruction is newly executable.  Change
  // the value type of I to reflect these changes if appropriate.
  void visitPHINode(PHINode &I);

  // Terminators

  void visitReturnInst(ReturnInst &I);
  void visitTerminator(Instruction &TI);

  void visitCastInst(CastInst &I);
  void visitSelectInst(SelectInst &I);
  void visitUnaryOperator(Instruction &I);
  void visitBinaryOperator(Instruction &I);
  void visitCmpInst(CmpInst &I);
  void visitExtractValueInst(ExtractValueInst &EVI);
  void visitInsertValueInst(InsertValueInst &IVI);

  void visitCatchSwitchInst(CatchSwitchInst &CPI) {
    markOverdefined(&CPI);
    visitTerminator(CPI);
  }

  // Instructions that cannot be folded away.

  void visitStoreInst     (StoreInst &I);
  void visitLoadInst      (LoadInst &I);
  void visitGetElementPtrInst(GetElementPtrInst &I);

  void visitCallInst      (CallInst &I) {
    visitCallBase(I);
  }

  void visitInvokeInst    (InvokeInst &II) {
    visitCallBase(II);
    visitTerminator(II);
  }

  void visitCallBrInst    (CallBrInst &CBI) {
    visitCallBase(CBI);
    visitTerminator(CBI);
  }

  void visitCallBase      (CallBase &CB);
  void visitResumeInst    (ResumeInst &I) { /*returns void*/ }
  void visitUnreachableInst(UnreachableInst &I) { /*returns void*/ }
  void visitFenceInst     (FenceInst &I) { /*returns void*/ }

  void visitInstruction(Instruction &I) {
    // All the instructions we don't do any special handling for just
    // go to overdefined.
    LLVM_DEBUG(dbgs() << "SCCP: Don't know how to handle: " << I << '\n');
    markOverdefined(&I);
  }
};

} // end anonymous namespace

// getFeasibleSuccessors - Return a vector of booleans to indicate which
// successors are reachable from a given terminator instruction.
void SCCPSolver::getFeasibleSuccessors(Instruction &TI,
                                       SmallVectorImpl<bool> &Succs) {
  Succs.resize(TI.getNumSuccessors());
  if (auto *BI = dyn_cast<BranchInst>(&TI)) {
    if (BI->isUnconditional()) {
      Succs[0] = true;
      return;
    }

    ValueLatticeElement BCValue = getValueState(BI->getCondition());
    ConstantInt *CI = getConstantInt(BCValue);
    if (!CI) {
      // Overdefined condition variables, and branches on unfoldable constant
      // conditions, mean the branch could go either way.
      if (!BCValue.isUnknownOrUndef())
        Succs[0] = Succs[1] = true;
      return;
    }

    // Constant condition variables mean the branch can only go a single way.
    Succs[CI->isZero()] = true;
    return;
  }

  // Unwinding instructions successors are always executable.
  if (TI.isExceptionalTerminator()) {
    Succs.assign(TI.getNumSuccessors(), true);
    return;
  }

  if (auto *SI = dyn_cast<SwitchInst>(&TI)) {
    if (!SI->getNumCases()) {
      Succs[0] = true;
      return;
    }
    const ValueLatticeElement &SCValue = getValueState(SI->getCondition());
    if (ConstantInt *CI = getConstantInt(SCValue)) {
      Succs[SI->findCaseValue(CI)->getSuccessorIndex()] = true;
      return;
    }

    // TODO: Switch on undef is UB. Stop passing false once the rest of LLVM
    // is ready.
    if (SCValue.isConstantRange(/*UndefAllowed=*/false)) {
      const ConstantRange &Range = SCValue.getConstantRange();
      for (const auto &Case : SI->cases()) {
        const APInt &CaseValue = Case.getCaseValue()->getValue();
        if (Range.contains(CaseValue))
          Succs[Case.getSuccessorIndex()] = true;
      }

      // TODO: Determine whether default case is reachable.
      Succs[SI->case_default()->getSuccessorIndex()] = true;
      return;
    }

    // Overdefined or unknown condition? All destinations are executable!
    if (!SCValue.isUnknownOrUndef())
      Succs.assign(TI.getNumSuccessors(), true);
    return;
  }

  // In case of indirect branch and its address is a blockaddress, we mark
  // the target as executable.
  if (auto *IBR = dyn_cast<IndirectBrInst>(&TI)) {
    // Casts are folded by visitCastInst.
    ValueLatticeElement IBRValue = getValueState(IBR->getAddress());
    BlockAddress *Addr = dyn_cast_or_null<BlockAddress>(getConstant(IBRValue));
    if (!Addr) {   // Overdefined or unknown condition?
      // All destinations are executable!
      if (!IBRValue.isUnknownOrUndef())
        Succs.assign(TI.getNumSuccessors(), true);
      return;
    }

    BasicBlock* T = Addr->getBasicBlock();
    assert(Addr->getFunction() == T->getParent() &&
           "Block address of a different function ?");
    for (unsigned i = 0; i < IBR->getNumSuccessors(); ++i) {
      // This is the target.
      if (IBR->getDestination(i) == T) {
        Succs[i] = true;
        return;
      }
    }

    // If we didn't find our destination in the IBR successor list, then we
    // have undefined behavior. Its ok to assume no successor is executable.
    return;
  }

  // In case of callbr, we pessimistically assume that all successors are
  // feasible.
  if (isa<CallBrInst>(&TI)) {
    Succs.assign(TI.getNumSuccessors(), true);
    return;
  }

  LLVM_DEBUG(dbgs() << "Unknown terminator instruction: " << TI << '\n');
  llvm_unreachable("SCCP: Don't know how to handle this terminator!");
}

// isEdgeFeasible - Return true if the control flow edge from the 'From' basic
// block to the 'To' basic block is currently feasible.
bool SCCPSolver::isEdgeFeasible(BasicBlock *From, BasicBlock *To) const {
  // Check if we've called markEdgeExecutable on the edge yet. (We could
  // be more aggressive and try to consider edges which haven't been marked
  // yet, but there isn't any need.)
  return KnownFeasibleEdges.count(Edge(From, To));
}

// visit Implementations - Something changed in this instruction, either an
// operand made a transition, or the instruction is newly executable.  Change
// the value type of I to reflect these changes if appropriate.  This method
// makes sure to do the following actions:
//
// 1. If a phi node merges two constants in, and has conflicting value coming
//    from different branches, or if the PHI node merges in an overdefined
//    value, then the PHI node becomes overdefined.
// 2. If a phi node merges only constants in, and they all agree on value, the
//    PHI node becomes a constant value equal to that.
// 3. If V <- x (op) y && isConstant(x) && isConstant(y) V = Constant
// 4. If V <- x (op) y && (isOverdefined(x) || isOverdefined(y)) V = Overdefined
// 5. If V <- MEM or V <- CALL or V <- (unknown) then V = Overdefined
// 6. If a conditional branch has a value that is constant, make the selected
//    destination executable
// 7. If a conditional branch has a value that is overdefined, make all
//    successors executable.
void SCCPSolver::visitPHINode(PHINode &PN) {
  // If this PN returns a struct, just mark the result overdefined.
  // TODO: We could do a lot better than this if code actually uses this.
  if (PN.getType()->isStructTy())
    return (void)markOverdefined(&PN);

  if (getValueState(&PN).isOverdefined())
    return; // Quick exit

  // Super-extra-high-degree PHI nodes are unlikely to ever be marked constant,
  // and slow us down a lot.  Just mark them overdefined.
  if (PN.getNumIncomingValues() > 64)
    return (void)markOverdefined(&PN);

  unsigned NumActiveIncoming = 0;

  // Look at all of the executable operands of the PHI node.  If any of them
  // are overdefined, the PHI becomes overdefined as well.  If they are all
  // constant, and they agree with each other, the PHI becomes the identical
  // constant.  If they are constant and don't agree, the PHI is a constant
  // range. If there are no executable operands, the PHI remains unknown.
  ValueLatticeElement PhiState = getValueState(&PN);
  for (unsigned i = 0, e = PN.getNumIncomingValues(); i != e; ++i) {
    if (!isEdgeFeasible(PN.getIncomingBlock(i), PN.getParent()))
      continue;

    ValueLatticeElement IV = getValueState(PN.getIncomingValue(i));
    PhiState.mergeIn(IV);
    NumActiveIncoming++;
    if (PhiState.isOverdefined())
      break;
  }

  // We allow up to 1 range extension per active incoming value and one
  // additional extension. Note that we manually adjust the number of range
  // extensions to match the number of active incoming values. This helps to
  // limit multiple extensions caused by the same incoming value, if other
  // incoming values are equal.
  mergeInValue(&PN, PhiState,
               ValueLatticeElement::MergeOptions().setMaxWidenSteps(
                   NumActiveIncoming + 1));
  ValueLatticeElement &PhiStateRef = getValueState(&PN);
  PhiStateRef.setNumRangeExtensions(
      std::max(NumActiveIncoming, PhiStateRef.getNumRangeExtensions()));
}

void SCCPSolver::visitReturnInst(ReturnInst &I) {
  if (I.getNumOperands() == 0) return;  // ret void
>>>>>>> 0826268d59c6e1bb3530dffd9dc5f6038774486d



static bool tryToReplaceWithConstant(SCCPSolver &Solver, Value *V) {
  Constant *Const = nullptr;
  if (V->getType()->isStructTy()) {
    std::vector<ValueLatticeElement> IVs = Solver.getStructLatticeValueFor(V);
    if (any_of(IVs,
               [](const ValueLatticeElement &LV) { return isOverdefined(LV); }))
      return false;
    std::vector<Constant *> ConstVals;
    auto *ST = cast<StructType>(V->getType());
    for (unsigned i = 0, e = ST->getNumElements(); i != e; ++i) {
      ValueLatticeElement V = IVs[i];
      ConstVals.push_back(isConstant(V)
                              ? Solver.getConstant(V)
                              : UndefValue::get(ST->getElementType(i)));
    }
    Const = ConstantStruct::get(ST, ConstVals);
  } else {
    const ValueLatticeElement &IV = Solver.getLatticeValueFor(V);
    if (isOverdefined(IV))
      return false;

    Const =
        isConstant(IV) ? Solver.getConstant(IV) : UndefValue::get(V->getType());
  }
  assert(Const && "Constant is nullptr here!");

  // Replacing `musttail` instructions with constant breaks `musttail` invariant
  // unless the call itself can be removed.
  // Calls with "clang.arc.attachedcall" implicitly use the return value and
  // those uses cannot be updated with a constant.
  CallBase *CB = dyn_cast<CallBase>(V);
  if (CB && ((CB->isMustTailCall() && !CB->isSafeToRemove()) ||
             CB->getOperandBundle(LLVMContext::OB_clang_arc_attachedcall))) {
    Function *F = CB->getCalledFunction();

    // Don't zap returns of the callee
    if (F)
      Solver.addToMustPreserveReturnsInFunctions(F);

    LLVM_DEBUG(dbgs() << "  Can\'t treat the result of call " << *CB
                      << " as a constant\n");
    return false;
  }

  LLVM_DEBUG(dbgs() << "  Constant: " << *Const << " = " << *V << '\n');

  // Replaces all of the uses of a variable with uses of the constant.
  V->replaceAllUsesWith(Const);
  return true;
}

static bool simplifyInstsInBlock(SCCPSolver &Solver, BasicBlock &BB,
                                 SmallPtrSetImpl<Value *> &InsertedValues,
                                 Statistic &InstRemovedStat,
                                 Statistic &InstReplacedStat) {
  bool MadeChanges = false;
  for (Instruction &Inst : make_early_inc_range(BB)) {
    if (Inst.getType()->isVoidTy())
      continue;
    if (tryToReplaceWithConstant(Solver, &Inst)) {
      if (Inst.isSafeToRemove())
        Inst.eraseFromParent();
      // Hey, we just changed something!
      MadeChanges = true;
      ++InstRemovedStat;
    } else if (isa<SExtInst>(&Inst)) {
      Value *ExtOp = Inst.getOperand(0);
      if (isa<Constant>(ExtOp) || InsertedValues.count(ExtOp))
        continue;
      const ValueLatticeElement &IV = Solver.getLatticeValueFor(ExtOp);
      if (!IV.isConstantRange(/*UndefAllowed=*/false))
        continue;
      if (IV.getConstantRange().isAllNonNegative()) {
        auto *ZExt = new ZExtInst(ExtOp, Inst.getType(), "", &Inst);
        InsertedValues.insert(ZExt);
        Inst.replaceAllUsesWith(ZExt);
        Solver.removeLatticeValueFor(&Inst);
        Inst.eraseFromParent();
        InstReplacedStat++;
        MadeChanges = true;
      }
    }
  }
  return MadeChanges;
}

// runSCCP() - Run the Sparse Conditional Constant Propagation algorithm,
// and return true if the function was modified.
static bool runSCCP(Function &F, const DataLayout &DL,
                    const TargetLibraryInfo *TLI) {
  LLVM_DEBUG(dbgs() << "SCCP on function '" << F.getName() << "'\n");
  SCCPSolver Solver(
      DL, [TLI](Function &F) -> const TargetLibraryInfo & { return *TLI; },
      F.getContext());

  // Mark the first block of the function as being executable.
  Solver.markBlockExecutable(&F.front());

  // Mark all arguments to the function as being overdefined.
  for (Argument &AI : F.args())
    Solver.markOverdefined(&AI);

  // Solve for constants.
  bool ResolvedUndefs = true;
  while (ResolvedUndefs) {
    Solver.solve();
    LLVM_DEBUG(dbgs() << "RESOLVING UNDEFs\n");
    ResolvedUndefs = Solver.resolvedUndefsIn(F);
  }

  bool MadeChanges = false;

  // If we decided that there are basic blocks that are dead in this function,
  // delete their contents now.  Note that we cannot actually delete the blocks,
  // as we cannot modify the CFG of the function.

  SmallPtrSet<Value *, 32> InsertedValues;
  for (BasicBlock &BB : F) {
    if (!Solver.isBlockExecutable(&BB)) {
      LLVM_DEBUG(dbgs() << "  BasicBlock Dead:" << BB);

      ++NumDeadBlocks;
      NumInstRemoved += removeAllNonTerminatorAndEHPadInstructions(&BB).first;

      MadeChanges = true;
      continue;
    }

    MadeChanges |= simplifyInstsInBlock(Solver, BB, InsertedValues,
                                        NumInstRemoved, NumInstReplaced);
  }

  return MadeChanges;
}

PreservedAnalyses SCCPPass::run(Function &F, FunctionAnalysisManager &AM) {
  const DataLayout &DL = F.getParent()->getDataLayout();
  auto &TLI = AM.getResult<TargetLibraryAnalysis>(F);
  if (!runSCCP(F, DL, &TLI))
    return PreservedAnalyses::all();

  auto PA = PreservedAnalyses();
  PA.preserveSet<CFGAnalyses>();
  return PA;
}

namespace {

//===--------------------------------------------------------------------===//
//
/// SCCP Class - This class uses the SCCPSolver to implement a per-function
/// Sparse Conditional Constant Propagator.
///
class SCCPLegacyPass : public FunctionPass {
public:
  // Pass identification, replacement for typeid
  static char ID;

  SCCPLegacyPass() : FunctionPass(ID) {
    initializeSCCPLegacyPassPass(*PassRegistry::getPassRegistry());
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<TargetLibraryInfoWrapperPass>();
    AU.addPreserved<GlobalsAAWrapperPass>();
    AU.setPreservesCFG();
  }

  // runOnFunction - Run the Sparse Conditional Constant Propagation
  // algorithm, and return true if the function was modified.
  bool runOnFunction(Function &F) override {
    if (skipFunction(F))
      return false;
    const DataLayout &DL = F.getParent()->getDataLayout();
    const TargetLibraryInfo *TLI =
        &getAnalysis<TargetLibraryInfoWrapperPass>().getTLI(F);
    return runSCCP(F, DL, TLI);
  }
};

} // end anonymous namespace

char SCCPLegacyPass::ID = 0;

INITIALIZE_PASS_BEGIN(SCCPLegacyPass, "sccp",
                      "Sparse Conditional Constant Propagation", false, false)
INITIALIZE_PASS_DEPENDENCY(TargetLibraryInfoWrapperPass)
INITIALIZE_PASS_END(SCCPLegacyPass, "sccp",
                    "Sparse Conditional Constant Propagation", false, false)

// createSCCPPass - This is the public interface to this file.
FunctionPass *llvm::createSCCPPass() { return new SCCPLegacyPass(); }

static void findReturnsToZap(Function &F,
                             SmallVector<ReturnInst *, 8> &ReturnsToZap,
                             SCCPSolver &Solver) {
  // We can only do this if we know that nothing else can call the function.
  if (!Solver.isArgumentTrackedFunction(&F))
    return;

  if (Solver.mustPreserveReturn(&F)) {
    LLVM_DEBUG(
        dbgs()
        << "Can't zap returns of the function : " << F.getName()
        << " due to present musttail or \"clang.arc.attachedcall\" call of "
           "it\n");
    return;
  }

  assert(
      all_of(F.users(),
             [&Solver](User *U) {
               if (isa<Instruction>(U) &&
                   !Solver.isBlockExecutable(cast<Instruction>(U)->getParent()))
                 return true;
               // Non-callsite uses are not impacted by zapping. Also, constant
               // uses (like blockaddresses) could stuck around, without being
               // used in the underlying IR, meaning we do not have lattice
               // values for them.
               if (!isa<CallBase>(U))
                 return true;
               if (U->getType()->isStructTy()) {
                 return all_of(Solver.getStructLatticeValueFor(U),
                               [](const ValueLatticeElement &LV) {
                                 return !isOverdefined(LV);
                               });
               }
               return !isOverdefined(Solver.getLatticeValueFor(U));
             }) &&
      "We can only zap functions where all live users have a concrete value");

  for (BasicBlock &BB : F) {
    if (CallInst *CI = BB.getTerminatingMustTailCall()) {
      LLVM_DEBUG(dbgs() << "Can't zap return of the block due to present "
                        << "musttail call : " << *CI << "\n");
      (void)CI;
      return;
    }

    if (auto *RI = dyn_cast<ReturnInst>(BB.getTerminator()))
      if (!isa<UndefValue>(RI->getOperand(0)))
        ReturnsToZap.push_back(RI);
  }
}

static bool removeNonFeasibleEdges(const SCCPSolver &Solver, BasicBlock *BB,
                                   DomTreeUpdater &DTU) {
  SmallPtrSet<BasicBlock *, 8> FeasibleSuccessors;
  bool HasNonFeasibleEdges = false;
  for (BasicBlock *Succ : successors(BB)) {
    if (Solver.isEdgeFeasible(BB, Succ))
      FeasibleSuccessors.insert(Succ);
    else
      HasNonFeasibleEdges = true;
  }

  // All edges feasible, nothing to do.
  if (!HasNonFeasibleEdges)
    return false;

  // SCCP can only determine non-feasible edges for br, switch and indirectbr.
  Instruction *TI = BB->getTerminator();
  assert((isa<BranchInst>(TI) || isa<SwitchInst>(TI) ||
          isa<IndirectBrInst>(TI)) &&
         "Terminator must be a br, switch or indirectbr");

  if (FeasibleSuccessors.size() == 1) {
    // Replace with an unconditional branch to the only feasible successor.
    BasicBlock *OnlyFeasibleSuccessor = *FeasibleSuccessors.begin();
    SmallVector<DominatorTree::UpdateType, 8> Updates;
    bool HaveSeenOnlyFeasibleSuccessor = false;
    for (BasicBlock *Succ : successors(BB)) {
      if (Succ == OnlyFeasibleSuccessor && !HaveSeenOnlyFeasibleSuccessor) {
        // Don't remove the edge to the only feasible successor the first time
        // we see it. We still do need to remove any multi-edges to it though.
        HaveSeenOnlyFeasibleSuccessor = true;
        continue;
      }

      Succ->removePredecessor(BB);
      Updates.push_back({DominatorTree::Delete, BB, Succ});
    }

    BranchInst::Create(OnlyFeasibleSuccessor, BB);
    TI->eraseFromParent();
    DTU.applyUpdatesPermissive(Updates);
  } else if (FeasibleSuccessors.size() > 1) {
    SwitchInstProfUpdateWrapper SI(*cast<SwitchInst>(TI));
    SmallVector<DominatorTree::UpdateType, 8> Updates;
    for (auto CI = SI->case_begin(); CI != SI->case_end();) {
      if (FeasibleSuccessors.contains(CI->getCaseSuccessor())) {
        ++CI;
        continue;
      }

      BasicBlock *Succ = CI->getCaseSuccessor();
      Succ->removePredecessor(BB);
      Updates.push_back({DominatorTree::Delete, BB, Succ});
      SI.removeCase(CI);
      // Don't increment CI, as we removed a case.
    }

    DTU.applyUpdatesPermissive(Updates);
  } else {
    llvm_unreachable("Must have at least one feasible successor");
  }
  return true;
}

bool llvm::runIPSCCP(
    Module &M, const DataLayout &DL,
    std::function<const TargetLibraryInfo &(Function &)> GetTLI,
    function_ref<AnalysisResultsForFn(Function &)> getAnalysis) {
  SCCPSolver Solver(DL, GetTLI, M.getContext());

  // Loop over all functions, marking arguments to those with their addresses
  // taken or that are external as overdefined.
  for (Function &F : M) {
    if (F.isDeclaration())
      continue;

    Solver.addAnalysis(F, getAnalysis(F));

    // Determine if we can track the function's return values. If so, add the
    // function to the solver's set of return-tracked functions.
    if (canTrackReturnsInterprocedurally(&F))
      Solver.addTrackedFunction(&F);

    // Determine if we can track the function's arguments. If so, add the
    // function to the solver's set of argument-tracked functions.
    if (canTrackArgumentsInterprocedurally(&F)) {
      Solver.addArgumentTrackedFunction(&F);
      continue;
    }

    // Assume the function is called.
    Solver.markBlockExecutable(&F.front());

    // Assume nothing about the incoming arguments.
    for (Argument &AI : F.args())
      Solver.markOverdefined(&AI);
  }

  // Determine if we can track any of the module's global variables. If so, add
  // the global variables we can track to the solver's set of tracked global
  // variables.
  for (GlobalVariable &G : M.globals()) {
    G.removeDeadConstantUsers();
    if (canTrackGlobalVariableInterprocedurally(&G))
      Solver.trackValueOfGlobalVariable(&G);
  }

  // Solve for constants.
  bool ResolvedUndefs = true;
  Solver.solve();
  while (ResolvedUndefs) {
    LLVM_DEBUG(dbgs() << "RESOLVING UNDEFS\n");
    ResolvedUndefs = false;
    for (Function &F : M) {
      if (Solver.resolvedUndefsIn(F))
        ResolvedUndefs = true;
    }
    if (ResolvedUndefs)
      Solver.solve();
  }

  bool MadeChanges = false;

  // Iterate over all of the instructions in the module, replacing them with
  // constants if we have found them to be of constant values.

  for (Function &F : M) {
    if (F.isDeclaration())
      continue;

    SmallVector<BasicBlock *, 512> BlocksToErase;

    if (Solver.isBlockExecutable(&F.front())) {
      bool ReplacedPointerArg = false;
      for (Argument &Arg : F.args()) {
        if (!Arg.use_empty() && tryToReplaceWithConstant(Solver, &Arg)) {
          ReplacedPointerArg |= Arg.getType()->isPointerTy();
          ++IPNumArgsElimed;
        }
      }

      // If we replaced an argument, the argmemonly and
      // inaccessiblemem_or_argmemonly attributes do not hold any longer. Remove
      // them from both the function and callsites.
      if (ReplacedPointerArg) {
        AttrBuilder AttributesToRemove;
        AttributesToRemove.addAttribute(Attribute::ArgMemOnly);
        AttributesToRemove.addAttribute(Attribute::InaccessibleMemOrArgMemOnly);
        F.removeAttributes(AttributeList::FunctionIndex, AttributesToRemove);

        for (User *U : F.users()) {
          auto *CB = dyn_cast<CallBase>(U);
          if (!CB || CB->getCalledFunction() != &F)
            continue;

          CB->removeAttributes(AttributeList::FunctionIndex,
                               AttributesToRemove);
        }
      }
    }

    SmallPtrSet<Value *, 32> InsertedValues;
    for (BasicBlock &BB : F) {
      if (!Solver.isBlockExecutable(&BB)) {
        LLVM_DEBUG(dbgs() << "  BasicBlock Dead:" << BB);
        ++NumDeadBlocks;

        MadeChanges = true;

        if (&BB != &F.front())
          BlocksToErase.push_back(&BB);
        continue;
      }

      MadeChanges |= simplifyInstsInBlock(Solver, BB, InsertedValues,
                                          IPNumInstRemoved, IPNumInstReplaced);
    }

    DomTreeUpdater DTU = Solver.getDTU(F);
    // Change dead blocks to unreachable. We do it after replacing constants
    // in all executable blocks, because changeToUnreachable may remove PHI
    // nodes in executable blocks we found values for. The function's entry
    // block is not part of BlocksToErase, so we have to handle it separately.
    for (BasicBlock *BB : BlocksToErase) {
      NumInstRemoved +=
          changeToUnreachable(BB->getFirstNonPHI(), /*UseLLVMTrap=*/false,
                              /*PreserveLCSSA=*/false, &DTU);
    }
    if (!Solver.isBlockExecutable(&F.front()))
      NumInstRemoved += changeToUnreachable(F.front().getFirstNonPHI(),
                                            /*UseLLVMTrap=*/false,
                                            /*PreserveLCSSA=*/false, &DTU);

    for (BasicBlock &BB : F)
      MadeChanges |= removeNonFeasibleEdges(Solver, &BB, DTU);

    for (BasicBlock *DeadBB : BlocksToErase)
      DTU.deleteBB(DeadBB);

    for (BasicBlock &BB : F) {
      for (BasicBlock::iterator BI = BB.begin(), E = BB.end(); BI != E;) {
        Instruction *Inst = &*BI++;
        if (Solver.getPredicateInfoFor(Inst)) {
          if (auto *II = dyn_cast<IntrinsicInst>(Inst)) {
            if (II->getIntrinsicID() == Intrinsic::ssa_copy) {
              Value *Op = II->getOperand(0);
              Inst->replaceAllUsesWith(Op);
              Inst->eraseFromParent();
            }
          }
        }
      }
    }
  }

  // If we inferred constant or undef return values for a function, we replaced
  // all call uses with the inferred value.  This means we don't need to bother
  // actually returning anything from the function.  Replace all return
  // instructions with return undef.
  //
  // Do this in two stages: first identify the functions we should process, then
  // actually zap their returns.  This is important because we can only do this
  // if the address of the function isn't taken.  In cases where a return is the
  // last use of a function, the order of processing functions would affect
  // whether other functions are optimizable.
  SmallVector<ReturnInst*, 8> ReturnsToZap;

  for (const auto &I : Solver.getTrackedRetVals()) {
    Function *F = I.first;
    const ValueLatticeElement &ReturnValue = I.second;

    // If there is a known constant range for the return value, add !range
    // metadata to the function's call sites.
    if (ReturnValue.isConstantRange() &&
        !ReturnValue.getConstantRange().isSingleElement()) {
      // Do not add range metadata if the return value may include undef.
      if (ReturnValue.isConstantRangeIncludingUndef())
        continue;

      auto &CR = ReturnValue.getConstantRange();
      for (User *User : F->users()) {
        auto *CB = dyn_cast<CallBase>(User);
        if (!CB || CB->getCalledFunction() != F)
          continue;

        // Limit to cases where the return value is guaranteed to be neither
        // poison nor undef. Poison will be outside any range and currently
        // values outside of the specified range cause immediate undefined
        // behavior.
        if (!isGuaranteedNotToBeUndefOrPoison(CB, nullptr, CB))
          continue;

        // Do not touch existing metadata for now.
        // TODO: We should be able to take the intersection of the existing
        // metadata and the inferred range.
        if (CB->getMetadata(LLVMContext::MD_range))
          continue;

        LLVMContext &Context = CB->getParent()->getContext();
        Metadata *RangeMD[] = {
            ConstantAsMetadata::get(ConstantInt::get(Context, CR.getLower())),
            ConstantAsMetadata::get(ConstantInt::get(Context, CR.getUpper()))};
        CB->setMetadata(LLVMContext::MD_range, MDNode::get(Context, RangeMD));
      }
      continue;
    }
    if (F->getReturnType()->isVoidTy())
      continue;
    if (isConstant(ReturnValue) || ReturnValue.isUnknownOrUndef())
      findReturnsToZap(*F, ReturnsToZap, Solver);
  }

  for (auto F : Solver.getMRVFunctionsTracked()) {
    assert(F->getReturnType()->isStructTy() &&
           "The return type should be a struct");
    StructType *STy = cast<StructType>(F->getReturnType());
    if (Solver.isStructLatticeConstant(F, STy))
      findReturnsToZap(*F, ReturnsToZap, Solver);
  }

  // Zap all returns which we've identified as zap to change.
  SmallSetVector<Function *, 8> FuncZappedReturn;
  for (unsigned i = 0, e = ReturnsToZap.size(); i != e; ++i) {
    Function *F = ReturnsToZap[i]->getParent()->getParent();
    ReturnsToZap[i]->setOperand(0, UndefValue::get(F->getReturnType()));
    // Record all functions that are zapped.
    FuncZappedReturn.insert(F);
  }

  // Remove the returned attribute for zapped functions and the
  // corresponding call sites.
  for (Function *F : FuncZappedReturn) {
    for (Argument &A : F->args())
      F->removeParamAttr(A.getArgNo(), Attribute::Returned);
    for (Use &U : F->uses()) {
      // Skip over blockaddr users.
      if (isa<BlockAddress>(U.getUser()))
        continue;
      CallBase *CB = cast<CallBase>(U.getUser());
      for (Use &Arg : CB->args())
        CB->removeParamAttr(CB->getArgOperandNo(&Arg), Attribute::Returned);
    }
  }

  // If we inferred constant or undef values for globals variables, we can
  // delete the global and any stores that remain to it.
  for (auto &I : make_early_inc_range(Solver.getTrackedGlobals())) {
    GlobalVariable *GV = I.first;
    if (isOverdefined(I.second))
      continue;
    LLVM_DEBUG(dbgs() << "Found that GV '" << GV->getName()
                      << "' is constant!\n");
    while (!GV->use_empty()) {
      StoreInst *SI = cast<StoreInst>(GV->user_back());
      SI->eraseFromParent();
      MadeChanges = true;
    }
    M.getGlobalList().erase(GV);
    ++IPNumGlobalConst;
  }

  return MadeChanges;
}
