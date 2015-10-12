// CastVerifier pass is invoked as early as possible, so it is assumed
// that the instruction order is always kept as follows.

// %10 = icmp eq %"class.foo::GrandParent"* %9, null, !dbg !75, !cver_static_cast !2
// br i1 %10, label %15, label %11, !dbg !75, !cver_static_cast !2
// %12 = bitcast %"class.foo::GrandParent"* %9 to i8*, !dbg !76, !cver_static_cast !2
// %13 = getelementptr i8* %12, i64 -8, !dbg !76, !cver_static_cast !2
// %14 = bitcast i8* %13 to %"class.foo::Parent"*, !dbg !76, !cver_static_cast !2
// br label %16, !dbg !76, !cver_static_cast !2
// br label %16, !dbg !78, !cver_static_cast !2
// %17 = phi %"class.foo::Parent"* [ %14, %11 ], [ null, %15 ], !dbg !75, !cver_static_cast !2

// %18 = ptrtoint %"class.foo::GrandParent"* %9 to i64, !dbg !80, !cver_check !2
// %19 = call i64 @__cver_handle_cast(i8* bitcast ({ { [8 x i8]*, i32, i32 }, i8*, i64 }* @3 to i8*), i64 %18) #5, !dbg !80, !cver_check !2

// %20 = icmp eq i64 %19, 0, !dbg !80, !cver_nullify !2
// br i1 %20, label %22, label %21, !dbg !80, !cver_nullify !2
// br label %23, !dbg !83, !cver_nullify !2
// br label %23, !dbg !85, !cver_nullify !2
// %24 = phi %"class.foo::Parent"* [ %17, %21 ], [ null, %22 ], !dbg !75, !cver_nullify !2

// The last cver_nullify instruction (%24) is actually the results of static_cast.

#include "llvm/Transforms/Instrumentation.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/Analysis/MemoryDependenceAnalysis.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Triple.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/ValueMap.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/Target/TargetLibraryInfo.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/FileSystem.h"

using namespace llvm;

#define DEBUG_TYPE "cver"

#define CVER_DEBUG(stmt)                        \
  do {                                          \
    if (ClDebug) {                              \
      llvm::errs() << stmt;                     \
    }                                           \
  } while(0)

static cl::opt<bool> ClDebug("cver-debug", cl::desc("debug"), cl::Hidden,
                             cl::init(false));
static cl::opt<bool> ClStat("cver-stat", cl::desc("Dump cver pass statistics"),
                            cl::Hidden, cl::init(false));

static cl::opt<bool> ClOnlySecurity(
  "cver-only-security", cl::desc("Only Instrument security related casts."),
  cl::Hidden, cl::init(false));

static cl::opt<bool> ClOptSafeCast(
  "cver-opt-safe-cast", cl::desc("Optout safe cast."),
  cl::Hidden, cl::init(false));

static cl::opt<bool> ClDumpCast(
  "cver-dump-cast", cl::desc("Dump all casting types."),
  cl::Hidden, cl::init(false));

namespace {

class CastVerifier : public FunctionPass {
 public:
  CastVerifier()
      : FunctionPass(ID),
        DL(nullptr) {}
  const char *getPassName() const override { return "CastVerifier"; }
  bool runOnFunction(Function &F) override;
  bool doInitialization(Module &M) override;
  static char ID;

 private:
  void handleDumpCast(Function &F);  
  bool handleOnlySecurity(Function &F);
  bool handleOptSafeCast(Function &F);
  bool isSafeCast(Instruction *Inst, Function &F);
  bool findAllocTbaaRec(Instruction *Inst, int depth,
                        SmallSet<Instruction *,16> &VisitedInst);
  void initializeCallbacks(Module &M);
  const DataLayout *DL;
  LLVMContext *C;
  bool IsSecurityRelatedCast(Value *value, Type *CastedTy, Type *OrigTy,
                             SmallSet<Value *, 16> &Visited);
  void removeNonSecurityCast(Function &F, Instruction *castInst,
                             SmallVector<Instruction *, 16> &InstToDelete);
};

}  // namespace

char CastVerifier::ID = 0;

