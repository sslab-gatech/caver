#include "llvm/Transforms/Instrumentation.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/CallGraphSCCPass.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>
using namespace llvm;

#define DEBUG_TYPE "cver-prune-stack"

STATISTIC(NumPrunned, "Pruned Functions");
STATISTIC(NumNonPrunned, "Non-prunned Functions");

#define CVER_DEBUG(stmt)                        \
  do {                                          \
    if (ClDebug) {                              \
      llvm::errs() << stmt;                     \
    }                                           \
  } while(0)


static cl::opt<bool> ClDisable(
  "disable-cver-stack-prune",
  cl::desc("Disable CVER stack prunning"),
  cl::Hidden,cl::init(false));

static cl::opt<bool> ClDebug(
  "cver-stack-prune-debug",
  cl::desc("Debug CVER stack prunning"),
  cl::Hidden, cl::init(false));

namespace {
  struct CverPruneStack : public CallGraphSCCPass {
    static char ID; // Pass identification, replacement for typeid
    CverPruneStack() : CallGraphSCCPass(ID) {
      initializeCverPruneStackPass(*PassRegistry::getPassRegistry());
    }

    const char *sStackEnter = "__cver_handle_stack_enter";
    const char *sStackExit = "__cver_handle_stack_exit";    
    const char *sCast = "__cver_handle_cast";
    
    bool PruneWithSCCCallGraph(CallGraphSCC &SCC);
    bool PruneWithDepthFirstSearch(CallGraphSCC &SCC);
    bool runOnSCC(CallGraphSCC &SCC) override;
    bool IsInvokeStack(Function *F, CallGraph &CG);
    bool IsAnyCallesInvokeCast(Function *F, CallGraph &CG,
                               SmallSet<Function *, 32> &Visited, int depth);

    bool CollectStackInvokeInstructions(
      Function *F, SmallVector<Instruction *, 16> &StackInvokes);
  };
}

char CverPruneStack::ID = 0;
INITIALIZE_PASS_BEGIN(CverPruneStack, "cver-prune-stack",
                "Prunning stack traces for CastVerifier", false, false)
INITIALIZE_PASS_DEPENDENCY(CallGraphWrapperPass)
INITIALIZE_PASS_END(CverPruneStack, "cver-prune-stack",
                "Prunning stack traces for CastVerifier", false, false)

Pass *llvm::createCverPruneStackPass() {
  return new CverPruneStack();
}

bool CverPruneStack::IsInvokeStack(Function *F, CallGraph &CG) {
  CallGraphNode *CGNode = CG[F];

  for (CallGraphNode::iterator I = CGNode->begin(); I != CGNode->end(); ++I) {
    Function *calleeFunction = I->second->getFunction();
    if (calleeFunction && calleeFunction->getName() == sStackEnter)
      return true;
  }
  return false;
}

bool CverPruneStack::IsAnyCallesInvokeCast(
  Function *F, CallGraph &CG, SmallSet<Function *, 32> &Visited, int depth) {


  CVER_DEBUG("\t Analyzing " << F->getName() << "\n");
  // static int maxDepth = 500;
  // if (depth >= maxDepth) {
  //   return true;
  // }

  assert(!Visited.count(F));
  Visited.insert(F);
  
  CallGraphNode *CGNode = CG[F];

  bool mayCast = false;
  for (CallGraphNode::iterator I = CGNode->begin(); I != CGNode->end(); ++I) {
    Function *calleeFunction = I->second->getFunction();
    if (!calleeFunction)
      continue;
    
    if (calleeFunction->getName() == sCast)
      return true;

    if (!Visited.count(calleeFunction))
      mayCast |= IsAnyCallesInvokeCast(calleeFunction, CG, Visited, depth+1);

    // If the callee may cast, then simply return true.
    if (mayCast)
      return true;
  }
  return mayCast;
}

