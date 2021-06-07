#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Type.h"
#include <iostream>
#include <bits/stdc++.h>
#include "llvm/IR/GlobalVariable.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
using namespace llvm;

#define DEBUG_TYPE "addfunc"

bool declare = true;

// struct mod_val {
//     int mod;
// };

// typedef struct mod_val mod_val;

namespace {
  struct AddOmptTest : public LoopPass {
    static char ID; // Pass identification, replacement for typeid
    void getAnalysisUsage(AnalysisUsage &AU) const {
        AU.setPreservesAll();
        AU.addRequired<LoopInfoWrapperPass>();
    }
    //virtual bool runOnModule(Module &M);

    bool runOnLoop(Loop *L, LPPassManager &LPM) override {
        errs() << "Loop block: ";
        errs().write_escaped(LPM.getPassName()) << '\n';
        
        for(BasicBlock *B : L->getBlocks()){
            errs().write_escaped(B->getName()) << '\n';
            PHINode *P = L->getCanonicalInductionVariable();
            Module *M = B->getModule();
            Function *F = B->getParent();
            if(!F->isDeclaration())
                LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>(*F).getLoopInfo();
            auto &CTX = M->getContext();

            Value *threshold =  ConstantInt::get(Type::getInt32Ty(CTX),2);
            Value *threshold1 =  ConstantInt::get(Type::getInt32Ty(CTX),2);
            GlobalVariable *gVar = new GlobalVariable(threshold->getType(),false,GlobalValue::CommonLinkage,0,"x");

            /*
             ////////////////////////////////////////////  Create a structure and assign global variable ///////////////////////////////////////////
            */


            std::vector<Type*> putsArgs;
            //Type* data = Type::getInt32Ty(CTX);
            Type* data = threshold->getType();
            putsArgs.push_back(data);
            ArrayRef<Type*> argsRef(putsArgs);
            StructType* llvm_struct = StructType::create(CTX, argsRef,"mod_val", false);
            //Value* data =  ConstantInt::get(Type::getInt32Ty(CTX),2);/*A structure value*/

            //AllocaInst* alloc = irbuilder.CreateAlloca(llvm_struct, 0, "alloctmp");
            //irbuilder.CreateStore(data, alloc);

            //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

            Value* temp;
            BasicBlock *Btemp;

            if(B->getName() == "omp.inner.for.cond"){
                Btemp = B;
            }

            if(B->getName() == "omp.inner.for.inc"){

                FunctionType *testing = FunctionType::get(
                    IntegerType::getInt32Ty(CTX),  // return type
                    //Type::getVoidTy(CTX),
                    IntegerType::getInt32Ty(CTX), //  argument type
                    //PointerType::getPointerAddressSpace(),
                    /*IsVarArgs=*/false);

                FunctionCallee check = M->getOrInsertFunction("test", testing);

                Function *testF = dyn_cast<Function>(check.getCallee());

                // Function::arg_iterator args = testF->arg_begin();
                // Value* ind_var = args++;  // take the first argument
                // ind_var->setName("i");


                for(llvm::BasicBlock::iterator I = B->begin(), Iend = B->end(); I != Iend ; ++I){
                    errs().write_escaped(I->getOperand(0)->getName())<<'\n';
                    if(I->getOperand(0)->getName() == "add3"){
                        errs().write_escaped(I->getOperand(1)->getName())<<'\n';
                        errs().write_escaped("working")<<'\n';
                        // IRBuilder<> builder(B);
                        // BasicBlock *Parent = builder.GetInsertBlock();
                        

                        /* Calling func which is predefined by creating a call in llvm-ir*/

                        /////////////////////////

                        FunctionCallee hookFunc = M->getOrInsertFunction("func", testing);
                        FunctionCallee hookTest = M->getOrInsertFunction("ompt_test", testing);

                        Function *hook = dyn_cast<Function>(hookTest.getCallee());


                        /////////////////////////

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
                       //  //temp = builder.CreateCall(check, args1, "tmp");

                        /*
                            Only calling instruction
    
                        */

                        ////////////////////////////////////////////////////////////////////////////////


                        // Instruction *newInst = CallInst::Create(hook, args1, "");
                        // B->getInstList().insert(I,newInst);

                        
                        ////////////////////////////////////////////////////////////////////////////////

                        // BasicBlock::iterator J = --I;
                        // I++;


                        // IRBuilder<> ompt2(I->getNextNode());

                        /*
                                            
                            Adding if else blocks
                            
                        */

                        Function *F = B->getParent();
                        IRBuilder<> ompt2(B);
                        BasicBlock* callTex = BasicBlock::Create(CTX, "if.then");
                        IRBuilder<> calling(callTex);
                        //Instruction *newInst = CallInst::Create(hook, args1, "");
                        //callTex->getInstList().insert(I,newInst);
                        calling.CreateCall(hook,args1,"temp");
                        calling.CreateBr(Btemp);
                        Value* compare = ompt2.CreateICmpEQ(threshold1,threshold,"tmp");
                        //LoopInfo &LI = getAnalysis<LoopInfo>();
                        LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>(*F).getLoopInfo();
                        //Instruction *Inst = SplitBlockAndInsertIfThen(compare, I->getNextNode(), false, nullptr, nullptr, &LI, callTex);
                        //Instruction *Inst = SplitBlockAndInsertIfThen(compare, I->getNextNode(), false, nullptr, nullptr, &LI, nullptr);
                        

                        /////////////////////////////////////////////////////////////////////////////////////////////////////////////

                        //B->getInstList().insert(I,Inst);

                       //  /* Using split block technique*/

                       //  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

                       //  //Value* compare = ompt2.CreateICmpEQ(vo,threshold,"tmp"); // have to pick value from structure instead of threshold
                       //  //ompt2.SetInsertPoint(I->getNextNode());
                        
                       //  /*Create the if then block below*/

                        // BasicBlock *callTest = BasicBlock::Create(CTX,"temp_block");
                        // IRBuilder<>calling(callTest);
                        // calling.CreateCall(hook, args1, "tmp");
                        // // calling.CreateAdd(vo,threshold,"val",false,false);
                        // calling.SetInsertPoint(I->getNextNode());
                        // Function *F = B->getParent();
                        // LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>(*F).getLoopInfo();

                        // Instruction *Inst = SplitBlockAndInsertIfThen(compare, I->getNextNode(), true, nullptr, nullptr, &LI, nullptr);
                        // //ompt2.Insert(Inst);
                        // ompt2.SetInsertPoint(Inst);

                       //  //calling.SetInsertPoint(I->getNextNode());

                       //  // ompt2.SetInsertPoint(Inst);
                       //  //calling.SetInsertPoint(I->getNextNode());


                       //  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

                       //  /*Just by adding instructions*/

                       //  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////


                       //  //BasicBlock *newB = B->splitBasicBlock(I,"newblock");


                       //  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                        
                        //temp = calling.CreateCall(hookTest, args1, "tmp");
                        //ompt2.CreateCall(hookTest, args1, "tmp");

                        /*
    
                                Add explicit sync similarly
                        */


                        /////////////////////////////////////////////////////////////////////////////////////

                        FunctionType *sync = FunctionType::get(
                        //IntegerType::getInt32Ty(CTX),  // return type
                        Type::getVoidTy(CTX),
                        IntegerType::getInt32Ty(CTX), //  argument type
                        //PointerType::getPointerAddressSpace(),
                        /*IsVarArgs=*/false);

                        FunctionCallee syncing = M->getOrInsertFunction("countCounter", sync);

                        Function *xsync = dyn_cast<Function>(syncing.getCallee());
                        Instruction *newInst2 = CallInst::Create(xsync, args1, "");
                        B->getInstList().insert(I,newInst2);

                        ////////////////////////////////////////////////////////////////////////////////////

                        break;
                    }
                }

            }    

            if(B->getName() == "omp.inner.for.body"){
            }
        }

    return true;
    }

    AddOmptTest() : LoopPass(ID) {}

    // bool AddOmptTest::runOnModule(Module &M){
    //     for (Module::iterator func_iter = M.begin(), func_iter_end = M.end(); func_iter != func_iter_end; ++func_iter) {
    //         Function &F = *func_iter;

    //         if (F.getName() == ".omp_outlined.") {
    //             LoopInfo &LI = getAnalysis<LoopInfo>(F);
    //         }
    //     }

    //     return false;
    // }

  };
}

char AddOmptTest::ID = 0;
static RegisterPass<AddOmptTest> X("addompt2", "add function");