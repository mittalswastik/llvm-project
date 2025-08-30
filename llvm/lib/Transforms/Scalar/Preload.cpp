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
#include "llvm/Transforms/Scalar/Preload.h"
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

struct TaskPrivLayout {
  StructType *RootTy = nullptr;            // %task_with_privates
  SmallVector<unsigned, 4> Indices;        // e.g., {0,1,0} -> privates.name
  uint64_t ByteOffset = 0;                 // fallback (if you prefer)
  bool HasTyped = false;
};


llvm::DenseMap<llvm::Function*, llvm::Constant*> NameConstByEntry;
std::unordered_map<Function*, struct TaskPrivLayout> umap;

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

static bool deriveLayoutFromAlloc(CallBase *AllocCB, TaskPrivLayout &Out, const DataLayout &DL, uint64_t *OffOut) {
  Value *Task = AllocCB; // return value
  dumpUsers(Task);
  bool Found = false;

  for (User *U : Task->users()) {
    auto *GEP = dyn_cast<GetElementPtrInst>(U);
    if (!GEP || GEP->getPointerOperand() != Task) continue;

    errs()<<"checking status\n";

    // Look for a store into that GEP of a pointer-typed value (char* candidate).
    for (User *GU : GEP->users()) {
      auto *SI = dyn_cast<StoreInst>(GU);
      if (!SI) continue;



      if (GEP->getSourceElementType()->isIntegerTy(8) && GEP->getNumIndices() == 1) {
        if (auto *CI = dyn_cast<ConstantInt>(GEP->idx_begin()->get())) {
          uint64_t Off = CI->getZExtValue();

          // Require: store <ptr>, ptr %gep
          for (User *W : GEP->users()) {
            if (auto *SI = dyn_cast<StoreInst>(W)) {
              if (SI->getPointerOperand() != GEP) continue;
              if (!SI->getValueOperand()->getType()->isPointerTy()) continue; // filters out i32 at 32
              OffOut = &Off;   // e.g., 40
              return true;
            }
          }
        }
      }

      auto *Root = dyn_cast<StructType>(GEP->getSourceElementType());
      if (!Root) continue; // may be i8 GEP; keep scanning

      if (!SI->getValueOperand()->getType()->isPointerTy()) continue;

      SmallVector<unsigned,4> Idx;
      bool AllConst = true;
      for (auto &Op : GEP->indices()) {
        auto *CI = dyn_cast<ConstantInt>(Op.get());
        if (!CI) { AllConst = false; break; }
        Idx.push_back((unsigned)CI->getZExtValue());
      }
      if (!AllConst || Idx.size() < 3) continue; // expect {0, 1, field}

      Out.RootTy   = Root;
      Out.Indices  = std::move(Idx);
      Out.HasTyped = true;

      // Also compute a byte offset as optional fallback:
      StructType *T0 = Root;               // %task_with_privates
      const auto *SL0 = DL.getStructLayout(T0);
      unsigned I0 = Out.Indices[1];              // privates field index
      auto *PrivST = dyn_cast<StructType>(T0->getElementType(I0));
      const auto *SL1 = DL.getStructLayout(PrivST);
      unsigned I1 = Out.Indices[2];              // 'name' field index
      Out.ByteOffset = SL0->getElementOffset(I0) + SL1->getElementOffset(I1);
      Found = true;
      break;
    }
  }
  return Found;
}

