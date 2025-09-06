#include "llvm/Transforms/Scalar/LoopUnrollPass.h"
#include "llvm/IR/Argument.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Analysis/CodeMetrics.h"
#include "llvm/Analysis/LazyBlockFrequencyInfo.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/LoopUnrollAnalyzer.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/ProfileSummaryInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"
#include "llvm/Transforms/Preload/Preload.h"
#include "llvm/Transforms/Utils.h"
#include "llvm/Transforms/Utils/LoopPeel.h"
#include "llvm/Transforms/Utils/LoopSimplify.h"
#include "llvm/Transforms/Utils/FixIrreducible.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Transforms/Utils/SizeOpts.h"
#include "llvm/Transforms/Utils/UnrollLoop.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Attributes.inc"
#include <iostream>
#include <bits/stdc++.h>
#include "llvm/IR/DataLayout.h"
#include "llvm/Transforms/Scalar/LoopSimplifyCFG.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/Analysis/DomTreeUpdater.h"
#include "llvm/Analysis/GlobalsModRef.h"
#include "llvm/Analysis/LoopIterator.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/MemorySSAUpdater.h"
#include "llvm/Analysis/ScalarEvolutionAliasAnalysis.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm-c/lto.h"
#include "llvm/Analysis/LoopInfo.h"
using namespace llvm;


llvm::DenseMap<llvm::Function*, llvm::Constant*> NameConstByEntry;

static Function *stripToFunc(Value *V) {
  if (auto *CE = dyn_cast<ConstantExpr>(V))
    if (CE->isCast()) V = CE->getOperand(0);
  return dyn_cast<Function>(V->stripPointerCasts());
}

static void dumpUsers(Value *V) {
  errs() << "Users of: " << *V << "\n";
  for (Use &UR : V->uses()) {
    User *U = UR.getUser();
    if (auto *I = dyn_cast<Instruction>(U))
      errs() << "  INST  " << I->getOpcodeName() << " : " << *I << "\n";
    else if (auto *CE = dyn_cast<ConstantExpr>(U))
      errs() << "  CEXPR " << Instruction::getOpcodeName(CE->getOpcode())
             << " : " << *CE << "\n";
    else
      errs() << "  USER  : " << *U << "\n";
  }
}

PreservedAnalyses PreloadPass::run(Module &M, ModuleAnalysisManager &MA) {

  LLVMContext &CTX = M.getContext();
  errs()<<" Reading from a file llvm\n";

  for (Module::iterator func_iter = M.begin(), func_iter_end = M.end(); func_iter != func_iter_end; ++func_iter) {
    Function &F = *func_iter;
    errs()<<"Function name is: "<<F.getName()<<"\n";

    if(F.getName().contains("__kmpc_omp_task_alloc")){
      const DataLayout &DL = M.getDataLayout();

      for (User *U : F.users()) {
        auto *CB = dyn_cast<CallBase>(U);
        if (!CB) continue;

        // Last arg is the task entry routine in both variants.
        Value *EntryArg = CB->getArgOperand(CB->arg_size()-1);
        Function *EntryF = stripToFunc(EntryArg);
        if (!EntryF) continue;
        if (!EntryF->getName().contains(".omp_task_entry.")) continue;

        Value *Task = CB; // return value
        dumpUsers(Task);
        bool Found = false;

        for(User *U : Task->users()) {
          auto *GEP = dyn_cast<GetElementPtrInst>(U);
          if (!GEP || GEP->getPointerOperand() != Task) continue;

          errs()<<"checking status\n";

          // Look for a store into that GEP of a pointer-typed value (char* candidate).
          for (User *GU : GEP->users()) {
            auto *SI = dyn_cast<StoreInst>(GU);
            if (!SI) continue;
            // Require: store <ptr>, ptr %gep
            for (User *W : GEP->users()) {
              if (auto *SI = dyn_cast<StoreInst>(W)) {
                if (SI->getPointerOperand() != GEP) continue;
                if (!SI->getValueOperand()->getType()->isPointerTy()) continue; // filters out i32 at 32
              
                Value *V = SI->getValueOperand()->stripPointerCasts();
                if (auto *K = dyn_cast<Constant>(V)) {
                  NameConstByEntry[EntryF] = K;   // e.g., ptr @.str.1 or a constexpr GEP
                  errs()<<"name entry string is: "<<NameConstByEntry[EntryF]<<"\n";
                  std::string test_str;
                  raw_string_ostream stream(test_str);
                  K->print(stream);
                  errs()<<test_str<<"\n";
                  
                  Instruction *IP = &*EntryF->getEntryBlock().getFirstNonPHIOrDbgOrAlloca();
                  // Type *I8Ty    = Type::getInt8Ty(CTX);
                  // Type *I8PtrTy = I8Ty->getPointerTo();
                  Type *PtrTy = PointerType::get(CTX, 0); 
                  IRBuilder<> B(IP);

                  auto Callee = EntryF->getParent()->getOrInsertFunction("unique_task", FunctionType::get(B.getVoidTy(), {PtrTy}, false));
                  Function *UF = cast<Function>(Callee.getCallee());
                  UF->setLinkage(Function::ExternalWeakLinkage);
                  B.CreateCall(Callee, {K});
                }
              }
            }
          }
        }
      }
    }
  }
    
    std::cout<<"--------=========== end of pass ==================----------------"<<std::endl;
    return PreservedAnalyses::all();
}

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "preload", "v0.1",
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](StringRef Name, ModulePassManager &MPM,
            ArrayRef<PassBuilder::PipelineElement>) {
          if (Name == "preload") { MPM.addPass(PreloadPass()); return true; }
          return false;
        });
    }};
}