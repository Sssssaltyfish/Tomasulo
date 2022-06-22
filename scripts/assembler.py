#! /usr/bin/env python3

import argparse
from pathlib import Path
import sys

from typing import Callable


def regno(*regs: str):
    return tuple(int(reg[1:]) for reg in regs)


def mask(bits: int):
    return (1 << bits) - 1


mask6 = mask(6)
mask5 = mask(5)
mask16 = mask(16)
mask11 = mask(11)
mask26 = mask(26)


def instr_i(op: int, rd: str, r1: str, imm: str):
    rs1, r_d = regno(r1, rd)
    return (
        (op & mask6) << 26
        | (rs1 & mask5) << 21
        | (r_d & mask5) << 16
        | (int(imm) & mask16)
    )


def instr_r(op: int, rd: str, r1: str, r2: str, func: int):
    rs1, rs2, r_d = regno(r1, r2, rd)
    return (
        (op & mask6) << 26
        | (rs1 & mask5) << 21
        | (rs2 & mask5) << 16
        | (r_d & mask5) << 11
        | (func & mask11)
    )


def instr_j(op: int, imm: int):
    return (op & mask6) << 26 | (imm & mask26)


InstrGen = Callable[..., int]


class Asm:
    opcode: dict[str, tuple[int, InstrGen]] = {
        "lw": (35, instr_i),
        "sw": (43, instr_i),
        "add": (0, instr_r),
        "addi": (8, instr_i),
        "sub": (0, instr_r),
        "and": (0, instr_r),
        "andi": (12, instr_i),
        "beqz": (4, instr_i),
        "j": (2, instr_j),
        "halt": (1, instr_j),
        "noop": (3, instr_j),
    }

    func_code = {
        "add": 32,
        "sub": 34,
        "and": 36,
    }

    def __init__(self) -> None:
        self.jmp = dict[str, int]()
        self.program = list[tuple[str, list[str]]]()
        self.lineno = 0

    def jmp_offset(self, label: str):
        return self.jmp[label] - self.lineno - 1

    def process_line(self, line: str):
        op, *args = line.strip().split()
        op = op.lower()

        if op not in self.opcode:
            self.jmp[op] = self.lineno
            op, *args = args
            op = op.lower()

        args = "".join(args)
        comment_index = args.find(";")
        if comment_index != -1:
            args = args[:comment_index]
        args = args.replace(" ", "").split(",")

        self.program.append((op, args))
        self.lineno += 1

    def process_program(self, program: list[str]):
        for line in program:
            self.process_line(line)
        self.lineno = 0

    def emit_line(self, op: str, args: list[str]):
        opcode, instr_gen = self.opcode[op]
        instr: int
        if instr_gen is instr_i:
            if op != "beqz":
                instr = instr_gen(opcode, *args)
            else:
                rs1, label = args
                imm = self.jmp_offset(label)
                instr = instr_gen(opcode, "r0", rs1, imm)
        elif instr_gen is instr_r:
            func = self.func_code[op]
            instr = instr_gen(opcode, *args, func)
        elif instr_gen is instr_j:
            if op in ("halt", "noop"):
                instr = instr_gen(opcode, 0)
            else:
                (label,) = args
                imm = self.jmp_offset(label)
                instr = instr_gen(opcode, imm)
        else:
            raise Exception("Invalid asm:", op, args, "lineno:", self.lineno)

        self.lineno += 1
        return instr

    def emit(self, program: str):
        p = program.splitlines()
        self.process_program(p)

        return [self.emit_line(*line) for line in self.program]


def translate(program: str):
    return bytes().join(
        instr.to_bytes(4, sys.byteorder, signed=False) for instr in Asm().emit(program)
    )


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("filepath", type=Path)
    parser.add_argument("-o", "--output", type=Path)
    return parser.parse_args()


def main():
    args = parse_args()
    input_path: Path = args.filepath
    output_path: Path = args.output
    if output_path is None:
        output_path = Path("a.out")
    with input_path.open("r") as input_f:
        program = input_f.read()

    instrs = translate(program)
    with output_path.open("wb") as output_f:
        output_f.write(instrs)


if __name__ == "__main__":
    main()
