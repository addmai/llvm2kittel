// This file is part of llvm2KITTeL
//
// Copyright 2010-2014 Stephan Falke
//
// Licensed under the University of Illinois/NCSA Open Source License.
// See LICENSE for details.

#include "llvm2kittel/Analysis/ConditionPropagator.h"
#include "llvm2kittel/Analysis/HierarchyBuilder.h"
#include "llvm2kittel/Analysis/InstChecker.h"
#include "llvm2kittel/Analysis/LoopConditionBlocksCollector.h"
#include "llvm2kittel/Analysis/LoopConditionExplicitizer.h"
#include "llvm2kittel/Analysis/MemoryAnalyzer.h"
#include "llvm2kittel/BoundConstrainer.h"
#include "llvm2kittel/ConstraintEliminator.h"
#include "llvm2kittel/ConstraintSimplifier.h"
#include "llvm2kittel/Converter.h"
#include "llvm2kittel/DivRemConstraintType.h"
#include "llvm2kittel/Export/ComplexityTuplePrinter.h"
#include "llvm2kittel/Export/UniformComplexityTuplePrinter.h"
#include "llvm2kittel/IntTRS/Polynomial.h"
#include "llvm2kittel/IntTRS/Rule.h"
#include "llvm2kittel/Kittelizer.h"
#include "llvm2kittel/Slicer.h"
#include "llvm2kittel/Transform/BasicBlockSorter.h"
#include "llvm2kittel/Transform/BitcastCallEliminator.h"
#include "llvm2kittel/Transform/ConstantExprEliminator.h"
#include "llvm2kittel/Transform/EagerInliner.h"
#include "llvm2kittel/Transform/ExtremeInliner.h"
#include "llvm2kittel/Transform/Hoister.h"
#include "llvm2kittel/Transform/InstNamer.h"
#include "llvm2kittel/Transform/Mem2Reg.h"
#include "llvm2kittel/Transform/NondefFactory.h"
#include "llvm2kittel/Transform/StrengthIncreaser.h"
#include "llvm2kittel/Util/CommandLine.h"
#include "llvm2kittel/Util/Version.h"

// llvm includes
#include "WARN_OFF.h"
#if LLVM_VERSION < VERSION(3, 5)
#include <llvm/Assembly/PrintModulePass.h>
#endif
#include <llvm/Bitcode/ReaderWriter.h>
#if LLVM_VERSION < VERSION(3, 3)
#include <llvm/LLVMContext.h>
#else
#include <llvm/IR/LLVMContext.h>
#endif
#if LLVM_VERSION < VERSION(3, 2)
#include <llvm/Target/TargetData.h>
#elif LLVM_VERSION == VERSION(3, 2)
#include <llvm/DataLayout.h>
#else
#include <llvm/IR/DataLayout.h>
#endif
#if LLVM_VERSION < VERSION(3, 7)
#include <llvm/PassManager.h>
#else
#include <llvm/IR/LegacyPassManager.h>
#endif
#include <llvm/Analysis/Passes.h>
#if LLVM_VERSION >= VERSION(3, 8)
#include <llvm/Analysis/BasicAliasAnalysis.h>
#endif
#if LLVM_VERSION < VERSION(3, 5)
#include <llvm/Analysis/Verifier.h>
#else
#include <llvm/IR/Verifier.h>
#endif
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Instructions.h>
#if LLVM_VERSION < VERSION(3, 5)
#include <llvm/Support/system_error.h>
#else
#include <system_error>
#endif
#if LLVM_VERSION >= VERSION(3, 5)
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/FileSystem.h>
#endif
#include "WARN_ON.h"

// C++ includes
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <algorithm>

// command line stuff

#include "GitSHA1.h"

void versionPrinter() {
  std::cout << "llvm2KITTeL" << std::endl;
  std::cout << "Copyright 2010-2015 Stephan Falke" << std::endl;
  std::cout << "Version " << get_git_sha1();
  std::cout << ", using LLVM " << LLVM_MAJOR << "." << LLVM_MINOR << std::endl;
}

static cl::opt<std::string> filename(cl::Positional, cl::Required,
                                     cl::desc("<input bitcode>"),
                                     cl::init(std::string()));
static cl::opt<std::string>
    functionname("function",
                 cl::desc("Start function for the termination analysis"),
                 cl::init(std::string()));
static cl::opt<unsigned int>
    numInlines("inline", cl::desc("Maximum number of function inline steps"),
               cl::init(0));
static cl::opt<bool>
    eagerInline("eager-inline",
                cl::desc("Exhaustively inline (acyclic call hierarchies only)"),
                cl::init(false));

static cl::opt<bool> debug("debug", cl::desc(""), cl::init(false),
                           cl::ReallyHidden);
static cl::opt<bool>
    assumeIsControl("assume-is-control",
                    cl::desc("Calls to assume are control points"),
                    cl::init(false));
static cl::opt<bool>
    selectIsControl("select-is-control",
                    cl::desc("Select instructions are control points"),
                    cl::init(false));
static cl::opt<bool>
    inlineVoids("inline-voids",
                cl::desc("Function with return type void are also inlined"),
                cl::init(false));
static cl::opt<bool>
    increaseStrength("increase-strength",
                     cl::desc("Replace shifts by multiplication/division"),
                     cl::init(false));
static cl::opt<bool> noSlicing("no-slicing",
                               cl::desc("Do not slice the generated TRS"),
                               cl::init(false));
static cl::opt<bool> conservativeSlicing(
    "conservative-slicing",
    cl::desc("Be conservative in slicing the generated TRS"), cl::init(false));
static cl::opt<bool> onlyMultiPredIsControl(
    "multi-pred-control",
    cl::desc("Only basic blocks with multiple predecessors are control points"),
    cl::init(false));
static cl::opt<bool> boundedIntegers(
    "bounded-integers",
    cl::desc("Use bounded integers instead of mathematical integers"),
    cl::init(false));
static cl::opt<bool>
    unsignedEncoding("unsigned-encoding",
                     cl::desc("Use unsigned box for bounded integers"),
                     cl::init(false));
static cl::opt<bool> propagateConditions(
    "propagate-conditions",
    cl::desc("Propagate branch conditions to successor basic blocks"),
    cl::init(false));
static cl::opt<bool>
    explicitizeLoopConditions("explicitize-loop-conditions",
                              cl::desc("Derive certain loop conditions"),
                              cl::init(false), cl::ReallyHidden);
