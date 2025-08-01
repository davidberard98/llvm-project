//===- YAMLRemarkSerializer.cpp -------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides the implementation of the YAML remark serializer using
// LLVM's YAMLTraits.
//
//===----------------------------------------------------------------------===//

#include "llvm/Remarks/YAMLRemarkSerializer.h"
#include "llvm/Remarks/Remark.h"
#include "llvm/Support/FileSystem.h"
#include <optional>

using namespace llvm;
using namespace llvm::remarks;

// Use the same keys whether we use a string table or not (respectively, T is an
// unsigned or a StringRef).
static void
mapRemarkHeader(yaml::IO &io, StringRef PassName, StringRef RemarkName,
                std::optional<RemarkLocation> RL, StringRef FunctionName,
                std::optional<uint64_t> Hotness, ArrayRef<Argument> Args) {
  io.mapRequired("Pass", PassName);
  io.mapRequired("Name", RemarkName);
  io.mapOptional("DebugLoc", RL);
  io.mapRequired("Function", FunctionName);
  io.mapOptional("Hotness", Hotness);
  io.mapOptional("Args", Args);
}

namespace llvm {
namespace yaml {

template <> struct MappingTraits<remarks::Remark *> {
  static void mapping(IO &io, remarks::Remark *&Remark) {
    assert(io.outputting() && "input not yet implemented");

    if (io.mapTag("!Passed", (Remark->RemarkType == Type::Passed)))
      ;
    else if (io.mapTag("!Missed", (Remark->RemarkType == Type::Missed)))
      ;
    else if (io.mapTag("!Analysis", (Remark->RemarkType == Type::Analysis)))
      ;
    else if (io.mapTag("!AnalysisFPCommute",
                       (Remark->RemarkType == Type::AnalysisFPCommute)))
      ;
    else if (io.mapTag("!AnalysisAliasing",
                       (Remark->RemarkType == Type::AnalysisAliasing)))
      ;
    else if (io.mapTag("!Failure", (Remark->RemarkType == Type::Failure)))
      ;
    else
      llvm_unreachable("Unknown remark type");

    mapRemarkHeader(io, Remark->PassName, Remark->RemarkName, Remark->Loc,
                    Remark->FunctionName, Remark->Hotness, Remark->Args);
  }
};

template <> struct MappingTraits<RemarkLocation> {
  static void mapping(IO &io, RemarkLocation &RL) {
    assert(io.outputting() && "input not yet implemented");

    StringRef File = RL.SourceFilePath;
    unsigned Line = RL.SourceLine;
    unsigned Col = RL.SourceColumn;

    io.mapRequired("File", File);

    io.mapRequired("Line", Line);
    io.mapRequired("Column", Col);
  }

  static const bool flow = true;
};

/// Helper struct for multiline string block literals. Use this type to preserve
/// newlines in strings.
struct StringBlockVal {
  StringRef Value;
  StringBlockVal(StringRef R) : Value(R) {}
};

template <> struct BlockScalarTraits<StringBlockVal> {
  static void output(const StringBlockVal &S, void *Ctx, raw_ostream &OS) {
    return ScalarTraits<StringRef>::output(S.Value, Ctx, OS);
  }

  static StringRef input(StringRef Scalar, void *Ctx, StringBlockVal &S) {
    return ScalarTraits<StringRef>::input(Scalar, Ctx, S.Value);
  }
};

/// ArrayRef is not really compatible with the YAMLTraits. Everything should be
/// immutable in an ArrayRef, while the SequenceTraits expect a mutable version
/// for inputting, but we're only using the outputting capabilities here.
/// This is a hack, but still nicer than having to manually call the YAMLIO
/// internal methods.
/// Keep this in this file so that it doesn't get misused from YAMLTraits.h.
template <typename T> struct SequenceTraits<ArrayRef<T>> {
  static size_t size(IO &io, ArrayRef<T> &seq) { return seq.size(); }
  static Argument &element(IO &io, ArrayRef<T> &seq, size_t index) {
    assert(io.outputting() && "input not yet implemented");
    // The assert above should make this "safer" to satisfy the YAMLTraits.
    return const_cast<T &>(seq[index]);
  }
};

/// Implement this as a mapping for now to get proper quotation for the value.
template <> struct MappingTraits<Argument> {
  static void mapping(IO &io, Argument &A) {
    assert(io.outputting() && "input not yet implemented");

    if (StringRef(A.Val).count('\n') > 1) {
      StringBlockVal S(A.Val);
      io.mapRequired(A.Key.data(), S);
    } else {
      io.mapRequired(A.Key.data(), A.Val);
    }
    io.mapOptional("DebugLoc", A.Loc);
  }
};

} // end namespace yaml
} // end namespace llvm

LLVM_YAML_IS_SEQUENCE_VECTOR(Argument)

YAMLRemarkSerializer::YAMLRemarkSerializer(raw_ostream &OS, SerializerMode Mode,
                                           std::optional<StringTable> StrTabIn)
    : RemarkSerializer(Format::YAML, OS, Mode),
      YAMLOutput(OS, reinterpret_cast<void *>(this)) {
  StrTab = std::move(StrTabIn);
}

void YAMLRemarkSerializer::emit(const Remark &Remark) {
  // Again, YAMLTraits expect a non-const object for inputting, but we're not
  // using that here.
  auto *R = const_cast<remarks::Remark *>(&Remark);
  YAMLOutput << R;
}

std::unique_ptr<MetaSerializer> YAMLRemarkSerializer::metaSerializer(
    raw_ostream &OS, std::optional<StringRef> ExternalFilename) {
  return std::make_unique<YAMLMetaSerializer>(OS, ExternalFilename);
}

static void emitMagic(raw_ostream &OS) {
  // Emit the magic number.
  OS << remarks::Magic;
  // Explicitly emit a '\0'.
  OS.write('\0');
}

static void emitVersion(raw_ostream &OS) {
  // Emit the version number: little-endian uint64_t.
  std::array<char, 8> Version;
  support::endian::write64le(Version.data(), remarks::CurrentRemarkVersion);
  OS.write(Version.data(), Version.size());
}

static void emitExternalFile(raw_ostream &OS, StringRef Filename) {
  // Emit the null-terminated absolute path to the remark file.
  SmallString<128> FilenameBuf = Filename;
  sys::fs::make_absolute(FilenameBuf);
  assert(!FilenameBuf.empty() && "The filename can't be empty.");
  OS.write(FilenameBuf.data(), FilenameBuf.size());
  OS.write('\0');
}

void YAMLMetaSerializer::emit() {
  emitMagic(OS);
  emitVersion(OS);

  // Emit StringTable with size 0. This is left over after removing StringTable
  // support from the YAML format. For now, don't unnecessarily change how the
  // the metadata is serialized. When changing the format, we should think about
  // just reusing the bitstream remark meta for this.
  uint64_t StrTabSize = 0;
  std::array<char, 8> StrTabSizeBuf;
  support::endian::write64le(StrTabSizeBuf.data(), StrTabSize);

  OS.write(StrTabSizeBuf.data(), StrTabSizeBuf.size());
  if (ExternalFilename)
    emitExternalFile(OS, *ExternalFilename);
}
