#ifndef CLANG_CODEGEN_CGTHTABLE_H
#define CLANG_CODEGEN_CGTHTABLE_H


#define CVER_DEBUG(stmt)                        \
  if (OptCverDebug)                             \
    llvm::errs() << stmt;                       \

namespace clang {
class CXXRecordDecl;

namespace CodeGen {
class CodeGenModule;

typedef SmallVector<const CXXRecordDecl*, 64> BaseVec;
typedef uint64_t TH_HASH;

class CodeGenTHTables {
  CodeGenModule &CGM;
  ASTContext &Context;

  llvm::DenseMap<const CXXRecordDecl *, llvm::GlobalVariable *> THTables;

public:
  CodeGenTHTables(CodeGenModule &CGM);
  void GetMangledName(const CXXRecordDecl *RD, SmallString<256>& MangledName);
  llvm::GlobalVariable *GenerateTypeHierarchy(const CXXRecordDecl *RD);
  static TH_HASH hash_value_with_uniqueness(StringRef S, bool isSameLayout);
  void dumpDowncastInfo(StringRef SrcTypeName, llvm::Value *BeforeAddress,
                        StringRef DstTypeName, llvm::Value *AfterAddress);
private:
  bool OptCverDebug;
  bool OptCverLog;
  bool constructHashVector(const CXXRecordDecl *RD,
                           SmallVector<llvm::Constant *, 64> &HashVector,
                           SmallVector<SmallString<256>, 64> &BaseNames);
  bool constructContainVector(const CXXRecordDecl *RD,
                              uint64_t Offset,
                              SmallVector<llvm::Constant *, 64> &ContainVec,
                              SmallVector<SmallString<256>, 64> &ContainNames);
  void dumpClassLayout(const CXXRecordDecl *RD);
  void dumpTHTableInfo(StringRef MangledName, StringRef Name,
                       int numComposites, int numBases,
                       SmallVector<SmallString<256>, 64> &BaseNames,
                       SmallVector<SmallString<256>, 64> &ContainNames);
};

} // end namespace CodeGen
} // end namespace clang
#endif
