#include "cver_report.h"
#include "cver_init.h"
#include "cver_flags.h"
#include "cver_allocator.h"
#include "cver_common.h"

#include "sanitizer_common/sanitizer_report_decorator.h"
#include "sanitizer_common/sanitizer_stacktrace.h"
#include "sanitizer_common/sanitizer_symbolizer.h"
#include "sanitizer_common/sanitizer_suppressions.h"

using namespace __cver;

void __cver::MaybePrintStackTrace(uptr sp, uptr pc, uptr bp) {
  if (flags()->no_print_stacktrace)
    return;
  if (StackTrace::WillUseFastUnwind(false))
    return;
  StackTrace stack;
  stack.Unwind(kStackTraceMax, pc, bp, 0, 0, 0, false);
  stack.Print();
}

Location __cver::getCallerLocation(uptr CallerLoc) {
  if (!CallerLoc)
    return Location();

  uptr Loc = StackTrace::GetPreviousInstructionPc(CallerLoc);
  return getFunctionLocation(Loc, 0);
}

Location __cver::getFunctionLocation(uptr Loc, const char **FName) {
  if (!Loc)
    return Location();

  AddressInfo Info;
  if (!Symbolizer::Get()->SymbolizePC(Loc, &Info, 1) || !Info.module ||
      !*Info.module)
    return Location(Loc);

  if (FName && Info.function)
    *FName = Info.function;

  if (!Info.file)
    return ModuleLocation(Info.module, Info.module_offset);

  return SourceLocation(Info.file, Info.line, Info.column);
}

static void renderLocation(Location Loc) {
  InternalScopedString LocBuffer(1024);
  switch (Loc.getKind()) {
  case Location::LK_Source: {
    SourceLocation SLoc = Loc.getSourceLocation();
    if (SLoc.isInvalid())
      LocBuffer.append("<unknown>");
    else
      PrintSourceLocation(&LocBuffer, SLoc.getFilename(), SLoc.getLine(),
                          SLoc.getColumn());
    break;
  }
  case Location::LK_Module:
    PrintModuleAndOffset(&LocBuffer, Loc.getModuleLocation().getModuleName(),
                         Loc.getModuleLocation().getOffset());
    break;
  case Location::LK_Memory:
    LocBuffer.append("%p", Loc.getMemoryLocation());
    break;
  case Location::LK_Null:
    LocBuffer.append("<unknown>");
    break;
  }
  Printf("%s:", LocBuffer.data());
}

bool __cver::MatchSuppression(const char *Str, SuppressionType Type) {
  Suppression *s;
  // If .preinit_array is not used, it is possible that the Cver runtime is not
  // initialized.
#ifndef CVER_USE_PREINIT_ARRAY
  InitIfNecessary();
#endif
  return SuppressionContext::Get()->Match(Str, Type, &s);
}

void __cver::ReportBadCasting(SourceLocation Loc, const char *dstTypeName,
                              const char *srcTypeName, uptr Pointer) {
  // From now, it is truly a bad casting.
  SanitizerCommonDecorator Decor;

  // Starting marker.
  Printf(Decor.Warning());
  Printf("== CastVerifier Bad-casting Reports\n");
  Printf(Decor.Default());

  renderLocation(Loc);

  Printf(" Casting from %s to %s\n", srcTypeName, dstTypeName);

  // Object meta information.
  Printf("\t Pointer \t %p\n", Pointer);
  Printf("\t Alloc base \t %p\n", GetAllocBegin(Pointer));
  Printf("\t User base \t %p\n", GetAllocUserBegin(Pointer));
  Printf("\t Offset \t %p\n", Pointer - (uptr)GetAllocUserBegin(Pointer));
  // Printf("\t Chunk base \t %p\n", GetCverChunkByAddr(Pointer));
  Printf("\t TypeTable \t %p\n", GetCverTypeTable(Pointer));

  // Print Stack Traces if necessary.
  GET_CALLER_PC_BP_SP;
  MaybePrintStackTrace(sp, pc, bp);

  // End marker.
  Printf(Decor.Warning());
  Printf("== End of reports.\n\n");
  Printf(Decor.Default());

  if (flags()->die_on_error) {
    Printf("== Terminating the program.\n\n");
    Die();
  }

  return;
}