static cl::opt<bool>
    simplifyConds("simplify-conditions",
                  cl::desc("Simplify conditions in the generated TRS"),
                  cl::init(false));
static cl::opt<bool> onlyLoopConditions(
    "only-loop-conditions",
    cl::desc("Only encode loop conditions in the generated TRS"),
    cl::init(false));
static cl::opt<DivRemConstraintType> divisionConstraintType(
    "division-constraint",
    cl::desc("Determine the constraint type generated for division and "
             "remainder operations:"),
    cl::init(Approximated),
    cl::values(
        clEnumValN(
            Exact, "exact",
            "exactly model the semantics (only for mathematical integers)"),
        clEnumValN(Approximated, "approximated",
                   "approximately model the semantics (default)"),
        clEnumValN(None, "none", "do not model the semantics"), clEnumValEnd));
static cl::opt<SMTSolver> smtSolver(
    "smt-solver",
    cl::desc("Use SMT solver to eliminate rules with unsatisfiable contraints"),
    cl::init(NoSolver),
    cl::values(clEnumValN(CVC4Solver, "cvc4", "use CVC4"),
               clEnumValN(MathSAT5Solver, "mathsat5", "use MathSAT5"),
               clEnumValN(Yices2Solver, "yices2", "use Yices2"),
               clEnumValN(Z3Solver, "z3", "use Z3"),
               clEnumValN(NoSolver, "none",
                          "do not eliminate unsatisfiable contraints"),
               clEnumValEnd));
static cl::opt<bool>
    bitwiseConditions("bitwise-conditions",
                      cl::desc("Add conditions for bitwise & and |"),
                      cl::init(false));

static cl::opt<bool> dumpLL("dump-ll",
                            cl::desc("Dump transformed bitcode into a file"),
                            cl::init(false));

static cl::opt<bool> sourceInfo("source-info",
                               cl::desc("Output CSV of source info and exit"),
                               cl::init(false));

static cl::opt<bool>
  llvmVarToC("llvm-var-to-c",
         cl::desc("Output CSV mapping LLVM IR values to C variable names (from debug info) and exit"),
         cl::init(false));

static cl::opt<bool> t2Output("t2", cl::desc("Generate T2 format"),
                              cl::init(false), cl::ReallyHidden);
static cl::opt<bool> complexityTuples("complexity-tuples",
                                      cl::desc("Generate complexity tuples"),
                                      cl::init(false), cl::ReallyHidden);
static cl::opt<bool>
    uniformComplexityTuples("uniform-complexity-tuples",
                            cl::desc("Generate uniform complexity tuples"),
                            cl::init(false), cl::ReallyHidden);

//<Negar>
static cl::opt<bool> signednessInfo(
    "signedness-info",
    cl::desc("Include or exclude signedness information in the output file."),
    cl::init(false)); // Default is false
static cl::opt<bool>
    nondetTypeInfo("nondet-type-info",
                   cl::desc("Include or exclude type information in "
                            "nondeterministic function names."),
                   cl::init(false)); // Default is false
static cl::opt<bool> unreachableExit(
    "unreachable-exit",
    cl::desc("Control behavior on unreachable. "
             "True: jump to the exit basic block (termination). "
             "False: jump back to the same basic block where unreachable was "
             "encountered (non-termination)."),
    cl::init(true)); // Default is true (termination)
static cl::opt<bool> ignoreReachError(
    "ignore-reach-error",
    cl::desc(
        "Ignore reach_error() calls completely (useful for termination "
        "analysis). "
        "True: treat reach_error() as empty function. "
        "False: generate ERROR state and transitions (for safety analysis)."),
    cl::init(false)); // Default is false (safety analysis)
//</Negar>

void printSourceInfo(llvm::Module *module);
void printLLVMVarToC(llvm::Module *module);

void transformModule(llvm::Module *module, llvm::Function *function,
                     NondefFactory &ndf) {
#if LLVM_VERSION < VERSION(3, 2)
  llvm::TargetData *TD = NULL;
#elif LLVM_VERSION < VERSION(3, 5)
  llvm::DataLayout *TD = NULL;
#elif LLVM_VERSION < VERSION(3, 7)
  llvm::DataLayoutPass *TD = NULL;
#endif
#if LLVM_VERSION < VERSION(3, 5)
  const std::string &ModuleDataLayout = module->getDataLayout();
#elif LLVM_VERSION == VERSION(3, 5)
  const std::string &ModuleDataLayout =
      module->getDataLayout()->getStringRepresentation();
#endif
#if LLVM_VERSION < VERSION(3, 2)
  if (!ModuleDataLayout.empty()) {
    TD = new llvm::TargetData(ModuleDataLayout);
  }
#elif LLVM_VERSION < VERSION(3, 5)
  if (!ModuleDataLayout.empty()) {
    TD = new llvm::DataLayout(ModuleDataLayout);
  }
#elif LLVM_VERSION == VERSION(3, 5)
  if (!ModuleDataLayout.empty()) {
    TD = new llvm::DataLayoutPass(llvm::DataLayout(ModuleDataLayout));
  }
#elif LLVM_VERSION < VERSION(3, 7)
  TD = new llvm::DataLayoutPass();
#endif

  // pass manager
#if LLVM_VERSION < VERSION(3, 7)
  llvm::PassManager llvmPasses;
#else
  llvm::legacy::PassManager llvmPasses;
#endif

#if LLVM_VERSION < VERSION(3, 7)
  if (TD != NULL) {
    llvmPasses.add(TD);
  }
#endif

  // first, do some verification of the input code before we modify it
  llvmPasses.add(llvm::createVerifierPass());

  // eliminate calls to bitcast functions
  llvmPasses.add(createBitcastCallEliminatorPass());

  // function inlining
  if (eagerInline) {
    llvmPasses.add(createEagerInlinerPass());
  } else {
    for (unsigned int i = 0; i < numInlines; ++i) {
      llvmPasses.add(createExtremeInlinerPass(function, inlineVoids));
    }
  }

  // mem2reg
  llvmPasses.add(createMem2RegPass(ndf));

  // Hoist
  llvmPasses.add(createHoisterPass());

  // DCE
  llvmPasses.add(llvm::createDeadCodeEliminationPass());

  // simplify cfg
  llvmPasses.add(llvm::createCFGSimplificationPass());

  // DCE
  llvmPasses.add(llvm::createDeadCodeEliminationPass());

  // lower switch to branches
  llvmPasses.add(llvm::createLowerSwitchPass());

  // eliminate constant expressions
  llvmPasses.add(createConstantExprEliminatorPass());

  // strength increasing
  if (increaseStrength) {
    llvmPasses.add(createStrengthIncreaserPass());
  }

  // sort basic blocks
  llvmPasses.add(createBasicBlockSorterPass());

  // Alias analysis
  llvmPasses.add(llvm::createBasicAliasAnalysisPass());

  // lastly, do some verification of the modified code
  llvmPasses.add(llvm::createVerifierPass());

  // run them!
  llvmPasses.run(*module);
}