INITIALIZE_PASS(CastVerifier, "cast",
                "CastVerifier: detect undefined casting behaviors.",
                false, false);

FunctionPass *llvm::createCastVerifierPass() {
  return new CastVerifier();
}

bool CastVerifier::doInitialization(Module &M) {
  // Do nothing.
  return true;
}

// static Instruction *getPreviousInstruction(Instruction *inst) {
//   BasicBlock::iterator it(inst);
//   assert(it != inst->getParent()->begin());
//   return --it;
// }

static Instruction *getNextInstruction(Instruction *inst) {
  BasicBlock::iterator it(inst);
  assert(it != inst->getParent()->end());
  return ++it;
}

bool CastVerifier::IsSecurityRelatedCast(Value *value, Type *CastedTy,
                                          Type *OrigTy,
                                          SmallSet<Value *, 16> &Visited) {

  // Propagate the value originated from the bitcasted value.  If any of the
  // propagated value is 1) used for return/call/invoke instructions, 2) not
  // a local variable, or 3) touching beyond the boundary of the original
  // type (before the bit-casting), it is a security related static_cast. If
  // it is not, it is non-security static_cast and we remove the
  // instrumented instructions.

  if (Visited.count(value)) // Already visited.
    return false;

  Type *valueTy = value->getType();

  CVER_DEBUG("Visit : " << *value << " (" << *valueTy << ")\n");
  Visited.insert(value);

  // Check how the value is taken.
  if (isa<GlobalValue>(value)) {
    CVER_DEBUG("\t True (global)\n");
    return true;
  } else if (LoadInst *LI = dyn_cast<LoadInst>(value)) {
    // If the value is taken from the outside, then we lost the control and mark
    // it as security sensitive.
    if (LI->getType() != CastedTy) {
      CVER_DEBUG("\t True (heap?)\n");
      return true;
    }
  }

  // Check how the value is used.
  for (User *user : value->users()) {
    CVER_DEBUG("\t (user) " << *user << "\n");

    if (StoreInst *SI = dyn_cast<StoreInst>(user)) {
      // store <CastedTy> <value>, <CastedTy>* <ptr>
      if (SI->getValueOperand()->getType() == CastedTy ||
          SI->getValueOperand()->getType() == valueTy) {
        if (IsSecurityRelatedCast(SI->getPointerOperand(),
                                  CastedTy, OrigTy, Visited))
          return true;
      } else {
        return true;
      }
    } // end of StoreInst

    else if (LoadInst *LI = dyn_cast<LoadInst>(user)) {
      // <value> = load <ty>* <ptr>
      if (LI->getType() == CastedTy) {
        if (IsSecurityRelatedCast(user, CastedTy, OrigTy, Visited))
          return true;
      } else {
        return true;
      }
    }
    else if (BitCastInst *BCI = dyn_cast<BitCastInst>(user)) {
      // <result> = bitcast <ty> <value> to <ty2>

      // If it is casted to larger types than the original type, this must be
      // security sensitive. If it is smaller, than all the following memory
      // acceses will be safe.
      // FIXME : use original type, not the casted type.
      if (BCI->getType()->getIntegerBitWidth() > OrigTy->getIntegerBitWidth()) {
        return true;
      }
    }

    else if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(user)) {
      if (GEP->getPointerOperandType() == CastedTy)
        return true;
    }

    else if (isa<ReturnInst>(user) ||
             isa<CallInst>(user) ||
             isa<InvokeInst>(user)) {
      return true;
    }
  } // end of User loop.
  return false;
}

