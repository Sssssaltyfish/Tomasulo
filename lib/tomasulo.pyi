from enum import IntEnum
from typing import Literal

class BHT(IntEnum):
    STRONGNOT: Literal[0]
    WEAKNOT: Literal[1]
    WEAKTAKEN: Literal[2]
    STORNGTAKEN: Literal[3]

class ResStation:
    busy: bool
    instr: int
    Vj: int
    Vk: int
    Qj: int
    Qk: int
    exTimeLeft: int
    robIdx: int

class ROBEntry:
    busy: bool
    valid: bool
    pc: int
    instr: int
    execUnit: int
    instrStatus: int
    result: int
    address: int

class RegResultEntry:
    valid: bool
    robIdx: int

class BTBEntry:
    valid: bool
    branchPred: BHT
    branchPc: int
    targetPc: int

class MachineState:
    pc: int
    cycles: int
    robHeadIdx: int
    robTailIdx: int
    rob: list[ROBEntry]
    reservation: list[ResStation]
    btb: list[BTBEntry]
    regResult: list[RegResultEntry]
    memory: list[int]
    regFile: list[int]
    def copy(self) -> MachineState: ...
    def nextStep(self) -> bool: ...
    def loadInstr(self, pc: int, instr: bytes) -> None: ...
    def setMemorySize(self, size: int) -> None: ...
    def __copy__(self) -> MachineState: ...
    def __deepcopy__(self) -> MachineState: ...

def printState(state: MachineState, memorySize: int) -> None: ...

class TomasuloError(Exception): ...