std::pair<MayMustMap, std::set<llvm::GlobalVariable *>>
getMayMustMap(llvm::Function *function) {
  llvm::Module *module = function->getParent();

#if LLVM_VERSION < VERSION(3, 2)
  llvm::TargetData *TD = NULL;
#elif LLVM_VERSION < VERSION(3, 5)
  llvm::DataLayout *TD = NULL;
#elif LLVM_VERSION < VERSION(3, 7)
  llvm::DataLayoutPass *TD = NULL;
#endif
#if LLVM_VERSION < VERSION(3, 5)
  const std::string &ModuleDataLayout = module->getDataLayout();
#elif LLVM_VERSION == VERSION(3, 5)
  const std::string &ModuleDataLayout =
      module->getDataLayout()->getStringRepresentation();
#endif
#if LLVM_VERSION < VERSION(3, 2)
  if (!ModuleDataLayout.empty()) {
    TD = new llvm::TargetData(ModuleDataLayout);
  }
#elif LLVM_VERSION < VERSION(3, 5)
  if (!ModuleDataLayout.empty()) {
    TD = new llvm::DataLayout(ModuleDataLayout);
  }
#elif LLVM_VERSION == VERSION(3, 5)
  if (!ModuleDataLayout.empty()) {
    TD = new llvm::DataLayoutPass(llvm::DataLayout(ModuleDataLayout));
  }
#elif LLVM_VERSION < VERSION(3, 7)
  TD = new llvm::DataLayoutPass();
#endif

  // pass manager
#if LLVM_VERSION < VERSION(3, 7)
  llvm::FunctionPassManager PM(module);
#else
  llvm::legacy::FunctionPassManager PM(module);
#endif

#if LLVM_VERSION < VERSION(3, 7)
  if (TD != NULL) {
    PM.add(TD);
  }
#endif

  PM.add(llvm::createBasicAliasAnalysisPass());

  MemoryAnalyzer *maPass = createMemoryAnalyzerPass();
  PM.add(maPass);

  PM.run(*function);

  return std::make_pair(maPass->getMayMustMap(), maPass->getMayZap());
}

TrueFalseMap getConditionPropagationMap(llvm::Function *function,
                                        std::set<llvm::BasicBlock *> lcbs) {
  llvm::Module *module = function->getParent();

  // pass manager
#if LLVM_VERSION < VERSION(3, 7)
  llvm::FunctionPassManager PM(module);
#else
  llvm::legacy::FunctionPassManager PM(module);
#endif

  ConditionPropagator *cpPass =
      createConditionPropagatorPass(debug, onlyLoopConditions, lcbs);
  PM.add(cpPass);

  PM.run(*function);

  return cpPass->getTrueFalseMap();
}

ConditionMap getExplicitizedLoopConditionMap(llvm::Function *function) {
  llvm::Module *module = function->getParent();

  // pass manager
#if LLVM_VERSION < VERSION(3, 7)
  llvm::FunctionPassManager PM(module);
#else
  llvm::legacy::FunctionPassManager PM(module);
#endif

  LoopConditionExplicitizer *lcePass =
      createLoopConditionExplicitizerPass(debug);
  PM.add(lcePass);

  PM.run(*function);

  return lcePass->getConditionMap();
}

std::set<llvm::BasicBlock *> getLoopConditionBlocks(llvm::Function *function) {
  llvm::Module *module = function->getParent();

  // pass manager
#if LLVM_VERSION < VERSION(3, 7)
  llvm::FunctionPassManager PM(module);
#else
  llvm::legacy::FunctionPassManager PM(module);
#endif

  LoopConditionBlocksCollector *lcbPass =
      createLoopConditionBlocksCollectorPass();
  PM.add(lcbPass);

  PM.run(*function);

  return lcbPass->getLoopConditionBlocks();
}

std::string getPartNumber(unsigned int currNum, unsigned int maxNum) {
  unsigned int width;
  if (maxNum >= 100) {
    width = 3;
  } else if (maxNum >= 10) {
    width = 2;
  } else {
    width = 1;
  }
  std::ostringstream sstream;
  sstream.fill('0');
  sstream << std::setw(static_cast<int>(width)) << currNum;
  return sstream.str();
}

std::string getSccName(std::list<llvm::Function *> scc) {
  std::ostringstream sstream;
  for (std::list<llvm::Function *>::iterator i = scc.begin(), e = scc.end();
       i != e;) {
    sstream << (*i)->getName().str();
    if (++i != e) {
      sstream << "_";
    }
  }
  return sstream.str();
}

/*
    This pass expands tail recursions with `tail call` into loops.
*/
struct TailRecursionToLoopPass : public llvm::FunctionPass {
  static char ID;
  TailRecursionToLoopPass() : llvm::FunctionPass(ID) {}
  ~TailRecursionToLoopPass() {}

  // Delete the specified instruction and all dependent instructions
  // recursively.
  static void recursivelyDeleteUses(llvm::Instruction *I) {
    if (!I || I->use_empty()) {
      I->eraseFromParent();
      return;
    }

    std::vector<llvm::User *> users(I->user_begin(), I->user_end());

    for (llvm::User *user : users) {
      if (llvm::Instruction *userInst =
              llvm::dyn_cast<llvm::Instruction>(user)) {
        recursivelyDeleteUses(userInst);
      }
    }

    I->eraseFromParent();
  }

  // Replace all occurrences of `oldValue` in `userInst`'s operands with
  // `newValue`.
  static void replaceAllOperandsWith(llvm::Instruction *userInst,
                                     llvm::Value *oldValue,
                                     llvm::Value *newValue) {
    for (unsigned i = 0; i < userInst->getNumOperands(); ++i) {
      if (userInst->getOperand(i) == oldValue) {
        userInst->setOperand(i, newValue);
      }
    }
  }