void CastVerifier::removeNonSecurityCast(
  Function &F, Instruction *inst,
  SmallVector<Instruction *, 16> &InstToDelete) {
  // Now we have non-security cast here, so remove the instrumented
  // instructions.

  PHINode *nullPhiInst = dyn_cast<PHINode>(inst);
  if (!nullPhiInst) {
    CVER_DEBUG("ERROR : Cannot locate PHINode\n");
    return;
  }


  // Phi(CastecValue, NullValue) ==> Phi(CastedValue, CastedValue)
  Value *castedValue = nullPhiInst->getIncomingValue(0);
  nullPhiInst->setIncomingValue(1, castedValue);
  CVER_DEBUG("Removing null conditions : " << *nullPhiInst << "\n");

  Instruction *actualCastInst = dyn_cast<Instruction>(castedValue);
  if (!actualCastInst) {
    CVER_DEBUG("ERROR : Cannot locate Actual casting instruction\n");
    return;
  }

  Instruction *ptrtointInst = getNextInstruction(actualCastInst);
  Instruction *callInst = getNextInstruction(ptrtointInst);
  if (!ptrtointInst->getMetadata("cver_check") ||
      !callInst->getMetadata("cver_check") ||
      !isa<CallInst>(callInst)) {
    CVER_DEBUG("ERROR : Cannot locate Cver's check call isntruction\n");
    return;
  }

  // Replace callInst with simple assign instruction (always assign 1).
  Instruction *voidCallInst = new PtrToIntInst(
    ConstantInt::get(callInst->getType(), 1), callInst->getType());
  ReplaceInstWithInst(callInst, voidCallInst);

  // Remove ptrtoint instruction.
  ptrtointInst->eraseFromParent();
  return;
}

#define MAX_SAFECAST_CHECK_DEPTH 10

bool CastVerifier::findAllocTbaaRec(Instruction *Inst, int depth,
                                    SmallSet<Instruction *,16> &VisitedInst) {

  CVER_DEBUG("\t\t\t " << depth << " : " << Inst->getOpcodeName() << ":"
             << *Inst << "\n");

  MDNode *Node = Inst->getMetadata(LLVMContext::MD_tbaa);
  if (Node)
    CVER_DEBUG("\t\t\t\t TBAA : " << *Node << "\n");
  
  // If it's too complicated, don't optimize.
  if (depth >= MAX_SAFECAST_CHECK_DEPTH)
    return false;
  
  // If the inst is visited already, it will be handled in some other branches.
  if (VisitedInst.count(Inst))
    return true;
    
  VisitedInst.insert(Inst);

  // opcode : http://llvm.org/docs/doxygen/html/Instruction_8cpp_source.html
  switch(Inst->getOpcode()) {
  case Instruction::Load:
  case Instruction::BitCast:
  case Instruction::Store: {
    Value *value = Inst->getOperand(0);

    if (!isa<Instruction>(value)) {
      // TODO: If the value is not instruction, this can be an non-instruction
      // value (i.e., an argument value). We don't handle this case for now.
      return false;
    }
    return findAllocTbaaRec(dyn_cast<Instruction>(value), depth+1, VisitedInst);
  }
  case Instruction::Alloca: {
    // Scan all of its def-use chain to see how the value is taken.
    bool isAllSafe = true;
    for (User *user : Inst->users())
      if (Instruction *userInst = dyn_cast<Instruction>(user))
        // If any of use instruction is not safe, it is not safe.
        isAllSafe &= findAllocTbaaRec(userInst, depth+1, VisitedInst);
    return isAllSafe;
  }
  case Instruction::Call: {
    // TODO : check the type in the metadata from Clang.
    if (Inst->getMetadata("cver_new")) {
      // If the instruction is allocation that cver identified, we should be
      // able to retrieve TBAA.
      MDNode *Node = Inst->getMetadata(LLVMContext::MD_tbaa);
      assert(Node);
      CVER_DEBUG("\t\t\t\t cver_new TBAA : " << *Node << "\n");
    }
    break;
  }
  default:
    break;
  }
  // If we don't know how to handle this instruction, don't optimize.
  return false;
}

// Recursively checks whether the given Inst and all of its defs are safe-cast.
// Return true if it is a safe cast, false otherwise.
bool CastVerifier::isSafeCast(Instruction *Inst, Function &F) {
  MDNode *Node = Inst->getMetadata(LLVMContext::MD_tbaa);
  assert(Node);
  CVER_DEBUG("\t\t\t cast TBAA : " << *Node << "\n");

  // Recursively find TBAA of allocation site.
  SmallSet<Instruction *,16> VisitedInst;
  findAllocTbaaRec(Inst, 0, VisitedInst);
  return false;
}

