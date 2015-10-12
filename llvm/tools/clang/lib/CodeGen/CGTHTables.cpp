#include "CodeGenModule.h"
#include "CGCXXABI.h"
#include "TargetInfo.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/FileSystem.h"

using namespace clang;
using namespace CodeGen;

static QualType NullQualType;

CodeGenTHTables::CodeGenTHTables(CodeGenModule &CGM)
  : CGM(CGM), Context(CGM.getContext()),
    OptCverDebug(CGM.getLangOpts().Sanitize.CverDebug),
    OptCverLog(CGM.getLangOpts().Sanitize.CverLog) {}

// A callback from Sema.
void CodeGenModule::EmitTHTable(CXXRecordDecl *theClass, bool isRequired) {
  if (!getLangOpts().Sanitize.Cver)
    return;

  if (!isRequired) return;

  THTables.GenerateTypeHierarchy(theClass);
}

CodeGenTHTables *CodeGenModule::getTHTables() {
  return &THTables;
}

void CodeGenTHTables::GetMangledName(const CXXRecordDecl *RD,
                                     SmallString<256>& MangledName) {
  llvm::raw_svector_ostream Out(MangledName);
  CGM.getCXXABI().getMangleContext().mangleCXXRTTI(Context.getTypeDeclType(RD),
                                                   Out);
  Out.flush();
  return;
}

llvm::Constant *CodeGenModule::GetAddrOfTypeTable(const CXXRecordDecl *RD) {
  llvm::GlobalVariable *GV = THTables.GenerateTypeHierarchy(RD);
  if (!GV)
    return 0;
  return llvm::ConstantExpr::getBitCast(GV, Int8PtrTy);
}

static bool CollectAllBases(const CXXRecordDecl *BaseRD, void *OpaqueData) {
  BaseVec *Bases = reinterpret_cast<BaseVec*>(OpaqueData);
  Bases->push_back(BaseRD);
  return true;
}

// Recursively search through the given QT, and return the element's QualType if
// it is CXX class. Return Null otherwise.
static QualType getElemQualTypeOrNull(const QualType QT, bool &isCompoundElem) {
  const Type *Ty = QT.getTypePtrOrNull();

  if (!Ty)
    return NullQualType;

  if (Ty->isStructureOrClassType()) {
      isCompoundElem = true;
      return QT;
  } else if (const ArrayType *ATy = dyn_cast<ArrayType>(Ty)) {
    QualType ElemQT = ATy->getElementType();
    return getElemQualTypeOrNull(ElemQT, isCompoundElem);
  } else {
    // Do nothing for now.
  }
  return NullQualType;
}

TH_HASH CodeGenTHTables::hash_value_with_uniqueness(
  StringRef S, bool isSameLayout) {
  // Ditch lsb bit to have a space for isUnique.
  return (hash_value(S) << 1 | isSameLayout);
}