  virtual bool runOnFunction(llvm::Function &F) {
    // std::cout << "runOnFunction: " << F.getName().str() << "\n";
    bool modified = false;

    // An IRBuilder is used to insert instructions before CI.
    llvm::IRBuilder<> builder(F.getContext());

    for (llvm::BasicBlock &BB : F) {
      // Search the `tail call` which is tail recursion.
      auto it_search = BB.begin();
      while (it_search != BB.end()) {
        if (llvm::CallInst *CI = llvm::dyn_cast<llvm::CallInst>(it_search)) {
          if (CI->isTailCall() && CI->getCalledFunction() == &F) {
            break;
          }
        }

        it_search++;
      }

      if (it_search == BB.end()) {
        continue;
      }

      // std::cout << "Found tail recursive call in function: " <<
      // F.getName().str() << "\n";

      /*
          Replace `tail call` with `br` to entry-block.
          However, LLVM-IR does not permit a function's entry block to have any
         predecessors. Therefore, a new block is inserted before the current
         entry block as a "dummy_entry".
      */
      llvm::BasicBlock *entry_block = &F.getEntryBlock();
      llvm::BasicBlock *dummy_entry = llvm::BasicBlock::Create(
          F.getContext(), "dummy_entry", &F, entry_block);
      llvm::BranchInst::Create(entry_block, dummy_entry);

      /*
          Overwrite the original function's arguments with those from the `tail
         call`:
          1. Allocate memory to store the new argument values.
          2. Replace all uses of the original arguments with values loaded from
         this memory.
          3. Store the arguments from the `tail call` into the allocated memory.
      */
      std::vector<llvm::AllocaInst *> argAllocas;
      for (llvm::Argument &arg : F.args()) {
        // Allocate memory to store the new argument values.
        builder.SetInsertPoint(dummy_entry->getFirstInsertionPt());
        llvm::AllocaInst *allocaInst =
            builder.CreateAlloca(arg.getType(), nullptr, "arg_alloca");
        llvm::StoreInst *storeInst = builder.CreateStore(&arg, allocaInst);

        // Replace all uses of the original arguments with values loaded from
        // this memory.
        for (llvm::User *user : arg.users()) {
          if (llvm::Instruction *userInst =
                  llvm::dyn_cast<llvm::Instruction>(user)) {
            if (userInst != storeInst) {
              builder.SetInsertPoint(userInst);
              llvm::LoadInst *loadInst =
                  builder.CreateLoad(allocaInst, "arg_load");
              replaceAllOperandsWith(userInst, static_cast<llvm::Value *>(&arg),
                                     static_cast<llvm::Value *>(loadInst));
            }
          }
        }

        argAllocas.push_back(allocaInst);
      }

      // Store the arguments from the `tail call` into the allocated memory.
      llvm::CallInst *CI = llvm::dyn_cast<llvm::CallInst>(it_search);
      builder.SetInsertPoint(CI);

      // Store the new argument values to the allocas.
      for (unsigned i = 0; i < CI->getNumArgOperands(); ++i) {
        builder.CreateStore(CI->getArgOperand(i), argAllocas[i]);
      }

      // Delete the `tail call` and all instructions that follow it within the
      // same basic block.
      std::vector<llvm::Instruction *> v;
      for (auto it = it_search; it != BB.end(); it++) {
        v.push_back(it);
      }

      for (auto it_delete = v.rbegin(); it_delete != v.rend();) {
        auto to_delete = it_delete;
        it_delete++;
        recursivelyDeleteUses(*to_delete);
      }

      // Insert `br` to original entry block.
      llvm::BranchInst::Create(entry_block, &BB);

      modified = true;
      break;
    }
    return modified;
  }
};

char TailRecursionToLoopPass::ID = 0;

// Register the pass with the legacy pass manager
static llvm::RegisterPass<TailRecursionToLoopPass>
    X("tailrec-to-loop", "Tail Recursion to Loop Conversion Pass");

// Provide a factory function for opt to load the pass
extern "C" llvm::Pass *createTailRecursionToLoopPass() {
  return new TailRecursionToLoopPass();
}

