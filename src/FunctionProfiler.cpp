// File: FunctionProfiler.cpp
// Description: LLVM module pass that injects function and loop profiling counters.
// Part of: LLVM Profiling Pass Project

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

#include <cctype>
#include <string>
#include <vector>

using namespace llvm;

namespace {

struct CounterInfo {
  GlobalVariable *NameGlobal;
  GlobalVariable *CountGlobal;
};

static std::string sanitizeName(StringRef Name) {
  std::string Out;
  Out.reserve(Name.size());
  for (char C : Name) {
    unsigned char UC = static_cast<unsigned char>(C);
    if (std::isalnum(UC) || C == '_') {
      Out.push_back(C);
    } else {
      Out.push_back('_');
    }
  }
  return Out.empty() ? "anon" : Out;
}

class FunctionProfilerPass : public PassInfoMixin<FunctionProfilerPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
    LLVMContext &Ctx = M.getContext();
    Type *I64Ty = Type::getInt64Ty(Ctx);
    Type *PtrTy = PointerType::getUnqual(Ctx);

    auto &FAMProxy = MAM.getResult<FunctionAnalysisManagerModuleProxy>(M);
    FunctionAnalysisManager &FAM = FAMProxy.getManager();

    std::vector<CounterInfo> Counters;
    Counters.reserve(M.size());

    for (Function &F : M) {
      if (F.isDeclaration()) {
        continue;
      }

      StringRef FName = F.getName();
      if (FName.starts_with("__profiler_") || FName == "dumpProfile") {
        continue;
      }

      std::string Suffix = sanitizeName(FName);
      std::string CounterSymbol = "__prof_count_" + Suffix;
      std::string NameSymbol = "__prof_name_" + Suffix;

      auto *CountGV = cast<GlobalVariable>(M.getOrInsertGlobal(CounterSymbol, I64Ty));
      CountGV->setLinkage(GlobalValue::InternalLinkage);
      CountGV->setAlignment(Align(8));
      if (!CountGV->hasInitializer()) {
        CountGV->setInitializer(ConstantInt::get(I64Ty, 0));
      }

      Constant *NameInit = ConstantDataArray::getString(Ctx, FName, true);
      auto *NameGV = new GlobalVariable(M, NameInit->getType(), true,
                                        GlobalValue::PrivateLinkage, NameInit,
                                        NameSymbol);
      NameGV->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
      NameGV->setAlignment(Align(1));

      Counters.push_back({NameGV, CountGV});

      Instruction *EntryIP = &*F.getEntryBlock().getFirstInsertionPt();
      IRBuilder<> EntryBuilder(EntryIP);
      EntryBuilder.CreateAtomicRMW(AtomicRMWInst::Add, CountGV,
                                   ConstantInt::get(I64Ty, 1), MaybeAlign(),
                                   AtomicOrdering::Monotonic);

      LoopInfo &LI = FAM.getResult<LoopAnalysis>(F);
      SmallVector<Loop *, 8> Stack(LI.begin(), LI.end());
      while (!Stack.empty()) {
        Loop *L = Stack.pop_back_val();
        for (Loop *SubLoop : L->getSubLoops()) {
          Stack.push_back(SubLoop);
        }

        BasicBlock *Header = L->getHeader();
        if (!Header) {
          continue;
        }

        if (Header->empty()) {
          continue;
        }

        Instruction *HeaderIP = &*Header->getFirstInsertionPt();
        IRBuilder<> LoopBuilder(HeaderIP);
        LoopBuilder.CreateAtomicRMW(AtomicRMWInst::Add, CountGV,
                                    ConstantInt::get(I64Ty, 1), MaybeAlign(),
                                    AtomicOrdering::Monotonic);
      }
    }

    if (!Counters.empty()) {
        FunctionType *RegisterTy =
          FunctionType::get(Type::getVoidTy(Ctx), {PtrTy, PtrTy}, false);
      FunctionCallee RegisterFn =
          M.getOrInsertFunction("__profiler_register", RegisterTy);

      Function *InitFn = Function::Create(
          FunctionType::get(Type::getVoidTy(Ctx), false),
          GlobalValue::InternalLinkage, "__profiler_init", M);
      BasicBlock *InitEntry = BasicBlock::Create(Ctx, "entry", InitFn);
      IRBuilder<> InitBuilder(InitEntry);

      Value *Zero = ConstantInt::get(Type::getInt32Ty(Ctx), 0);
      for (const CounterInfo &Info : Counters) {
        Value *NamePtr = InitBuilder.CreateInBoundsGEP(
            Info.NameGlobal->getValueType(), Info.NameGlobal, {Zero, Zero});
        InitBuilder.CreateCall(RegisterFn, {NamePtr, Info.CountGlobal});
      }

      InitBuilder.CreateRetVoid();
      appendToGlobalCtors(M, InitFn, 0);
    }

    if (Function *Main = M.getFunction("main"); Main && !Main->isDeclaration()) {
      FunctionCallee DumpFn = M.getOrInsertFunction(
          "dumpProfile", FunctionType::get(Type::getVoidTy(Ctx), false));

      SmallVector<ReturnInst *, 8> Returns;
      for (BasicBlock &BB : *Main) {
        if (auto *Ret = dyn_cast<ReturnInst>(BB.getTerminator())) {
          Returns.push_back(Ret);
        }
      }

      for (ReturnInst *Ret : Returns) {
        IRBuilder<> RetBuilder(Ret);
        RetBuilder.CreateCall(DumpFn);
      }
    }

    return PreservedAnalyses::none();
  }
};

} // namespace

extern "C" PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK llvmGetPassPluginInfo() {
  return {
      LLVM_PLUGIN_API_VERSION,
      "FunctionProfiler",
      "v0.2",
      [](PassBuilder &PB) {
        PB.registerPipelineParsingCallback(
            [](StringRef Name, ModulePassManager &MPM,
               ArrayRef<PassBuilder::PipelineElement>) {
              if (Name == "profiler-pass") {
                MPM.addPass(FunctionProfilerPass());
                return true;
              }
              return false;
            });
      }};
}