bool CodeGenTHTables::constructHashVector(
  const CXXRecordDecl *RD,
  SmallVector<llvm::Constant *, 64> &HashVector,
  SmallVector<llvm::SmallString<256>, 64> &BaseNames) {
  // Collect all base RD recursively.
  BaseVec Bases;
  RD->forallBases(CollectAllBases, (void*)&Bases);

  // Insert owner's hash first.
  SmallString<256> MangledTypeName;
  GetMangledName(RD, MangledTypeName);

  TH_HASH OwnHash = hash_value_with_uniqueness(MangledTypeName, false);
  HashVector.push_back(llvm::ConstantInt::get(CGM.Int64Ty, OwnHash));
  HashVector.push_back(llvm::Constant::getNullValue(CGM.Int64Ty));
  
  CVER_DEBUG("\t self: [ " << MangledTypeName << "] : "
             << (long)OwnHash << ":" << (void*)RD << "\n");

  CharUnits LeafSize = Context.getTypeSizeInChars(Context.getTypeDeclType(RD));
  
  // Compute the hash of Base type, and push to the table.
  const ASTRecordLayout &Layout = Context.getASTRecordLayout(RD);
  for (auto &BaseRD: Bases) {
    SmallString<256> BaseName;
    GetMangledName(BaseRD, BaseName);

    uint64_t BaseOffset = Layout.getBaseClassOffset(BaseRD).getQuantity();

    // If the BaseRD has the same size as RD, then isSameSize is marked as true.
    // This will be later used to handle empty inheritance cases.
    bool isSameLayout =
      (LeafSize == Context.getTypeSizeInChars(Context.getTypeDeclType(BaseRD)));

    TH_HASH BaseHash = hash_value_with_uniqueness(BaseName, isSameLayout);
    HashVector.push_back(llvm::ConstantInt::get(CGM.Int64Ty, BaseHash));
    HashVector.push_back(llvm::ConstantInt::get(CGM.Int64Ty, BaseOffset));
    BaseNames.push_back(BaseName);

    CVER_DEBUG( "\t Base: [ " << BaseName << "] : "
                << (long)BaseHash << ":" << (void*)RD << ":"
                << isSameLayout << ", offset:" << BaseOffset << "\n");
  }
  return true;
}

bool CodeGenTHTables::constructContainVector(
  const CXXRecordDecl *RD, uint64_t Offset,
  SmallVector<llvm::Constant *, 64> &ContainVec,
  SmallVector<SmallString<256>, 64> &ContainNames) {
  // Collect all (direct) containment classes
  // 1. The contained type has to be POD types (C++).
  //    See is{.*}Type() in Type.h
  // 2. Only collect the first order. Rests will be handled recursively.
  // TODO : consider C99 cases (complex/composite types).
  // TODO : what to do for static variables?
  const ASTRecordLayout &Layout = Context.getASTRecordLayout(RD);

  for (const auto *FD: RD->fields()) {
    // Ignore an anonymous field.
    if (!FD->getDeclName())
      continue;

    QualType QT = FD->getType().getUnqualifiedType();
    bool isCompoundElem = false;
    QualType ElemQT = getElemQualTypeOrNull(QT, isCompoundElem);
    assert(ElemQT != NullQualType);

    if (!isCompoundElem) {
      CVER_DEBUG("\t Field (!compound) " << " : " << FD->getDeclName() << "\n");
      continue;
    }

    // CHECKME: Possibly the containment field is on bit-field? Forget this case
    //        for now.
    assert(!FD->isBitField());

    // Get field offset and size.
    uint64_t fieldOffsetBits = Layout.getFieldOffset(FD->getFieldIndex());
    uint64_t fieldOffset = fieldOffsetBits / 8;
    uint64_t fieldSize = Context.getTypeSizeInChars(QT).getQuantity();

    // Should not be bit-fields.
    CXXRecordDecl *ElemRD = ElemQT->getAsCXXRecordDecl();
    
    assert(fieldOffsetBits % 8 == 0 && Context.getType(QT) % 8 == 0);

    ContainVec.push_back(llvm::ConstantInt::get(CGM.Int64Ty,
                                                Offset + fieldOffset));
    ContainVec.push_back(llvm::ConstantInt::get(CGM.Int64Ty, fieldSize));
    ContainVec.push_back(CGM.GetAddrOfTypeTable(ElemRD));

    SmallString<256> ContainName;
    GetMangledName(ElemRD, ContainName);
    ContainNames.push_back(ContainName);

    CVER_DEBUG("\t Field " << " : " << FD->getDeclName());
    CVER_DEBUG(" : "<<Offset<<"+"<<fieldOffset<<":"<<fieldSize);
    CVER_DEBUG(":" << (void*)RD << "\n");
  }

  // We should handle the containment of bases recursively, but not the
  // containment of the containment.
  // See DumpCXXRecordLayout().

  // Collect non-virtual bases.
  SmallVector<const CXXRecordDecl *, 4> Bases;
  for (const auto &I : RD->bases()) {
    assert(!I.getType()->isDependentType() &&
           "Cannot layout class with dependent bases.");
    if (!I.isVirtual())
      Bases.push_back(I.getType()->getAsCXXRecordDecl());
  }

  // Sort nvbases by offset.
  std::stable_sort(Bases.begin(), Bases.end(),
                   [&](const CXXRecordDecl *L, const CXXRecordDecl *R) {
    return Layout.getBaseClassOffset(L) < Layout.getBaseClassOffset(R);
  });

  // Recursively handle non-virtual bases
  for (SmallVectorImpl<const CXXRecordDecl *>::iterator I = Bases.begin(),
                                                        E = Bases.end();
       I != E; ++I) {
    const CXXRecordDecl *Base = *I;
    CVER_DEBUG("\t recursive containment on " << (void*)Base << "\n");
    CharUnits LocalBaseOffset = Layout.getBaseClassOffset(Base);

    assert(LocalBaseOffset.isPositive() || isZero());

    uint64_t BaseOffset = Offset + LocalBaseOffset.getQuantity();
    constructContainVector(Base, BaseOffset, ContainVec, ContainNames);
  }
  // TODO : Handle virtual bases?!
  return true;
}