int main(int argc, char *argv[]) {
  cl::SetVersionPrinter(&versionPrinter);
  cl::ParseCommandLineOptions(argc, argv, "llvm2kittel\n");

  if (boundedIntegers && divisionConstraintType == Exact) {
    std::cerr << "Cannot use \"-division-constraint=exact\" in combination "
                 "with \"-bounded-integers\""
              << std::endl;
    return 333;
  }
  if (!boundedIntegers && unsignedEncoding) {
    std::cerr
        << "Cannot use \"-unsigned-encoding\" without \"-bounded-integers\""
        << std::endl;
    return 333;
  }
  if (!boundedIntegers && bitwiseConditions) {
    std::cerr
        << "Cannot use \"-bitwise-conditions\" without \"-bounded-integers\""
        << std::endl;
    return 333;
  }
  if (numInlines != 0 && eagerInline) {
    std::cerr << "Cannot use \"-inline\" in combination with \"-eager-inline\""
              << std::endl;
    return 333;
  }

#if LLVM_VERSION < VERSION(3, 5)
  llvm::OwningPtr<llvm::MemoryBuffer> owningBuffer;
  llvm::MemoryBuffer::getFileOrSTDIN(filename, owningBuffer);
  llvm::MemoryBuffer *buffer = owningBuffer.get();
#else
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> owningBuffer =
      llvm::MemoryBuffer::getFileOrSTDIN(filename);
  llvm::MemoryBuffer *buffer = NULL;
  if (!owningBuffer.getError()) {
    buffer = owningBuffer->get();
  }
#endif

  if (buffer == NULL) {
    std::cerr << "LLVM bitcode file \"" << filename
              << "\" does not exist or cannot be read." << std::endl;
    return 1;
  }

  llvm::LLVMContext context;
  std::string errMsg;
#if LLVM_VERSION < VERSION(3, 5)
  llvm::Module *module = llvm::ParseBitcodeFile(buffer, context, &errMsg);
#elif LLVM_VERSION == VERSION(3, 5)
  llvm::Module *module = NULL;
  llvm::ErrorOr<llvm::Module *> moduleOrError =
      llvm::parseBitcodeFile(buffer, context);
  std::error_code ec = moduleOrError.getError();
  if (ec) {
    errMsg = ec.message();
  } else {
    module = moduleOrError.get();
  }
#elif LLVM_VERSION == VERSION(3, 6)
  llvm::Module *module = NULL;
  llvm::ErrorOr<llvm::Module *> moduleOrError =
      llvm::parseBitcodeFile(buffer->getMemBufferRef(), context);
  std::error_code ec = moduleOrError.getError();
  if (ec) {
    errMsg = ec.message();
  } else {
    module = moduleOrError.get();
  }
#else
  llvm::Module *module = NULL;
  llvm::ErrorOr<std::unique_ptr<llvm::Module>> moduleOrError =
      llvm::parseBitcodeFile(buffer->getMemBufferRef(), context);
  std::error_code ec = moduleOrError.getError();
  if (ec) {
    errMsg = ec.message();
  } else {
    module = moduleOrError->get();
  }
#endif

  // check if the file is a proper bitcode file and contains a module
  if (module == NULL) {
    std::cerr << "LLVM bitcode file doesn't contain a valid module."
              << std::endl;
    return 2;
  }

  llvm::Function *function = NULL;
  llvm::Function *firstFunction = NULL;
  unsigned int numFunctions = 0;
  std::list<std::string> functionNames;

  for (llvm::Module::iterator i = module->begin(), e = module->end(); i != e;
       ++i) {
    if (i->getName() == functionname) {
      function = i;
      break;
    } else if (functionname.empty() && i->getName() == "main") {
      function = i;
      break;
    } else if (!i->isDeclaration()) {
      ++numFunctions;
      functionNames.push_back(i->getName());
      if (firstFunction == NULL) {
        firstFunction = i;
      }
    }
  }

  if (function == NULL) {
    if (numFunctions == 0) {
      std::cerr << "Module does not contain any function." << std::endl;
      return 3;
    }
    if (functionname.empty()) {
      if (numFunctions == 1) {
        function = firstFunction;
      } else {
        std::cerr << "LLVM module contains more than one function:"
                  << std::endl;
        for (std::list<std::string>::iterator i = functionNames.begin(),
                                              e = functionNames.end();
             i != e; ++i) {
          std::cerr << "    " << *i << std::endl;
        }
        std::cerr << "Please specify which function should be used."
                  << std::endl;
        return 4;
      }
    } else {
      std::cerr << "Specified function not found." << std::endl;
      std::cerr << "Candidates are:" << std::endl;
      for (std::list<std::string>::iterator i = functionNames.begin(),
                                            e = functionNames.end();
           i != e; ++i) {
        std::cerr << "    " << *i << std::endl;
      }
      return 5;
    }
  }

  // llvm::PassManager tailrecToLoopPass;
  // tailrecToLoopPass.add(createTailRecursionToLoopPass());
  // NondefFactory nondef_factory(module);
  // tailrecToLoopPass.add(createMem2RegPass(nondef_factory));
  // tailrecToLoopPass.run(*module);

  // std::string outFile = filename.substr(0, filename.length() - 3) +
  // "_tailRecursionToLoop.ll"; llvm::raw_fd_ostream stream(outFile.data(), ec,
  // llvm::sys::fs::F_Text); if (ec) {
  //     std::cerr << "Cannot open the test file: " << outFile << std::endl;
  // }
  // stream << *module << '\n';
  // stream.close();

  // check for cyclic call hierarchies
  HierarchyBuilder checkHierarchy;
  checkHierarchy.computeHierarchy(module);
  if (eagerInline && checkHierarchy.isCyclic()) {
    std::cerr << "Cannot use \"-eager-inline\" with a cyclic call hierarchy!"
              << std::endl;
    return 7;
  }

  // transform!
  NondefFactory ndf(module);
  transformModule(module, function, ndf);

  // name them!
  InstNamer namer;
  namer.visit(module);

  // If requested, output mapping from LLVM values to C variable names and exit
  if (llvmVarToC) {
    printLLVMVarToC(module);
    return 0;
  }

  // If requested, output per-BasicBlock source info as CSV and exit
  if (sourceInfo) {
    printSourceInfo(module);
    return 0;
  }

  // check them!
  const llvm::Type *boolType = llvm::Type::getInt1Ty(context);
  const llvm::Type *floatType = llvm::Type::getFloatTy(context);
  const llvm::Type *doubleType = llvm::Type::getDoubleTy(context);
  InstChecker checker(boolType, floatType, doubleType);
  checker.visit(module);

  // print it!
  if (debug) {
#if LLVM_VERSION < VERSION(3, 5)
    llvm::PassManager printPass;
    printPass.add(llvm::createPrintModulePass(&llvm::outs()));
    printPass.run(*module);
#else
    llvm::outs() << *module << '\n';
#endif
  }
  if (dumpLL) {
    std::string outFile = filename.substr(0, filename.length() - 3) + ".ll";
#if LLVM_VERSION < VERSION(3, 5)
    std::string errorInfo;
    llvm::raw_fd_ostream stream(outFile.data(), errorInfo);
    if (errorInfo.empty()) {
      llvm::PassManager dumpPass;
      dumpPass.add(llvm::createPrintModulePass(&stream));
      dumpPass.run(*module);
      stream.close();
    }
#elif LLVM_VERSION == VERSION(3, 5)
    std::string errorInfo;
    llvm::raw_fd_ostream stream(outFile.data(), errorInfo,
                                llvm::sys::fs::F_Text);
    if (errorInfo.empty()) {
      stream << *module << '\n';
      stream.close();
    }
#else
    std::error_code errorCode;
    llvm::raw_fd_ostream stream(outFile.data(), errorCode,
                                llvm::sys::fs::F_Text);
    if (!errorCode) {
      stream << *module << '\n';
      stream.close();
    }
#endif
  }

  // check for junk
  std::list<llvm::Instruction *> unsuitable = checker.getUnsuitableInsts();
  if (!unsuitable.empty()) {
    std::cerr << "Unsuitable instructions detected:" << std::endl;
    for (std::list<llvm::Instruction *>::iterator i = unsuitable.begin(),
                                                  e = unsuitable.end();
         i != e; ++i) {
      (*i)->dump();
    }
    return 6;
  }

  // compute recursion hierarchy
  HierarchyBuilder hierarchy;
  hierarchy.computeHierarchy(module);
  std::list<std::list<llvm::Function *>> sccs = hierarchy.getSccs();

  std::map<llvm::Function *, std::list<llvm::Function *>> funToScc;
  for (std::list<std::list<llvm::Function *>>::iterator i = sccs.begin(),
                                                        e = sccs.end();
       i != e; ++i) {
    std::list<llvm::Function *> scc = *i;
    for (std::list<llvm::Function *>::iterator si = scc.begin(), se = scc.end();
         si != se; ++si) {
      funToScc.insert(std::make_pair(*si, scc));
    }
  }
  std::list<llvm::Function *> dependsOnList =
      hierarchy.getTransitivelyCalledFunctions(function);
  std::set<llvm::Function *> dependsOn;
  dependsOn.insert(dependsOnList.begin(), dependsOnList.end());
  dependsOn.insert(function);
  std::list<std::list<llvm::Function *>> dependsOnSccs;
  for (std::list<std::list<llvm::Function *>>::iterator i = sccs.begin(),
                                                        e = sccs.end();
       i != e; ++i) {
    std::list<llvm::Function *> scc = *i;
    for (std::list<llvm::Function *>::iterator fi = scc.begin(), fe = scc.end();
         fi != fe; ++fi) {
      llvm::Function *f = *fi;
      if (dependsOn.find(f) != dependsOn.end()) {
        dependsOnSccs.push_back(scc);
        break;
      }
    }
  }

  // compute may/must info, propagated conditions, and compute loop exiting
  // blocks for all functions
  std::map<llvm::Function *, MayMustMap> mmMap;
  std::map<llvm::Function *, std::set<llvm::GlobalVariable *>> funcMayZapDirect;
  std::map<llvm::Function *, TrueFalseMap> tfMap;
  std::map<llvm::Function *, std::set<llvm::BasicBlock *>> lebMap;
  std::map<llvm::Function *, ConditionMap> elcMap;
  for (std::set<llvm::Function *>::iterator df = dependsOn.begin(),
                                            dfe = dependsOn.end();
       df != dfe; ++df) {
    llvm::Function *func = *df;
    std::pair<MayMustMap, std::set<llvm::GlobalVariable *>> tmp =
        getMayMustMap(func);
    mmMap.insert(std::make_pair(func, tmp.first));
    funcMayZapDirect.insert(std::make_pair(func, tmp.second));
    std::set<llvm::BasicBlock *> lcbs;
    if (onlyLoopConditions) {
      lcbs = getLoopConditionBlocks(func);
      lebMap.insert(std::make_pair(func, lcbs));
    }
    if (propagateConditions) {
      tfMap.insert(
          std::make_pair(func, getConditionPropagationMap(func, lcbs)));
    }
    if (explicitizeLoopConditions) {
      elcMap.insert(
          std::make_pair(func, getExplicitizedLoopConditionMap(func)));
    }
  }

  // transitively close funcMayZapDirect
  std::map<llvm::Function *, std::set<llvm::GlobalVariable *>> funcMayZap;
  for (std::set<llvm::Function *>::iterator df = dependsOn.begin(),
                                            dfe = dependsOn.end();
       df != dfe; ++df) {
    llvm::Function *func = *df;
    std::set<llvm::GlobalVariable *> funcTransZap;
    std::list<llvm::Function *> funcDependsOnList =
        hierarchy.getTransitivelyCalledFunctions(func);
    std::set<llvm::Function *> funcDependsOn;
    funcDependsOn.insert(funcDependsOnList.begin(), funcDependsOnList.end());
    funcDependsOn.insert(func);
    for (std::set<llvm::Function *>::iterator depfi = funcDependsOn.begin(),
                                              depfe = funcDependsOn.end();
         depfi != depfe; ++depfi) {
      llvm::Function *depf = *depfi;
      std::map<llvm::Function *, std::set<llvm::GlobalVariable *>>::iterator
          depfZap = funcMayZapDirect.find(depf);
      if (depfZap == funcMayZapDirect.end()) {
        std::cerr << "Could not find alias information (" << __FILE__ << ":"
                  << __LINE__ << ")!" << std::endl;
        exit(9876);
      }
      funcTransZap.insert(depfZap->second.begin(), depfZap->second.end());
    }
    funcMayZap.insert(std::make_pair(func, funcTransZap));
  }

  // convert sccs separately
  unsigned int num = static_cast<unsigned int>(dependsOnSccs.size());
  unsigned int currNum = 0;
  for (std::list<std::list<llvm::Function *>>::iterator
           scci = dependsOnSccs.begin(),
           scce = dependsOnSccs.end();
       scci != scce; ++scci) {
    std::list<llvm::Function *> scc = *scci;
    std::list<ref<Rule>> allRules;
    std::list<ref<Rule>> allCondensedRules;
    std::list<ref<Rule>> allKittelizedRules;
    std::list<ref<Rule>> allSlicedRules;
    if (debug) {
      std::cout << "========================================" << std::endl;
    }
    if ((!complexityTuples && !uniformComplexityTuples) || debug) {
      std::cout << "///*** " << getPartNumber(++currNum, num) << '_'
                << getSccName(scc) << " ***///" << std::endl;
    }
    std::set<llvm::Function *> sccSet;
    sccSet.insert(scc.begin(), scc.end());

    std::set<std::string> complexityLHSs;

    for (std::list<llvm::Function *>::iterator fi = scc.begin(), fe = scc.end();
         fi != fe; ++fi) {
      llvm::Function *curr = *fi;
      std::string t2Filename =
          filename.substr(0, filename.length() - 3) + ".t2";
      //<Negar>
      // Converter converter(boolType, assumeIsControl, selectIsControl,
      // onlyMultiPredIsControl, boundedIntegers, unsignedEncoding,
      // onlyLoopConditions, divisionConstraintType, bitwiseConditions,
      // complexityTuples || uniformComplexityTuples, t2Output);
      Converter converter(
          boolType, assumeIsControl, selectIsControl, onlyMultiPredIsControl,
          boundedIntegers, unsignedEncoding, onlyLoopConditions,
          divisionConstraintType, bitwiseConditions,
          complexityTuples || uniformComplexityTuples, t2Output, signednessInfo,
          nondetTypeInfo, unreachableExit, ignoreReachError);
      //</Negar>
      std::map<llvm::Function *, MayMustMap>::iterator tmp1 = mmMap.find(curr);
      if (tmp1 == mmMap.end()) {
        std::cerr << "Could not find alias information (" << __FILE__ << ":"
                  << __LINE__ << ")!" << std::endl;
        exit(9876);
      }
      MayMustMap curr_mmMap = tmp1->second;
      std::map<llvm::Function *, TrueFalseMap>::iterator tmp2 =
          tfMap.find(curr);
      TrueFalseMap curr_tfMap;
      if (tmp2 != tfMap.end()) {
        curr_tfMap = tmp2->second;
      }
      std::map<llvm::Function *, std::set<llvm::BasicBlock *>>::iterator tmp3 =
          lebMap.find(curr);
      std::set<llvm::BasicBlock *> curr_leb;
      if (tmp3 != lebMap.end()) {
        curr_leb = tmp3->second;
      }
      std::map<llvm::Function *, ConditionMap>::iterator tmp4 =
          elcMap.find(curr);
      ConditionMap curr_elcMap;
      if (tmp4 != elcMap.end()) {
        curr_elcMap = tmp4->second;
      }
      converter.phase1(curr, sccSet, curr_mmMap, funcMayZap, curr_tfMap,
                       curr_leb, curr_elcMap);
      converter.phase2(curr, sccSet, curr_mmMap, funcMayZap, curr_tfMap,
                       curr_leb, curr_elcMap);

      if (!t2Output) {
        std::list<ref<Rule>> rules = converter.getRules();
        std::list<ref<Rule>> condensedRules = converter.getCondensedRules();
        std::list<ref<Rule>> kittelizedRules =
            kittelize(condensedRules, smtSolver);
        Slicer slicer(curr, converter.getPhiVariables());
        std::list<ref<Rule>> slicedRules;
        if (noSlicing) {
          slicedRules = kittelizedRules;
        } else {
          slicedRules = slicer.sliceUsage(kittelizedRules);
          slicedRules = slicer.sliceConstraint(slicedRules);
          slicedRules = slicer.sliceDefined(slicedRules);
          slicedRules = slicer.sliceStillUsed(slicedRules, conservativeSlicing);
          slicedRules = slicer.sliceDuplicates(slicedRules);
        }
        if (boundedIntegers) {
          //<Negar>
          // slicedRules = kittelize(addBoundConstraints(slicedRules,
          // converter.getBitwidthMap(), unsignedEncoding), smtSolver);
          slicedRules = kittelize(
              addBoundConstraints(slicedRules, converter.getBitwidthMap(),
                                  unsignedEncoding, signednessInfo),
              smtSolver);
          //</Negar>
        }
        if (debug) {
          allRules.insert(allRules.end(), rules.begin(), rules.end());
          allCondensedRules.insert(allCondensedRules.end(),
                                   condensedRules.begin(),
                                   condensedRules.end());
          allKittelizedRules.insert(allKittelizedRules.end(),
                                    kittelizedRules.begin(),
                                    kittelizedRules.end());
        }
        if (simplifyConds) {
          slicedRules = simplifyConstraints(slicedRules);
        }
        allSlicedRules.insert(allSlicedRules.end(), slicedRules.begin(),
                              slicedRules.end());

        if (complexityTuples || uniformComplexityTuples) {
          std::set<std::string> tmpLHSs = converter.getComplexityLHSs();
          complexityLHSs.insert(tmpLHSs.begin(), tmpLHSs.end());
        }
      }
    }
    if (!t2Output) {
      if (debug) {
        std::cout << "========================================" << std::endl;
        for (std::list<ref<Rule>>::iterator i = allRules.begin(),
                                            e = allRules.end();
             i != e; ++i) {
          ref<Rule> tmp = *i;
          std::cout << tmp->toString() << std::endl;
        }
        std::cout << "========================================" << std::endl;
        for (std::list<ref<Rule>>::iterator i = allCondensedRules.begin(),
                                            e = allCondensedRules.end();
             i != e; ++i) {
          ref<Rule> tmp = *i;
          std::cout << tmp->toString() << std::endl;
        }
        std::cout << "========================================" << std::endl;
        for (std::list<ref<Rule>>::iterator i = allKittelizedRules.begin(),
                                            e = allKittelizedRules.end();
             i != e; ++i) {
          ref<Rule> tmp = *i;
          std::cout << tmp->toString() << std::endl;
        }
        std::cout << "========================================" << std::endl;
      }
      if (complexityTuples) {
        printComplexityTuples(allSlicedRules, complexityLHSs, std::cout);
      } else if (uniformComplexityTuples) {
        std::ostringstream startfun;
        startfun << "eval_" << getSccName(scc) << "_start";
        std::string name = startfun.str();
        printUniformComplexityTuples(allSlicedRules, complexityLHSs, name,
                                     std::cout);
      } else {
        for (std::list<ref<Rule>>::iterator i = allSlicedRules.begin(),
                                            e = allSlicedRules.end();
             i != e; ++i) {
          ref<Rule> tmp = *i;
          std::cout << tmp->toKittelString() << std::endl;
        }
      }
    }
  }

  return 0;
}