static void dumpCastInfo(Type *DstTy) {
  // Get PID, and dump to /tmp/cast-info/[PID].txt
  llvm::sys::self_process *SP = llvm::sys::process::get_self();
  unsigned Pid = SP->get_id();
  std::string LogFilename = std::string("/tmp/cast-info/") +
    std::to_string(Pid) + std::string(".txt");
  std::string Error;

  llvm::raw_fd_ostream *OS = new llvm::raw_fd_ostream(
    LogFilename.c_str(), Error,
    llvm::sys::fs::F_Append | llvm::sys::fs::F_Text);

  if (!Error.empty()) {
    llvm::outs() << Error << "\n";
  } else {
    // Write to file.
    *OS << *DstTy << "\n";
    OS->close();
  }
  return;
}

void CastVerifier::handleDumpCast(Function &F) {
  for (auto &BB : F)
    for (auto &Inst : BB)
      if (isa<BitCastInst>(&Inst)) {
        Type *DstTy = Inst.getType();

        // we are only interested in first class pointer type castings.
        if (DstTy->isPointerTy()) {
          Type *PointeeTy = DstTy->getContainedType(0);
          if (!PointeeTy)
            continue;

          if (!PointeeTy->isIntegerTy() && !PointeeTy->isPointerTy()) {
            CVER_DEBUG("\t\t Dumping : " << *PointeeTy << "\n");
            dumpCastInfo(PointeeTy);
          }
        }
      }
  return;
}

bool CastVerifier::handleOptSafeCast(Function &F) {
  bool isModified = false;

  for (auto &BB : F) {
    for (auto &Inst : BB) {
      if (isa<BitCastInst>(&Inst) && Inst.getMetadata("cver_static_cast")) {
        Type *DstTy = Inst.getType();        
        CVER_DEBUG("\t\t Casting to " << *DstTy << "\n");
        CVER_DEBUG("\t----------------------------------------\n");

        isSafeCast(&Inst, F);
      }
    }
  }
  return isModified;
}

bool CastVerifier::handleOnlySecurity(Function &F) {
  bool isModified = false;  
  int CastNum = 0;
  int SecurityCastNum = 0;
  int NonSecurityCastNum = 0;

  SmallVector<Instruction *,16> InstToDelete;

  for (auto &BB : F) {
    for (auto &Inst : BB) {
      CVER_DEBUG("\t " << Inst << "\n");
      // 1. Locate the ptrtoint instruction. Ignore if it is not.
      if (isa<PHINode>(&Inst) && Inst.getMetadata("cver_nullify")) {
        Type *OrigTy = Inst.getType();
        Type *CastedTy = Inst.getType();

        CVER_DEBUG("\t\t Casting from " << *OrigTy << " to " << *CastedTy << "\n");
        CVER_DEBUG("\t----------------------------------------\n");

        CastNum++;
        SmallSet<Value *, 16> Visited;
        bool res = IsSecurityRelatedCast(&Inst, CastedTy, OrigTy, Visited);
        if (res) {
          CVER_DEBUG( "SECURITY RELATED : " << F.getName()<< ": YES\n");
          SecurityCastNum++;
        } else {
          CVER_DEBUG( "SECURITY RELATED : " << F.getName() << ": NO\n");
          removeNonSecurityCast(F, &Inst, InstToDelete);
          NonSecurityCastNum++;
          isModified = true;
        }
        CVER_DEBUG("\t----------------------------------------\n");
      }
    } // End of Inst loop.
  } // End of BB loop.

  if (ClStat && CastNum > 0) {
    llvm::errs() << "@CVER_STAT:"
                 << SecurityCastNum << ":"
                 << NonSecurityCastNum << ":"
                 << F.getName()
                 << "@\n";
  }
  return isModified;
}

bool CastVerifier::runOnFunction(Function &F) {
  bool isModified = false;

  CVER_DEBUG("----------------------------------------\n");
  CVER_DEBUG("[*] " << F.getName() << "\n");

  if (ClDumpCast)
    handleDumpCast(F);

  if (ClOnlySecurity)
    isModified |= handleOnlySecurity(F);

  if (ClOptSafeCast)
    isModified |= handleOptSafeCast(F);

  return isModified;
}