PreservedAnalyses PreloadPass::run(Module &M, ModuleAnalysisManager &MA) {

    LLVMContext &CTX = M.getContext();
    errs()<<" Reading from a file llvm\n";
    // if ((Options.count("set_ttex") && input_ttex)){
    //   return PreservedAnalyses::all();
    // }

    for (Module::iterator func_iter = M.begin(), func_iter_end = M.end(); func_iter != func_iter_end; ++func_iter) {
      Function &F = *func_iter;

      if(F.getName().contains("__kmpc_omp_task_alloc")){
        errs()<<"Function name is: "<<F.getName()<<"\n";
        const DataLayout &DL = M.getDataLayout();

        for (User *U : F.users()) {
          auto *CB = dyn_cast<CallBase>(U);
          if (!CB) continue;

          // Last arg is the task entry routine in both variants.
          Value *EntryArg = CB->getArgOperand(CB->arg_size()-1);
          Function *EntryF = stripToFunc(EntryArg);
          if (!EntryF) continue;
          if (!EntryF->getName().contains(".omp_task_entry.")) continue;

          // struct TaskPrivLayout T;
          // uint64_t OffOut;
          // bool Ok = deriveLayoutFromAlloc(CB, T, DL, &OffOut);
          // errs()<<"OK value is: "<<Ok<<"\n";
          // if (!Ok) continue; // try next alloc; maybe another user has typed GEPs

          Value *Task = CB; // return value
          dumpUsers(Task);
          bool Found = false;

          for (User *U : Task->users()) {
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
                    
                    // Instruction *IP = &*EntryF->getEntryBlock().getFirstNonPHIOrDbgOrAlloca();
                    // Type *I8Ty    = Type::getInt8Ty(CTX);
                    // Type *I8PtrTy = I8Ty->getPointerTo(); 
                    // IRBuilder<> B(IP);
                    // Value *Task  = EntryF->getArg(1);
                    // Value *Addr  = B.CreateGEP(I8Ty, Task, B.getInt64(OffOut)); // 40
                    // Value *Name  = B.CreateLoad(I8PtrTy, Addr);

                    // auto Callee = EntryF->getParent()->getOrInsertFunction(
                    //                 "unique_task", FunctionType::get(B.getVoidTy(), {I8PtrTy}, false));
                    // Function *UF = cast<Function>(Callee.getCallee());
                    // UF->setLinkage(Function::ExternalWeakLinkage);
                    // B.CreateCall(Callee, {Name});
                  }
                }
              }
            }
          }

          // auto It = umap.find(EntryF);
          // if (It == umap.end()) {
          //   umap[EntryF] = T;
          // } else {
          //   // Multiple allocs → verify consistency (defensive)
          //   auto &Prev = It->second;
          //   if (T.HasTyped && Prev.HasTyped) {
          //     if (T.RootTy != Prev.RootTy || T.Indices != Prev.Indices) {
          //       errs() << "warning: differing task priv layouts for " << EntryF->getName() << "\n";
          //     }
          //   }
          //   // Prefer first typed layout; keep as-is.
          // }
        }
      }
    }    


    // for(auto itr = umap.begin() ; itr != umap.end() ; ++itr){
    //   Function &F = *(itr->first);
    //   errs()<<"function names in map: "<<F.getName()<<"\n";

    //   for (Function::iterator block_iter = F.begin(), block_iter_end = F.end(); block_iter != block_iter_end; ++block_iter) {
    //       BasicBlock &B = *block_iter;
    //       for(BasicBlock::iterator instr_iter = B.begin(), instr_iter_end = B.end(); instr_iter != instr_iter_end; ++instr_iter){
    //           Instruction &I = *instr_iter;

    //           //errs()<<"Basic block name "<<B.getName()<<"\n";

    //           if(PHINode *Pi = dyn_cast<PHINode>(&I)){
    //             continue;
    //           }

    //           if(&I == nullptr){
    //               continue; // null instruction?
    //           }

    //           //AddFunc(M,B,I,itr->second);
    //       }
    //   }

    // }

    // for (Module::iterator func_iter = M.begin(), func_iter_end = M.end(); func_iter != func_iter_end; ++func_iter) {
    
    //     Function &F = *func_iter;

    //     if (!F.isDeclaration()) {
    //         errs()<<"Function name is:"<<F.getName()<<"\n";
    //         if(!F.getName().contains(".omp_task_entry.") && !F.getName().contains("ompt") && !F.getName().contains("debug")){ //F.getName() != ".omp_outlined._debug__"){
    //             auto &FM = MA.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
    //             LoopInfo *LI = &FM.getResult<LoopAnalysis>(F);
    //             errs()<<"Calling analysis manager above for loops\n";

                // for (Function::iterator block_iter = F.begin(), block_iter_end = F.end(); block_iter != block_iter_end; ++block_iter) {
                //     BasicBlock &B = *block_iter;
                //     for(BasicBlock::iterator instr_iter = B.begin(), instr_iter_end = B.end(); instr_iter != instr_iter_end; ++instr_iter){
                //         Instruction &I = *instr_iter;

                //         //errs()<<"Basic block name "<<B.getName()<<"\n";

                //         if(PHINode *Pi = dyn_cast<PHINode>(&I)){
                //           continue;
                //         }

                //         if(&I == nullptr){
                //             continue; // null instruction?
                //         }

                //         AddFunc(M,B,I);
                //     }
                // }
    //         }
    //     }
    // }
    
    std::cout<<"--------=========== end of pass ==================----------------"<<std::endl;
    return PreservedAnalyses::all();
}

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "preload_2", "v0.1",
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](StringRef Name, ModulePassManager &MPM,
            ArrayRef<PassBuilder::PipelineElement>) {
          if (Name == "preload") { MPM.addPass(PreloadPass()); return true; }
          return false;
        });
    }};
}