void printSourceInfo(llvm::Module *module){
// CSV header: include bb index and optional file/dir
    std::cout << "function,bb_index,bb_name,line,column,op" << std::endl;
    for (llvm::Module::iterator fi = module->begin(), fe = module->end();
         fi != fe; ++fi) {
      llvm::Function &F = *fi;
      if (F.isDeclaration())
        continue;
      unsigned bb_index = 0;
      for (llvm::Function::iterator bbi = F.begin(), bbe = F.end(); bbi != bbe;
           ++bbi, ++bb_index) {
        llvm::BasicBlock &BB = *bbi;
        unsigned line = 0;
        unsigned col = 0;
        std::string file;
        std::string dir;
        for (llvm::BasicBlock::iterator ii = BB.begin(), ie = BB.end(); ii != ie;
             ++ii) {
          llvm::Instruction &I = *ii;
          // Prefer DebugLoc API when available
          llvm::DebugLoc dbg = I.getDebugLoc();
          if (!dbg.isUnknown()) {
            line = dbg.getLine();
            col = dbg.getCol();
            if (llvm::MDNode *scope = dbg.getScope()) {
              // Use DILocation to extract filename/directory in a compatible way
              llvm::DILocation Loc(scope);
              file = Loc.getFilename().str();
              dir = Loc.getDirectory().str();
            } else if (llvm::MDNode *md = dbg.getAsMDNode()) {
              llvm::DILocation Loc(md);
              file = Loc.getFilename().str();
              dir = Loc.getDirectory().str();
            }
          }
          // fallback to metadata node
          else if (llvm::MDNode *N = I.getMetadata("dbg")) {
            llvm::DILocation Loc(N);
            line = Loc.getLineNumber();
            col = Loc.getColumnNumber();
            file = Loc.getFilename().str();
            dir = Loc.getDirectory().str();
          }

		std::string bb_name = BB.getName().str();
		// if (bb_name.empty()) {
		//   std::ostringstream tmp;
		//   tmp << "bb_" << bb_index;
		//   bb_name = tmp.str();
		// }

      std::cout << F.getName().str() << "," << bb_index << "," << bb_name
        << "," << line << "," << col << "," << I.getOpcodeName()
		<<std::endl;
        }
      }
    }
}

