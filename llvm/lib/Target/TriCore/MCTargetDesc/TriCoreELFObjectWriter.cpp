//===-- TriCoreELFObjectWriter.cpp - TriCore ELF Writer -------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/TriCoreMCTargetDesc.h"
#include "MCTargetDesc/TriCoreFixupKinds.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#include <memory>

using namespace llvm;

namespace {
  class TriCoreELFObjectWriter : public MCELFObjectTargetWriter {
  public:
    TriCoreELFObjectWriter(uint8_t OSABI);

    virtual ~TriCoreELFObjectWriter();

    unsigned getRelocType(MCContext &Ctx, const MCValue &Target, const MCFixup &Fixup,
                          bool IsPCRel) const override;
  };
}

unsigned TriCoreELFObjectWriter::getRelocType(MCContext &Ctx, const MCValue &Target,
                                          const MCFixup &Fixup,
                                          bool IsPCRel) const {
  if (!IsPCRel) {
    llvm_unreachable("Only dealying with PC-relative fixups for now");
  }

  unsigned Type = 0;
  switch ((unsigned)Fixup.getKind()) {
  default:
    llvm_unreachable("Unimplemented");
  case TriCore::fixup_leg_mov_hi16_pcrel:
    Type = ELF::R_ARM_MOVT_PREL;
    break;
  case TriCore::fixup_leg_mov_lo16_pcrel:
    Type = ELF::R_ARM_MOVW_PREL_NC;
    break;
  }
  return Type;
}

TriCoreELFObjectWriter::TriCoreELFObjectWriter(uint8_t OSABI)
    : MCELFObjectTargetWriter(/*Is64Bit*/ false, OSABI, /*ELF::EM_TriCore*/ ELF::EM_ARM,
                              /*HasRelocationAddend*/ false) {}

TriCoreELFObjectWriter::~TriCoreELFObjectWriter() {}

std::unique_ptr<MCObjectTargetWriter> llvm::createTriCoreELFObjectWriter(uint8_t OSABI) {
  return make_unique<TriCoreELFObjectWriter>(OSABI);
}
