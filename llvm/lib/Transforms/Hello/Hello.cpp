//===- Hello.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "Hello World" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//

//#include "llvm/Constants.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Type.h"
#include <iostream>
#include <bits/stdc++.h>
#include "llvm/IR/GlobalVariable.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
  class ModuleLoop : public ModulePass {
    public:
      static char ID; // Pass identification, replacement for typeid
      
      void getAnalysisUsage(AnalysisUsage &AU) const {
        AU.setPreservesAll();
        AU.addRequired<LoopInfoWrapperPass>();
      }


      bool runOnModule(Module &M) {

        auto &CTX = M.getContext();
        Value *threshold =  ConstantInt::get(Type::getInt32Ty(CTX),2);
        Value *threshold1 =  ConstantInt::get(Type::getInt32Ty(CTX),2);
        Value *tempcmp = ConstantInt::getBool(CTX,false);


        BasicBlock *Btemp;

        for (Module::iterator func_iter = M.begin(), func_iter_end = M.end(); func_iter != func_iter_end; ++func_iter) {
          
          Function &F = *func_iter;

          if (!F.isDeclaration()) {
            if(F.getName() == ".omp_outlined."){
              LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>(F).getLoopInfo();
              errs() << F.getName() <<"\n";
              for (BasicBlock &B: F) {
                if(B.getName() == "omp.inner.for.cond"){
                  Btemp = &B;
                  errs() << B.getName() <<"\n";
                }

                else if(B.getName() == "omp.inner.for.inc"){

                  for(llvm::BasicBlock::iterator I = B.begin(), Iend = B.end(); I != Iend ; ++I){

                    errs().write_escaped(I->getOperand(0)->getName())<<'\n';
                    if(I->getOperand(0)->getName() == "add3"){
                      errs().write_escaped(I->getOperand(1)->getName())<<'\n';
                      errs().write_escaped("working")<<'\n';
                      // IRBuilder<> builder(B);
                      // BasicBlock *Parent = builder.GetInsertBlock();

                      // Constant * C = dyn_cast<Constant*>(I->getOperand(0));
                      Value* vo = I->getOperand(0);
                      //raw_ostream &O = raw_ostream(false);
                      Type* t = vo->getType();
                      std::string type_str;
                      raw_string_ostream rso(type_str);
                      t->print(rso); //i32
                      std::cout<<"v0 type is "<<rso.str(); //add3

                      std::vector<Value*> args1;
                      args1.push_back(vo);

                      FunctionType *testing = FunctionType::get(
                      IntegerType::getInt32Ty(CTX),  // return type
                      //Type::getVoidTy(CTX),
                      IntegerType::getInt32Ty(CTX), //  argument type
                      //PointerType::getPointerAddressSpace(),
                      /*IsVarArgs=*/false);
                      

                      //FunctionCallee hookTest = M.getOrInsertFunction("ompt_test", testing);
                      //Function *hook = dyn_cast<Function>(hookTest.getCallee());
                      

                      IRBuilder<> ompt2(&B);
                      BasicBlock* callTex = BasicBlock::Create(CTX, "checking");
                      IRBuilder<> calling(callTex);
                      //Instruction *newInst = CallInst::Create(hook, args1, "");
                      //callTex->getInstList().insert(I,newInst);
                      

                      //calling.CreateCall(hook,args1,"temp");
                      

                      calling.CreateBr(Btemp);
                      //Value* compare = ompt2.CreateICmpEQ(threshold1,threshold,"tmp");
                      //LoopInfo &LI = getAnalysis<LoopInfo>();
                      //Instruction *Inst = SplitBlockAndInsertIfThen(compare, I->getNextNode(), false, nullptr, nullptr, nullptr, nullptr);
                      //Instruction *Inst = SplitBlockAndInsertIfThen(tempcmp, I->getNextNode(), false, nullptr, nullptr, &LI, callTex);
                      break;
                    }
                  }
                }
              }
            }
            
            //for (Function::const_iterator bb_it = func_iter->begin(), bb_ite = func_iter->end(); bb_it != bb_ite; ++bb_it)
              //errs() << bb_it->getName() << "\n";//<< " " << LI.getLoopDepth(bb_it) << "\n";
          }

        }

        return false;
      }

      ModuleLoop() : ModulePass(ID) {}
  };
}
  // ***************************************************************************

char ModuleLoop::ID = 0;
static RegisterPass<ModuleLoop> X("moduleloop", "LoopPass within ModulePass");


// char Hello2::ID = 0;
// static RegisterPass<Hello2>
// Y("hello2", "Hello World Pass (with getAnalysisUsage implemented)");
