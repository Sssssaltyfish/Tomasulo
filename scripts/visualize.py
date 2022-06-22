#! /usr/bin/env python3

import os
import sys
import argparse
import platform

from typing import Callable

sys.path.append(".")
if platform.system() == "Windows":
    from pathlib import Path
    libpath = Path(__file__).parent.absolute() / "../lib"
    os.add_dll_directory(libpath.absolute().as_posix())

import tkinter as tk
import tkinter.messagebox as msg
from tkinter import filedialog as fdl
from tksheet import Sheet

from scripts.assembler import translate


if not os.path.isfile("lib/tomasulo.so"):
    import subprocess

    subprocess.run(["xmake"])


import lib.tomasulo as t

instr_state = ["ISSUING", "EXECUTING", "WRITING_RESULT", "COMMITTING"]
units = ["READY", "LOAD1", "LOAD2", "STORE1", "STORE2", "INT1", "INT2"]


def load_asm(path: str):
    with open(path, "r") as f:
        a = f.read()
    try:
        b = translate(a)
    except Exception as e:
        raise ValueError("Invalid asm") from e
    return a.splitlines(), [b[i * 4 : (i + 1) * 4] for i in range(len(b) // 4)]


def sheet(
    parent,
    data,
    headers,
    row_index,
    show_x_scrollbar=False,
    show_y_scrollbar=False,
    height=600,
    width=300,
    row_height=20,
    column_width=100,
    **kwargs,
):
    args = locals().copy()
    args.pop("kwargs")
    return Sheet(**args, **kwargs)


class GUI:
    def __init__(self, autospeed: int = 200) -> None:
        self.init()
        self.autospeed = autospeed

        window = tk.Tk()
        window.geometry("1920x1080")
        window.title("Demonstration of Tomasulo Algorithm")
        window.grid_columnconfigure(0, weight=1)

        self.window = window

        buttons = tk.Frame(window)
        self.control_buttons = {
            "prev": tk.Button(buttons, text="<-", command=self.prev, state="disabled"),
            "next": tk.Button(buttons, text="->", command=self.next, state="disabled"),
            "auto": tk.Button(
                buttons, text="|>", command=self.autoplay, state="disabled"
            ),
            "pause": tk.Button(
                buttons, text="||", command=self.pause, state="disabled"
            ),
            "reset": tk.Button(
                buttons, text="reset", command=self.reset, state="disabled"
            ),
        }
        self.load_asm_button = tk.Button(
            buttons, text="load asm", command=lambda: self.load_by(load_asm)
        )

        buttons.grid(row=0)
        for idx, button in enumerate(buttons.children.values()):
            button.grid(row=0, column=idx)
        self.buttons = buttons

        info_frame = tk.Frame(window)
        info_frame.grid(row=1)
        pc = tk.StringVar(info_frame, self.current_pc)
        pc_frame = tk.Label(info_frame, textvariable=pc)
        cycle = tk.StringVar(info_frame, self.current_cycle)
        cycle_frame = tk.Label(info_frame, textvariable=cycle)
        pc_frame.grid(row=0, column=0)
        cycle_frame.grid(row=0, column=1)
        self.pc = pc
        self.cycle = cycle

        reg_mem_btb_frame = tk.Frame(window)
        reg_mem_btb_frame.grid(row=2)

        reg_frame = tk.Frame(reg_mem_btb_frame)
        reg_frame.pack(side="left", padx=(50, 50))
        reg_label = tk.Label(reg_frame, text="Registers")
        reg_sheet = sheet(
            reg_frame,
            self.current_reg,
            ["value", "valid", "result #ROB"],
            [f"r{i}" for i in range(32)],
            height=20 * 33,
            width=400,
            row_height=20,
            column_width=100,
        )
        reg_label.pack()
        reg_sheet.pack()
        self.reg_sheet = reg_sheet

        mem_frame = tk.Frame(reg_mem_btb_frame)
        mem_frame.pack(side="left", padx=(50, 50))
        mem_label = tk.Label(mem_frame, text="Memory")
        mem_sheet = sheet(
            mem_frame,
            self.current_memory,
            ["value"],
            [i for i in range(self.memsize)],
            show_y_scrollbar=True,
            width=300,
            column_width=100,
        )
        mem_label.pack()
        mem_sheet.pack()
        self.mem_sheet = mem_sheet

        btb_asm_frame = tk.Frame(reg_mem_btb_frame)
        btb_asm_frame.pack(side="left", padx=(50, 50))

        btb_frame = tk.Frame(btb_asm_frame)
        btb_frame.pack()
        btb_label = tk.Label(btb_frame, text="Branch Target Buffer")
        btb = self.current_btb
        btb_sheet = sheet(
            btb_frame,
            btb,
            ["valid", "branch PC", "target PC", "pred"],
            list(range(len(btb))),
            width=500,
            height=20 * (len(btb) + 1),
        )
        btb_label.pack()
        btb_sheet.pack()
        self.btb_sheet = btb_sheet

        asm_frame = tk.Frame(btb_asm_frame)
        asm_frame.pack()

        asm_label = tk.Label(asm_frame, text="Assembly")
        asm_sheet = sheet(
            asm_frame,
            [[line] for line in self.asm],
            [],
            [],
            width=300,
            height=20 * 20,
            show_y_scrollbar=True,
            show_row_index=False,
            show_header=False,
            show_top_left=False,
        )
        asm_label.pack()
        asm_sheet.pack()
        self.asm_sheet = asm_sheet

        rob_reserv_frame = tk.Frame(window)
        rob_reserv_frame.grid(row=3)

        rob_frame = tk.Frame(rob_reserv_frame)
        rob_frame.pack(side="left", padx=(50, 50))
        rob_label = tk.Label(rob_frame, text="Reorder Buffer")
        current_rob = self.current_rob
        rob_size = len(current_rob)
        rob_sheet = sheet(
            rob_frame,
            current_rob,
            [
                "busy",
                "valid",
                "pc",
                "instr",
                "instr status",
                "exec unit",
                "result",
                "address",
            ],
            [i for i in range(rob_size)],
            height=20 * (rob_size + 1),
            row_height=20,
            width=590,
        )
        rob_label.pack()
        rob_sheet.pack()
        self.rob_sheet = rob_sheet

        reserv_frame = tk.Frame(rob_reserv_frame)
        reserv_frame.pack(side="left", padx=(50, 50))
        reserv_label = tk.Label(reserv_frame, text="Reservations")
        reserv_sheet = sheet(
            reserv_frame,
            self.current_reserv,
            ["busy", "instr", "Vj", "Vk", "Qj", "Qk", "exec time left", "ROB index"],
            units[1:],
            height=140,
            row_height=20,
            width=900,
        )
        reserv_label.pack()
        reserv_sheet.pack()
        self.reserv_sheet = reserv_sheet

        window.after(self.autospeed, self.tick)
        window.mainloop()

    def init(self):
        self.state = [t.MachineState()]
        self.playing = False
        self.loaded = False
        self.halted = False
        self.current = 0
        self.memsize = 0
        self.asm = list[str]()

    def reset(self):
        self.playing = False
        self.current = 0
        self.redraw()

    def update_button(self):
        buttons = self.control_buttons
        if not self.loaded:
            for button in buttons.values():
                button["state"] = "disabled"
            return
        for button in buttons.values():
            button["state"] = "normal"
        if not self.playing:
            buttons["pause"]["state"] = "disabled"
            buttons["auto"]["state"] = "normal"
        else:
            buttons["pause"]["state"] = "normal"
            buttons["auto"]["state"] = "disabled"

        if self.halted and self.current == len(self.state) - 1:
            buttons["next"]["state"] = "disabled"
        else:
            buttons["next"]["state"] = "normal"
        if self.current == 0:
            buttons["prev"]["state"] = "disabled"
        else:
            buttons["prev"]["state"] = "normal"

    def update(self):
        new_state = self.state[-1].copy()
        self.halted = new_state.nextStep()
        if self.halted:
            msg.showinfo("halted", "Halted")
        self.state.append(new_state)

    def load_by(self, f: Callable[[str], tuple[list[str], list[bytes]]]):
        self.init()
        path = fdl.askopenfilename()
        if not path:
            return
        try:
            self.asm, b = f(path)
        except Exception as e:
            msg.showerror(type(e).__name__, " ".join(map(str, e.args)))
            return
        init = self.state[0]
        pc = init.pc
        for instr in b:
            init.loadInstr(pc, instr)
            pc += 1
        self.memsize = pc
        init.setMemorySize(pc)
        self.loaded = True
        self.mem_sheet.row_index([i for i in range(pc)])
        self.redraw()

    def prev(self):
        if self.loaded:
            self.current -= 1
            self.redraw()

    def next(self):
        if self.loaded:
            self.current += 1
            if self.current == len(self.state):
                self.update()
            self.redraw()

    def autoplay(self):
        if self.loaded:
            self.playing = True
            self.update_button()

    def pause(self):
        if self.loaded:
            self.playing = False
            self.update_button()

    def tick(self):
        if self.loaded:
            if self.playing:
                if not self.halted or self.current != len(self.state) - 1:
                    self.next()
                else:
                    self.playing = False
                    self.update_button()
        self.window.after(self.autospeed, self.tick)

    @property
    def current_state(self):
        return self.state[self.current]

    @property
    def current_pc(self):
        return f"PC: {self.current_state.pc}"

    @property
    def current_cycle(self):
        return f"Cycles: {self.current_state.cycles}"

    @property
    def current_reg(self):
        state = self.current_state
        return [
            (reg, result.valid, result.robIdx)
            for reg, result in zip(state.regFile, state.regResult)
        ]

    @property
    def current_memory(self):
        state = self.current_state
        return [(word,) for word in state.memory[: self.memsize]]

    @property
    def current_rob(self):
        state = self.current_state
        return [
            (
                entry.busy,
                entry.valid,
                entry.pc,
                entry.instr,
                instr_state[entry.instrStatus],
                units[entry.execUnit],
                entry.result,
                entry.address,
            )
            for entry in state.rob
        ]

    @property
    def current_reserv(self):
        state = self.current_state
        return [
            (
                entry.busy,
                entry.instr,
                entry.Vj,
                entry.Vk,
                units[entry.Qj],
                units[entry.Qk],
                entry.exTimeLeft,
                entry.robIdx,
            )
            for entry in state.reservation[1:]
        ]

    @property
    def current_btb(self):
        state = self.current_state
        return [
            (
                entry.valid,
                entry.branchPc,
                entry.targetPc,
                entry.branchPred.name,
            )
            for entry in state.btb
        ]

    def redraw(self):
        # t.printState(self.current_state, self.memsize)
        self.update_button()

        self.pc.set(self.current_pc)
        self.cycle.set(self.current_cycle)

        self.asm_sheet.set_sheet_data([[line] for line in self.asm], redraw=False)
        self.asm_sheet.dehighlight_all()
        self.asm_sheet.highlight_cells(
            self.current_state.pc - 16, 0, bg="#dce3e7", redraw=True
        )

        self.reg_sheet.set_sheet_data(self.current_reg, redraw=False)
        self.reg_sheet.set_all_column_widths(redraw=True)

        self.mem_sheet.set_sheet_data(self.current_memory, redraw=False)
        self.mem_sheet.set_all_column_widths(redraw=False)
        self.mem_sheet.dehighlight_all()
        self.mem_sheet.highlight_cells(
            self.current_state.pc, 0, bg="#dce3e7", redraw=True
        )

        self.btb_sheet.set_sheet_data(self.current_btb, redraw=False)
        self.btb_sheet.set_all_column_widths(redraw=True)

        self.rob_sheet.set_sheet_data(self.current_rob, redraw=False)
        self.rob_sheet.set_all_column_widths(redraw=True)

        self.reserv_sheet.set_sheet_data(self.current_reserv, redraw=False)
        self.reserv_sheet.set_all_column_widths(redraw=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "play_speed",
        type=int,
        default=100,
        nargs="?",
        help="speed of auto-playing in ms",
    )
    args = parser.parse_args()
    gui = GUI(args.play_speed)