bool CverPruneStack::PruneWithDepthFirstSearch(CallGraphSCC &SCC) {
  bool isModified = false;

  SmallPtrSet<CallGraphNode *, 8> SCCNodes;
  CallGraph &CG = getAnalysis<CallGraphWrapperPass>().getCallGraph();

  SmallVector<Instruction *, 16> InstToDelete;

  CVER_DEBUG("-----------------------------------\n");
  
  // First, check each function whether it has static_cast.
  for (CallGraphSCC::iterator I = SCC.begin(), E = SCC.end(); I != E; ++I) {
    Function *F = (*I)->getFunction();
    if (!F)
      continue;

    CVER_DEBUG("[*] F : " << F->getName() << "\n");
    
    // This function does not invoke stack_enter or stack_exit, so not our
    // interests.
    if (!IsInvokeStack(F, CG)) {
      CVER_DEBUG("\t Skip " << F->getName() << "as there's no stack funcs\n");
      NumNonPrunned++;      
      continue;
    }
    
    SmallSet<Function *, 32> Visited;
    bool mayCast = IsAnyCallesInvokeCast(F, CG, Visited, 0);
    CVER_DEBUG("\t mayCast : " << mayCast << "\n");
    
    if (!mayCast) {
      // Implies all of callees of this function never invoke
      // __cver_handle_cast.
      CollectStackInvokeInstructions(F, InstToDelete);
      NumPrunned++;        
      isModified = true;
    } else {
      NumNonPrunned++;
    }

  }

  for (Instruction *inst : InstToDelete) {
    CVER_DEBUG( "(prunning) : " << *inst << "\n");
    inst->eraseFromParent();
  }
  
  return isModified;
}

bool CverPruneStack::CollectStackInvokeInstructions(
  Function *F, SmallVector<Instruction *, 16> &StackInvokes) {
  
  bool isCollected = false;  
  for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB) 
    for (BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; ++I)
      if (CallInst *CI = dyn_cast<CallInst>(I)) {
        Function *Callee = CI->getCalledFunction();
        
        if (!Callee)
          continue;
          
        if (Callee->getName() == sStackEnter ||
            Callee->getName() == sStackExit) {

          CVER_DEBUG("[*] Prunning " << Callee->getName() << " in " <<
                     F->getName() << "\n");
          StackInvokes.push_back(I);
          isCollected = true;
        }
      }
  return isCollected;
}

bool CverPruneStack::PruneWithSCCCallGraph(CallGraphSCC &SCC) {
  
  CVER_DEBUG("-----------------------------------\n");

  bool mayCallCast = false;
  for (CallGraphSCC::iterator I = SCC.begin(), E = SCC.end(); I != E; ++I) {
    Function *F = (*I)->getFunction();
    if (!F)
      continue;

    CVER_DEBUG(" F : " << F->getName() << "\n");
    
    // Check if this function may call __cver_handle_stack_enter
    for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB) {
      for (BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; ++I) {
        if (CallInst *CI = dyn_cast<CallInst>(I)) {
          Function *Callee = CI->getCalledFunction();
          if (!Callee) {
            CVER_DEBUG("\t mayCallCast due to indirect calls\n");
            // Indirect call, and we lost the control.
            mayCallCast = true;
            break;
          } else if (Callee->getName() == sCast) {
            CVER_DEBUG("\t mayCallCast due to explicit __cver_handle_cast\n");
            mayCallCast = true;
            break;
          }
        }
      } // End of I loop.
      if (mayCallCast)
        break;
    } // End of BB loop.
  }

  bool isModified = false;
  SmallVector<Instruction *, 16> InstToDelete;
  // If there must not exist __stack_handle_cast, we prune all
  // __cver_handle_stack_enter and __cver_handle_stack_exit in SCC.
  for (CallGraphSCC::iterator I = SCC.begin(), E = SCC.end(); I != E; ++I) {
    Function *F = (*I)->getFunction();
    if (!F)
      continue;

    if (!mayCallCast)
      isModified = CollectStackInvokeInstructions(F, InstToDelete);
  }

  for (Instruction *inst : InstToDelete) {
    CVER_DEBUG( "(prunning) : " << *inst << "\n");
    inst->eraseFromParent();
  }
  return isModified;
}

// If any of function in SCC must not call __stack_handle_cast,
// then we do prune out all __cver_handle_stack_enter in the SCC.
bool CverPruneStack::runOnSCC(CallGraphSCC &SCC) {
  if (ClDisable)
    return false;
  
  bool isModified;

  // Looks like SCCCallGraph is not a good choice (too many functions out of SCC
  // set), and hand-written DFS style call-graph scanning is working quite nice.
  // So SCC analysis is commented out.
  
  // isModified = PruneWithSCCCallGraph(SCC);
  isModified = PruneWithDepthFirstSearch(SCC);
  
  return isModified;;
}
