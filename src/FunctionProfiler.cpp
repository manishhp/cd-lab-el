#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

class FunctionProfilerPass : public PassInfoMixin<FunctionProfilerPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
    Module &M = *F.getParent();
    LLVMContext &Ctx = M.getContext();

    // Skip external declarations
    if (F.isDeclaration()) {
      return PreservedAnalyses::all();
    }

    // Create a global i64 counter for this function
    std::string CounterName = F.getName().str() + "_counter";
    Type *I64Ty = Type::getInt64Ty(Ctx);
    GlobalVariable *Counter = dyn_cast<GlobalVariable>(
        M.getOrInsertGlobal(CounterName, I64Ty));
    if (!Counter->hasInitializer()) {
      Counter->setInitializer(ConstantInt::get(I64Ty, 0));
    }

    // Insert increment logic at the beginning of the function
    BasicBlock &Entry = F.getEntryBlock();
    IRBuilder<> Builder(&Entry, Entry.getFirstInsertionPt());

    // Load the current counter value
    Value *Load = Builder.CreateLoad(I64Ty, Counter, "load_counter");
    // Add 1 to it
    Value *Inc = Builder.CreateAdd(Load, ConstantInt::get(I64Ty, 1), "inc_counter");
    // Store back
    Builder.CreateStore(Inc, Counter);

    // Register the counter for dumping
    // Declare registerCounter function
    FunctionType *RegFuncTy = FunctionType::get(
        Type::getVoidTy(Ctx),
        {Type::getInt8PtrTy(Ctx), Type::getInt64PtrTy(Ctx)},
        false);
    FunctionCallee RegFunc = M.getOrInsertFunction("registerCounter", RegFuncTy);

    // Create a global string for the function name
    Value *NameStr = Builder.CreateGlobalStringPtr(F.getName().str());
    // Pass the counter pointer
    Builder.CreateCall(RegFunc, {NameStr, Counter});

    // If this is main, insert dumpProfile before return
    if (F.getName() == "main") {
      // Find the return instruction
      for (auto &BB : F) {
        for (auto &I : BB) {
          if (auto *Ret = dyn_cast<ReturnInst>(&I)) {
            Builder.SetInsertPoint(Ret);
            // Declare dumpProfile
            FunctionType *DumpFuncTy = FunctionType::get(Type::getVoidTy(Ctx), false);
            FunctionCallee DumpFunc = M.getOrInsertFunction("dumpProfile", DumpFuncTy);
            Builder.CreateCall(DumpFunc);
            goto done; // Only insert once
          }
        }
      }
    done:;
    }

    return PreservedAnalyses::none();
  }
};

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "FunctionProfiler", "v0.1",
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](StringRef Name, FunctionPassManager &FPM,
           ArrayRef<PassBuilder::PipelineElement>) {
          if (Name == "function-profiler") {
            FPM.addPass(FunctionProfilerPass());
            return true;
          }
          return false;
        });
    }
  };
}