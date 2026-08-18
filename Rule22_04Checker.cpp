#include <clang/Basic/LLVM.h>
#include <clang/StaticAnalyzer/Core/BugReporter/BugReporter.h>
#include <clang/StaticAnalyzer/Core/Checker.h>
#include <clang/StaticAnalyzer/Core/CheckerManager.h>
#include <clang/StaticAnalyzer/Core/PathSensitive/CallDescription.h>
#include <clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h>
#include <clang/StaticAnalyzer/Core/PathSensitive/MemRegion.h>
#include <clang/StaticAnalyzer/Core/PathSensitive/ProgramState_Fwd.h>
#include <clang/StaticAnalyzer/Core/PathSensitive/SVals.h>
#include <clang/StaticAnalyzer/Core/PathSensitive/SymExpr.h>
#include <clang/StaticAnalyzer/Frontend/CheckerRegistry.h>

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/ErrorHandling.h>

#include <memory>
#include <optional>
#include <utility>

using namespace clang;
using namespace clang::ento;

namespace {
class Rule22_04Checker
    : public Checker<check::PostCall, check::PreCall, check::DeadSymbols> {
  const CallDescription FOpenFn{CDM::CLibrary, {"fopen"}, 2};
  const CallDescription FPrintfFn{CDM::CLibrary, {"fprintf"}, 2};
  const CallDescription FCloseFn{CDM::CLibrary, {"fclose"}, 1};

  const BugType WriteBugType{this, "Write to read-only stream"};

public:
  void checkPostCall(const CallEvent &Call, CheckerContext &C) const;
  void checkPreCall(const CallEvent &Call, CheckerContext &C) const;
  void checkDeadSymbols(SymbolReaper &SR, CheckerContext &C) const;
};
} // namespace

static std::optional<StringRef> getModeString(SVal Mode) {
  const MemRegion *MR = Mode.getAsRegion();
  if (!MR)
    return {};
  const auto *SR = llvm::dyn_cast<StringRegion>(MR->StripCasts());
  if (!SR)
    return {};
  return SR->getStringLiteral()->getString();
}

static bool isReadOnly(StringRef Mode) {
  assert(!Mode.empty() && "Mode string is empty");

  switch (Mode[0]) {
  case 'a':
  case 'w':
    return false;
  case 'r':
    return !Mode.contains('+');
  default:
    llvm_unreachable("Invalid mode string");
  }
}

REGISTER_MAP_WITH_PROGRAMSTATE(FileStreamMap, SymbolRef, bool)

void Rule22_04Checker::checkPostCall(const CallEvent &Call,
                                     CheckerContext &C) const {
  if (!FOpenFn.matches(Call))
    return;

  SymbolRef FileStream = Call.getReturnValue().getAsSymbol();
  if (!FileStream)
    return;

  std::optional<StringRef> Mode = getModeString(Call.getArgSVal(1));
  if (!Mode)
    return;

  const bool IsReadOnly = isReadOnly(*Mode);

  ProgramStateRef State = C.getState();
  State = State->set<FileStreamMap>(FileStream, IsReadOnly);

  const NoteTag *Note = C.getNoteTag([=](PathSensitiveBugReport &BR) {
    if (&BR.getBugType() != &WriteBugType || !BR.isInteresting(FileStream))
      return "";
    return IsReadOnly ? "File stream opened read-only"
                      : "File stream opened for writing";
  });

  C.addTransition(State, Note);
}

void Rule22_04Checker::checkPreCall(const CallEvent &Call,
                                    CheckerContext &C) const {
  if (FPrintfFn.matches(Call)) {
    SymbolRef FileStream = Call.getArgSVal(0).getAsLocSymbol();
    if (!FileStream)
      return;

    ProgramStateRef State = C.getState();
    const bool *IsReadOnly = State->get<FileStreamMap>(FileStream);
    if (IsReadOnly && *IsReadOnly) {
      ExplodedNode *N = C.generateErrorNode();
      if (!N)
        return;

      auto R = std::make_unique<PathSensitiveBugReport>(
          WriteBugType, "Trying to write to a read-only stream", N);
      R->addRange(Call.getSourceRange());
      R->markInteresting(FileStream);

      C.emitReport(std::move(R));
    }
    return;
  }

  if (FCloseFn.matches(Call)) {
    SymbolRef FileStream = Call.getArgSVal(0).getAsLocSymbol();
    if (!FileStream)
      return;

    ProgramStateRef State = C.getState();
    State = State->remove<FileStreamMap>(FileStream);

    C.addTransition(State);
    return;
  }
}

void Rule22_04Checker::checkDeadSymbols(SymbolReaper &SR,
                                        CheckerContext &C) const {
  ProgramStateRef State = C.getState();

  FileStreamMapTy TrackedFileStreams = State->get<FileStreamMap>();
  for (auto [FileStream, _] : TrackedFileStreams) {
    if (SR.isDead(FileStream))
      State = State->remove<FileStreamMap>(FileStream);
  }

  C.addTransition(State);
}

extern "C" void clang_registerCheckers(CheckerRegistry &Registry) {
  Registry.addChecker<Rule22_04Checker>("misra.Rule22_04",
                                        "Checker for MISRA C:2023 Rule 22.04");
}

extern "C" const char clang_analyzerAPIVersionString[] =
    CLANG_ANALYZER_API_VERSION_STRING;