void printLLVMVarToC(llvm::Module *module) {
  auto valueToString = [&](const llvm::Value *v) {
    std::string s;
    llvm::raw_string_ostream rso(s);
    v->print(rso);
    rso.flush();
    return s;
  };

  auto instToString = [&](const llvm::Instruction *I) {
    std::string s;
    llvm::raw_string_ostream rso(s);
    I->print(rso);
    rso.flush();
    return s;
  };

  std::cout << "function,bb_index,bb_name,ir_name,source_name" << std::endl;

  std::map<const llvm::Value *, std::string> valToSrc;
  std::map<std::string, std::string> reprToSrc;
  std::set<const llvm::Value *> printedVals;

  // First pass: collect dbg.declare / dbg.value (intrinsic and call forms)
  for (llvm::Module::iterator fi = module->begin(), fe = module->end(); fi != fe; ++fi) {
    llvm::Function &F = *fi;
    if (F.isDeclaration()) continue;
    unsigned bb_index = 0;
    for (llvm::Function::iterator bbi = F.begin(), bbe = F.end(); bbi != bbe; ++bbi, ++bb_index) {
      llvm::BasicBlock &BB = *bbi;
      for (llvm::BasicBlock::iterator ii = BB.begin(), ie = BB.end(); ii != ie; ++ii) {
        llvm::Instruction &I = *ii;
        if (llvm::DbgDeclareInst *DDI = llvm::dyn_cast<llvm::DbgDeclareInst>(&I)) {
          if (llvm::MDNode *varMD = DDI->getVariable()) {
            llvm::DIVariable var(varMD);
            std::string srcName = var.getName().str();
            if (llvm::Value *addr = DDI->getAddress()) {
              std::string ir_repr = valueToString(addr);
              // record mapping but DO NOT print dbg.declare lines
              valToSrc[addr] = srcName;
              reprToSrc[ir_repr] = srcName;
              printedVals.insert(addr);
            }
          }
        } else if (llvm::DbgValueInst *DVI = llvm::dyn_cast<llvm::DbgValueInst>(&I)) {
          if (llvm::MDNode *varMD = DVI->getVariable()) {
            llvm::DIVariable var(varMD);
            std::string srcName = var.getName().str();
            if (llvm::Value *v = DVI->getValue()) {
              std::string ir_repr = valueToString(v);
              // record mapping but DO NOT print dbg.value lines
              valToSrc[v] = srcName;
              reprToSrc[ir_repr] = srcName;
            }
          }
        } else if (llvm::CallInst *CI = llvm::dyn_cast<llvm::CallInst>(&I)) {
          if (llvm::Function *CF = CI->getCalledFunction()) {
            llvm::StringRef cname = CF->getName();
            if (cname == "llvm.dbg.value") {
              if (CI->getNumArgOperands() >= 3) {
                if (llvm::MetadataAsValue *mav = llvm::dyn_cast<llvm::MetadataAsValue>(CI->getArgOperand(2))) {
                  if (llvm::Metadata *md = mav->getMetadata()) {
                    if (llvm::MDNode *varMD = llvm::dyn_cast<llvm::MDNode>(md)) {
                      llvm::DIVariable var(varMD);
                      std::string srcName = var.getName().str();
                      std::string ir_repr;
                      if (llvm::MetadataAsValue *valm = llvm::dyn_cast<llvm::MetadataAsValue>(CI->getArgOperand(0))) {
                        std::string s; llvm::raw_string_ostream rso(s); valm->print(rso); rso.flush(); ir_repr = s;
                      } else {
                        ir_repr = instToString(&I);
                      }
                      // record mapping but DO NOT print dbg.value(call) lines
                      reprToSrc[ir_repr] = srcName;
                    }
                  }
                }
              }
            } else if (cname == "llvm.dbg.declare") {
              if (CI->getNumArgOperands() >= 2) {
                if (llvm::MetadataAsValue *mav = llvm::dyn_cast<llvm::MetadataAsValue>(CI->getArgOperand(1))) {
                  if (llvm::Metadata *md = mav->getMetadata()) {
                    if (llvm::MDNode *varMD = llvm::dyn_cast<llvm::MDNode>(md)) {
                      llvm::DIVariable var(varMD);
                      std::string srcName = var.getName().str();
                      std::string ir_repr = instToString(&I);
                      // record mapping but DO NOT print dbg.declare(call) lines
                      reprToSrc[ir_repr] = srcName;
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  // Second pass: simple PHI inference using exact Value* or repr match
  for (llvm::Module::iterator fi = module->begin(), fe = module->end(); fi != fe; ++fi) {
    llvm::Function &F = *fi;
    if (F.isDeclaration()) continue;
    unsigned bb_index = 0;
    for (llvm::Function::iterator bbi = F.begin(), bbe = F.end(); bbi != bbe; ++bbi, ++bb_index) {
      llvm::BasicBlock &BB = *bbi;
      for (llvm::BasicBlock::iterator ii = BB.begin(), ie = BB.end(); ii != ie; ++ii) {
        if (llvm::PHINode *PN = llvm::dyn_cast<llvm::PHINode>(&*ii)) {
          if (!PN->hasName()) continue;
          if (printedVals.find(PN) != printedVals.end()) continue;
          bool matched = false;
          std::string srcName;
          for (unsigned k = 0; k < PN->getNumIncomingValues(); ++k) {
            llvm::Value *inc = PN->getIncomingValue(k);
            auto itv = valToSrc.find(inc);
            if (itv != valToSrc.end()) { matched = true; srcName = itv->second; break; }
            std::string inc_repr = valueToString(inc);
            auto itr = reprToSrc.find(inc_repr);
            if (itr != reprToSrc.end()) { matched = true; srcName = itr->second; break; }
          }
          if (matched) {
            std::string ir_name = PN->getName().str();
            std::string ir_repr = instToString(PN);
            std::string inst_repr = instToString(PN);
            std::cout << F.getName().str() << "," << bb_index << "," << BB.getName().str()
                      << "," << ir_name << "," << srcName << std::endl;
            printedVals.insert(PN);
          }
        }
        }
      }
    }
}