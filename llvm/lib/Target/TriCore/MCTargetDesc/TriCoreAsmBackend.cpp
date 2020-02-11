//===-- TriCoreAsmBackend.cpp - TriCore Assembler Backend -----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/TriCoreMCTargetDesc.h"
#include "MCTargetDesc/TriCoreFixupKinds.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDirectives.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCFixupKindInfo.h"
#include "llvm/MC/MCMachObjectWriter.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCSectionMachO.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCValue.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/BinaryFormat/MachO.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

namespace {
class TriCoreELFObjectWriter : public MCELFObjectTargetWriter {
public:
  TriCoreELFObjectWriter(uint8_t OSABI)
      : MCELFObjectTargetWriter(/*Is64Bit*/ false, OSABI, 
                                /*ELF::EM_TriCore*/ ELF::EM_ARM,
                                /*HasRelocationAddend*/ false) {}
};

class TriCoreAsmBackend : public MCAsmBackend {
public:
  TriCoreAsmBackend(Triple::OSType OSType) : MCAsmBackend(support::little), OSType(OSType) {}

  ~TriCoreAsmBackend() {}

  unsigned getNumFixupKinds() const override {
    return TriCore::NumTargetFixupKinds;
  }

  std::unique_ptr<MCObjectTargetWriter> createObjectTargetWriter() const override {
    return createTriCoreELFObjectWriter(MCELFObjectTargetWriter::getOSABI(OSType));
  }

  const MCFixupKindInfo &getFixupKindInfo(MCFixupKind Kind) const override {
    const static MCFixupKindInfo Infos[TriCore::NumTargetFixupKinds] = {
      // This table *must* be in the order that the fixup_* kinds are defined in
      // TriCoreFixupKinds.h.
      //
      // { Name, Offset (bits), Size (bits), Flags }
      { "fixup_leg_mov_hi16_pcrel", 0, 32, MCFixupKindInfo::FKF_IsPCRel },
      { "fixup_leg_mov_lo16_pcrel", 0, 32, MCFixupKindInfo::FKF_IsPCRel },
      { "fixup_call", 0, 24, 0 },
    };

    if (Kind < FirstTargetFixupKind) {
      return MCAsmBackend::getFixupKindInfo(Kind);
    }

    assert(unsigned(Kind - FirstTargetFixupKind) < getNumFixupKinds() &&
           "Invalid kind!");
    return Infos[Kind - FirstTargetFixupKind];
  }

  void applyFixup(const MCAssembler &Asm, const MCFixup &Fixup, const MCValue &Target,
                  MutableArrayRef<char> Data, uint64_t Value, bool IsResolved,
                  const MCSubtargetInfo *STI) const override;

  bool mayNeedRelaxation(const MCInst &Inst,
                         const MCSubtargetInfo &STI) const override { return false; }

  bool fixupNeedsRelaxation(const MCFixup &Fixup, uint64_t Value,
                            const MCRelaxableFragment *DF,
                            const MCAsmLayout &Layout) const override {
    return false;
  }

  void relaxInstruction(const MCInst &Inst, const MCSubtargetInfo &STI, MCInst &Res) const override {}

  bool writeNopData(raw_ostream &OS, uint64_t Count) const override {
    if (Count == 0) {
      return true;
    }
    return false;
  }

  unsigned getPointerSize() const { return 4; }

private:
  Triple::OSType OSType;
};

} // end anonymous namespace

static unsigned adjustFixupValue(const MCFixup &Fixup, const MCValue &Target, uint64_t Value,
                                 MCContext *Ctx) {
  unsigned Kind = Fixup.getKind();
  switch (Kind) {
  default:
    llvm_unreachable("Unknown fixup kind!");
  case TriCore::fixup_call:
    return Value & 0xffffff;
  case TriCore::fixup_leg_mov_hi16_pcrel:
    Value >>= 16;
    // Intentional fall-through
  case TriCore::fixup_leg_mov_lo16_pcrel:
    unsigned Hi4  = (Value & 0xF000) >> 12;
    unsigned Lo12 = Value & 0x0FFF;
    // inst{19-16} = Hi4;
    // inst{11-0} = Lo12;
    Value = (Hi4 << 16) | (Lo12);
    return Value;
  }
  return Value;
}

void TriCoreAsmBackend::applyFixup(const MCAssembler &Asm, const MCFixup &Fixup, const MCValue &Target,
                  MutableArrayRef<char> Data, uint64_t Value, bool IsResolved,
                  const MCSubtargetInfo *STI) const {
  Value = adjustFixupValue(Fixup, Target, Value, &Asm.getContext());
  if (Value == 0) {
    return; // Doesn't change encoding.
  }

  unsigned NumBytes = 4;
  unsigned Offset = Fixup.getOffset();
  assert(Offset + NumBytes <= Data.size() && "Invalid fixup offset!");

  // For each byte of the fragment that the fixup touches, mask in the bits from
  // the fixup value. The Value has been "split up" into the appropriate
  // bitfields above.
  for (unsigned i = 0; i != NumBytes; ++i) {
    Data[Offset + i] |= uint8_t((Value >> (i * 8)) & 0xff);
  }
}

MCAsmBackend *llvm::createTriCoreAsmBackend(const Target &T, const MCSubtargetInfo &STI,
                                            const MCRegisterInfo &MRI,
                                            const llvm::MCTargetOptions &TO) {
  return new TriCoreAsmBackend(STI.getTargetTriple().getOS());
}