void CodeGenTHTables::dumpDowncastInfo(StringRef SrcTypeName,
                                       llvm::Value *BeforeAddress,
                                       StringRef DstTypeName,
                                       llvm::Value *AfterAddress) {
  // Get PID, and dump to /tmp/cast-info/[PID].txt
  llvm::Type *SrcTy = BeforeAddress->getType();
  llvm::Type *DstTy = AfterAddress->getType();
  
  CVER_DEBUG("downcast : " << *SrcTy << "\t" << SrcTypeName << "\t"
             << *DstTy << "\t" << DstTypeName << "\n");
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
    *OS << "* " << *SrcTy << "\t" << SrcTypeName << "\t"
        << *DstTy << "\t" << DstTypeName << "\n";
    OS->close();
  }
  return;
}

// Dump class layouts for the given RD for easy debugging.
void CodeGenTHTables::dumpClassLayout(const CXXRecordDecl *RD) {
  // Get PID, and dump to /tmp/class/[PID].txt
  llvm::sys::self_process *SP = llvm::sys::process::get_self();
  unsigned Pid = SP->get_id();
  std::string LogFilename = std::string("/tmp/class/") +
    std::to_string(Pid) + std::string(".txt");
  std::string Error;

  llvm::raw_fd_ostream *OS = new llvm::raw_fd_ostream(
    LogFilename.c_str(), Error,
    llvm::sys::fs::F_Append | llvm::sys::fs::F_Text);

  if (!Error.empty()) {
    llvm::outs() << Error << "\n";
  } else {
    ASTContext &context = CGM.getContext();
    *OS << "\n*** DUMP_CLASS : " << Pid << " ***\n";
    context.DumpRecordLayout(RD, *OS, false);
    OS->close();
  }
  return;
}

void CodeGenTHTables::dumpTHTableInfo(
  StringRef MangledName, StringRef Name,
  int numComposites,int numBases,
  SmallVector<SmallString<256>, 64> &BaseNames,
  SmallVector<SmallString<256>, 64> &ContainNames) {

  // Get PID, and dump to /tmp/class/[PID].txt
  llvm::sys::self_process *SP = llvm::sys::process::get_self();
  unsigned Pid = SP->get_id();
  std::string LogFilename = std::string("/tmp/thtable/") +
    std::to_string(Pid) + std::string(".txt");
  std::string Error;

  llvm::raw_fd_ostream *OS = new llvm::raw_fd_ostream(
    LogFilename.c_str(), Error,
    llvm::sys::fs::F_Append | llvm::sys::fs::F_Text);

  if (!Error.empty()) {
    llvm::outs() << Error << "\n";
  } else {
    // Write to file.
    *OS << "[*] " << CGM.getModule().getModuleIdentifier() << " @@ "
        << MangledName << " @@ "
        << Name << " @@ " << numComposites << " @@ " << numBases << "\n";
    CVER_DEBUG("\t name : " << Name << "\n");    
    for (auto bn: BaseNames) {
      CVER_DEBUG("\t base : " << bn << "\n");
      *OS << "\tb\t" << bn << "\n";
    }
    for (auto cn: ContainNames) {
      CVER_DEBUG("\t contain : " << cn << "\n");
      *OS << "\tc\t" << cn << "\n";
    }
    OS->close();
  }
  return;
}

