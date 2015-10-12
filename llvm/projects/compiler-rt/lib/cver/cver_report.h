#ifndef CVER_REPORT_H
#define CVER_REPORT_H

#include "sanitizer_common/sanitizer_common.h"
#include "sanitizer_common/sanitizer_stacktrace.h"
#include "sanitizer_common/sanitizer_suppressions.h"

namespace __cver {

void MaybePrintStackTrace(uptr sp, uptr pc, uptr bp);

/// \brief A description of a source location. This corresponds to Clang's
/// \c PresumedLoc type.
class SourceLocation {
  const char *Filename;
  u32 Line;
  u32 Column;

public:
  SourceLocation() : Filename(), Line(), Column() {}
  SourceLocation(const char *Filename, unsigned Line, unsigned Column)
    : Filename(Filename), Line(Line), Column(Column) {}

  /// \brief Determine whether the source location is known.
  bool isInvalid() const { return !Filename; }

  /// \brief Atomically acquire a copy, disabling original in-place.
  /// Exactly one call to acquire() returns a copy that isn't disabled.
  SourceLocation acquire() {
    u32 OldColumn = __sanitizer::atomic_exchange(
                        (__sanitizer::atomic_uint32_t *)&Column, ~u32(0),
                        __sanitizer::memory_order_relaxed);
    return SourceLocation(Filename, Line, OldColumn);
  }

  /// \brief Determine if this Location has been disabled.
  /// Disabled SourceLocations are invalid to use.
  bool isDisabled() {
    return Column == ~u32(0);
  }

  /// \brief Get the presumed filename for the source location.
  const char *getFilename() const { return Filename; }
  /// \brief Get the presumed line number.
  unsigned getLine() const { return Line; }
  /// \brief Get the column within the presumed line.
  unsigned getColumn() const { return Column; }
};

/// \brief A location within a loaded module in the program. These are used when
/// the location can't be resolved to a SourceLocation.
class ModuleLocation {
  const char *ModuleName;
  uptr Offset;

public:
  ModuleLocation() : ModuleName(0), Offset(0) {}
  ModuleLocation(const char *ModuleName, uptr Offset)
    : ModuleName(ModuleName), Offset(Offset) {}
  const char *getModuleName() const { return ModuleName; }
  uptr getOffset() const { return Offset; }
};

/// A location of some data within the program's address space.
typedef uptr MemoryLocation;

/// \brief Location at which a diagnostic can be emitted. Either a
/// SourceLocation, a ModuleLocation, or a MemoryLocation.
class Location {
public:
  enum LocationKind { LK_Null, LK_Source, LK_Module, LK_Memory };

private:
  LocationKind Kind;
  // FIXME: In C++11, wrap these in an anonymous union.
  SourceLocation SourceLoc;
  ModuleLocation ModuleLoc;
  MemoryLocation MemoryLoc;

public:
  Location() : Kind(LK_Null) {}
  Location(SourceLocation Loc) :
    Kind(LK_Source), SourceLoc(Loc) {}
  Location(ModuleLocation Loc) :
    Kind(LK_Module), ModuleLoc(Loc) {}
  Location(MemoryLocation Loc) :
    Kind(LK_Memory), MemoryLoc(Loc) {}

  LocationKind getKind() const { return Kind; }

  bool isSourceLocation() const { return Kind == LK_Source; }
  bool isModuleLocation() const { return Kind == LK_Module; }
  bool isMemoryLocation() const { return Kind == LK_Memory; }

  SourceLocation getSourceLocation() const {
    CHECK(isSourceLocation());
    return SourceLoc;
  }
  ModuleLocation getModuleLocation() const {
    CHECK(isModuleLocation());
    return ModuleLoc;
  }
  MemoryLocation getMemoryLocation() const {
    CHECK(isMemoryLocation());
    return MemoryLoc;
  }
};

/// Try to obtain a location for the caller. This might fail, and produce either
/// an invalid location or a module location for the caller.
Location getCallerLocation(uptr CallerLoc = GET_CALLER_PC());

/// Try to obtain a location for the given function pointer. This might fail,
/// and produce either an invalid location or a module location for the caller.
/// If FName is non-null and the name of the function is known, set *FName to
/// the function name, otherwise *FName is unchanged.
Location getFunctionLocation(uptr Loc, const char **FName);

/// \brief A description of a type.
class TypeDescriptor {
  /// A value from the \c Kind enumeration, specifying what flavor of type we
  /// have.
  u16 TypeKind;

  /// A \c Type-specific value providing information which allows us to
  /// interpret the meaning of a ValueHandle of this type.
  u16 TypeInfo;

  /// The name of the type follows, in a format suitable for including in
  /// diagnostics.
  char TypeName[1];

public:
  enum Kind {
    /// An integer type. Lowest bit is 1 for a signed value, 0 for an unsigned
    /// value. Remaining bits are log_2(bit width). The value representation is
    /// the integer itself if it fits into a ValueHandle, and a pointer to the
    /// integer otherwise.
    TK_Integer = 0x0000,
    /// A floating-point type. Low 16 bits are bit width. The value
    /// representation is that of bitcasting the floating-point value to an
    /// integer type.
    TK_Float = 0x0001,
    /// Any other type. The value representation is unspecified.
    TK_Unknown = 0xffff
  };

  const char *getTypeName() const { return TypeName; }

  Kind getKind() const {
    return static_cast<Kind>(TypeKind);
  }

  bool isIntegerTy() const { return getKind() == TK_Integer; }
  bool isSignedIntegerTy() const {
    return isIntegerTy() && (TypeInfo & 1);
  }
  bool isUnsignedIntegerTy() const {
    return isIntegerTy() && !(TypeInfo & 1);
  }
  unsigned getIntegerBitWidth() const {
    CHECK(isIntegerTy());
    return 1 << (TypeInfo >> 1);
  }

  bool isFloatTy() const { return getKind() == TK_Float; }
  unsigned getFloatBitWidth() const {
    CHECK(isFloatTy());
    return TypeInfo;
  }
};

/// \brief Annotation for a range of locations in a diagnostic.
class Range {
  Location Start, End;
  const char *Text;

public:
  Range() : Start(), End(), Text() {}
  Range(MemoryLocation Start, MemoryLocation End, const char *Text)
    : Start(Start), End(End), Text(Text) {}
  Location getStart() const { return Start; }
  Location getEnd() const { return End; }
  const char *getText() const { return Text; }
};

void ReportBadCasting(SourceLocation Loc, const char *dstTypeName,
                      const char *srcTypeName, uptr Pointer);
bool MatchSuppression(const char *Str, SuppressionType Type);
} // namespace __cver

#endif // CVER_REPORT_H