llvm::GlobalVariable *CodeGenTHTables::GenerateTypeHierarchy(
  const CXXRecordDecl *RD) {
  // Skip the THTable emit if it is not required (and not possible).
  // TODO : Completely separate THTable from Vtable routines.
  // if (CGM.getVTables().isVTableExternal(RD))
  //   return 0;

  // Check if THTable for RD is already generated.
  if (llvm::GlobalVariable *GV = CGM.getTHTableFromMap(RD))
    return GV;

  // Retrieve the unmangled typeName.
  SmallString<32> TypeName;
  intptr_t OpaquePtr = (intptr_t)Context.getTypeDeclType(RD).getAsOpaquePtr();
  CGM.getDiags().ConvertArgToString(DiagnosticsEngine::ak_qualtype,
                                    OpaquePtr,
                                    StringRef(), StringRef(), None, TypeName,
                                    ArrayRef<intptr_t>());
  CVER_DEBUG("GenerateTypeHierarchy: [ " << TypeName.str() << " ] : "
             << (void*)RD << "\n");

  // --------------------------------------------
  // Create and set the initializer; see emitVTableDefinitions()
  // FIXME : handle non-polymorphic types.

  SmallVector<SmallString<256>, 64> BaseNames;
  SmallVector<SmallString<256>, 64> ContainNames;
  
  SmallVector<llvm::Constant *, 64> HashVector;
  SmallVector<llvm::Constant *, 64> ContainVec;

  constructHashVector(RD, HashVector, BaseNames);
  constructContainVector(RD, 0, ContainVec, ContainNames);

  // --------------------------------------------
  // Create the global variable; see getAddrOfVTable()
  llvm::ArrayType *ContainVecTy = llvm::ArrayType::get(CGM.Int64Ty,
                                                       ContainVec.size());

  llvm::ArrayType *HashVectorType = llvm::ArrayType::get(CGM.Int64Ty,
                                                         HashVector.size());
  assert(ContainVec.size() % 3 == 0);
  assert(HashVector.size() % 2 == 0);

  llvm::Constant *TypeTableStruct[] = {
    // Containment Vector
    llvm::ConstantInt::get(CGM.Int64Ty, ContainVec.size()/3),
    llvm::ConstantArray::get(ContainVecTy, ContainVec),
    // Hash Vector
    llvm::ConstantInt::get(CGM.Int64Ty, HashVector.size()/2),
    llvm::ConstantArray::get(HashVectorType, HashVector),
    // Type name
    llvm::ConstantDataArray::getString(CGM.getLLVMContext(), TypeName.str())
  };
  llvm::Constant *THTable = llvm::ConstantStruct::getAnon(TypeTableStruct);

  auto *GV = new llvm::GlobalVariable(
      CGM.getModule(), THTable->getType(),
      /*isConstant=*/true, llvm::GlobalVariable::PrivateLinkage, THTable);
  GV->setUnnamedAddr(true);

  CGM.setTHTableInMap(RD, GV);

  SmallString<256> MangledTypeName;
  GetMangledName(RD, MangledTypeName);
  if (OptCverLog)
    dumpTHTableInfo(MangledTypeName, TypeName,
                    ContainVec.size()/3, HashVector.size()/2-1,
                    BaseNames, ContainNames);

  if (OptCverDebug)
    dumpClassLayout(RD);

  return GV;
}
