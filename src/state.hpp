#pragma once

#include <cassert>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <random>

#include "decode.hpp"
#include "defines.hpp"
#include "error.hpp"

template <class F> inline word randBy(F&& f) {
    static thread_local auto gen = std::mt19937_64{};
    return f(gen);
}

inline BHT newBHT(BHT old, bool taken) {
    if (taken) {
        if (old == BHT::STRONGTAKEN) {
            return BHT::STRONGTAKEN;
        } else {
            return BHT(old + 1);
        }
    } else {
        if (old == BHT::STRONGNOT) {
            return BHT::STRONGNOT;
        } else {
            return BHT(old - 1);
        }
    }
}

struct MachineState {
    word pc = 16;        /* PC */
    word cycles = 0;     /* 已经过的周期数 */
    word robHeadIdx = 0; /* 循环队列的头指针 */
    word robTailIdx = 0; /* 循环队列的尾指针 */
    word memorySize = 0;
    std::array<ROBEntry, ROBSIZE> rob{};                /* ROB */
    std::array<ResStation, NUMUNITS + 1> reservation{}; /* 保留栈 */
    std::array<BTBEntry, BTBSIZE> btb{};                /* 分支预测缓冲栈 */
    std::array<RegResultEntry, NUMREGS> regResult{};    /* 寄存器状态 */
    std::array<word, MEMSIZE> memory{};                 /* 内存   */
    std::array<word, NUMREGS> regFile{};                /* 寄存器 */

    void broadcastUpdate(word unit, word value) {
        /*
         * 更新保留栈:
         * 将位于公共数据总线上的数据
         * 复制到正在等待它的其他保留栈中去
         */
        for (auto& reserv : reservation) {
            if (!reserv.busy)
                continue;
            if (reserv.Qj == unit) {
                reserv.Vj = value;
                reserv.Qj = READY;
            }
            if (reserv.Qk == unit) {
                reserv.Vk = value;
                reserv.Qk = READY;
            }
        }
        for (auto& robEntry : rob) {
            if (robEntry.busy && !robEntry.valid && robEntry.execUnit == unit) {
                robEntry.result = value;
                robEntry.valid = true;
            }
        }
    }

    void resetROB() {
        //* 清空 ROB
        robHeadIdx = 0;
        robTailIdx = 0;
        for (auto& robEntry : rob) {
            robEntry = {};
        }
    }

    void resetReserve() {
        //* 清空所有保留站
        for (auto& reservEntry : reservation) {
            reservEntry = {};
        }
    }

    void resetRegResult() {
        //* 清空"寄存器状态"中的各项, 即使得所有寄存器直接可用
        for (auto& rg : regResult) {
            rg = {};
        }
    }

    void issueInstr(word pc, word unit, word robIdx) {
        /*
         * 发射指令:
         * 填写保留栈和 ROB 项的内容.
         * 注意, 要在所有的字段中写入正确的值.
         * 检查寄存器状态, 相应的在 Vj, Vk 和 Qj, Qk 字段中设置正确的值:
         * 对于 I 类型指令, 设置 Qk=0, Vk=0;
         * 对于 sw 指令, 如果寄存器有效, 将寄存器中的内存基地址保存在 Vj 中;
         * 对于 beqz 和 j 指令, 将当前 PC+1 的值保存在 Vk 字段中.
         * 如果指令在提交时会修改寄存器的值, 还需要在这里更新寄存器状态数据结构.
         */
        auto instr = memory[pc];
        auto op = opcode(instr);
        auto& reservEntry = reservation[unit];
        auto& robEntry = rob[robIdx];
        reservEntry.busy = true;
        reservEntry.robIdx = robIdx;
        reservEntry.instr = instr;
        robEntry.busy = true;
        robEntry.instr = instr;
        robEntry.instrStatus = ISSUING;
        robEntry.execUnit = unit;
        robEntry.pc = pc;

        word exTimeLeft = 0;
        switch (op) {
        case NOOP:
        case HALT:
            reservEntry.exTimeLeft = INTEXEC;
            return;
        case RR_ALU:
        case ADDI:
        case ANDI:
        case J:
        case SW:
            exTimeLeft = INTEXEC;
            break;
        case LW:
            exTimeLeft = LDEXEC;
            break;
        case BEQZ:
            exTimeLeft = BRANCHEXEC;
            break;
        default:
            throw TomasuloError("Invalid op:", op, "at pc=", pc);
        }
        reservEntry.exTimeLeft = exTimeLeft;

        switch (op) {
        case RR_ALU: {
            auto rs1 = reg1(instr);
            auto rs2 = reg2(instr);
            auto rd = reg3(instr);

            auto rg1 = regResult[rs1];
            auto rg2 = regResult[rs2];

            auto& rg3 = regResult[rd];
            rg3.valid = false;
            rg3.robIdx = robIdx;

            if (rg1.valid) {
                reservEntry.Qj = READY;
                reservEntry.Vj = regFile[rs1];
            } else {
                const auto& rg1Rob = rob[rg1.robIdx];
                if (rg1Rob.valid) {
                    reservEntry.Qj = READY;
                    reservEntry.Vj = rg1Rob.result;
                } else {
                    reservEntry.Qj = rg1Rob.execUnit;
                }
            }
            if (rg2.valid) {
                reservEntry.Qk = READY;
                reservEntry.Vk = regFile[rs2];
            } else {
                const auto& rg2Rob = rob[rg2.robIdx];
                if (rg2Rob.valid) {
                    reservEntry.Qk = READY;
                    reservEntry.Vk = rg2Rob.result;
                } else {
                    reservEntry.Qk = rg2Rob.execUnit;
                }
            }
            break;
        }
        case LW:
        case SW:
        case ADDI:
        case ANDI:
        case BEQZ: {
            auto rs1 = reg1(instr);
            auto rd = reg2(instr);

            auto rg1 = regResult[rs1];
            auto& rg2 = regResult[rd];
            switch (op) {
            case LW:
            case ADDI:
            case ANDI:
            case BEQZ: {
                rg2.valid = false;
                rg2.robIdx = robIdx;
                if (rg1.valid) {
                    reservEntry.Qj = READY;
                    reservEntry.Vj = regFile[rs1];
                } else {
                    const auto& rg1Rob = rob[rg1.robIdx];
                    if (rg1Rob.valid) {
                        reservEntry.Qj = READY;
                        reservEntry.Vj = rg1Rob.result;
                    } else {
                        reservEntry.Qj = rg1Rob.execUnit;
                    }
                }
                break;
            }
            case SW: {
                if (rg1.valid) {
                    reservEntry.Qj = READY;
                    reservEntry.Vj = regFile[rs1];
                } else {
                    const auto& rg1Rob = rob[rg1.robIdx];
                    if (rg1Rob.valid) {
                        reservEntry.Qj = READY;
                        reservEntry.Vj = rg1Rob.result;
                    } else {
                        reservEntry.Qj = rg1Rob.execUnit;
                    }
                }
                if (rg2.valid) {
                    reservEntry.Qk = READY;
                    reservEntry.Vk = regFile[rd];
                } else {
                    const auto& rg2Rob = rob[rg2.robIdx];
                    if (rg2Rob.valid) {
                        reservEntry.Qk = READY;
                        reservEntry.Vk = rg2Rob.result;
                    } else {
                        reservEntry.Qk = rg2Rob.execUnit;
                    }
                }
                break;
            }
            default:
                __builtin_unreachable();
            }
            break;
        }
        case J: {
            reservEntry.Qk = READY;
            reservEntry.Vk = pc + 1;
            break;
        }
        default:
            __builtin_unreachable();
        }
    }

    void updateBTB(word branchPc, word targetPc, bool taken) {
        /*
         * 更新分支预测缓冲栈: 检查是否与缓冲栈中的项目匹配.
         * 如果是, 对 2-bit 的历史记录进行更新;
         * 如果不是, 将当前的分支语句添加到缓冲栈中去.
         * 如果缓冲栈已满，你需要选择一种替换算法将旧的记录替换出去.
         * 如果当前跳转成功, 将初始的历史状态设置为 STRONGTAKEN;
         * 如果不成功, 将历史设置为 STRONGNOT
         */
        size_t victimIdx = -1;
        for (size_t i = 0; i < btb.size(); ++i) {
            auto& entry = btb[i];
            if (entry.valid) {
                if (entry.branchPc == branchPc && entry.targetPc == targetPc) {
                    entry.branchPred = newBHT(entry.branchPred, taken);
                    return;
                }
            } else {
                victimIdx = i;
            }
        }
        if (victimIdx == (size_t)-1) {
            victimIdx = randBy(std::uniform_int_distribution<size_t>(0, btb.size() - 1));
        }

        auto& victim = btb[victimIdx];
        victim = {
            .valid = true,
            .branchPred = taken ? BHT::STRONGTAKEN : BHT::STRONGNOT,
            .branchPc = branchPc,
            .targetPc = targetPc,
        };
    }

    word getTarget(word branchPc) const {
        /*
         * 检查分支指令是否已保存在分支预测缓冲栈中:
         * 如果不是, 返回当前 pc+1, 这意味着我们预测分支跳转不会成功;
         * 如果在, 并且历史信息为 STRONGTAKEN 或 WEAKTAKEN, 返回跳转的目标地址,
         * 如果历史信息为 STRONGNOT 或 WEAKNOT, 返回当前 pc+1.
         */
        for (const auto& pred : btb) {
            if (pred.valid && pred.branchPc == branchPc) {
                switch (pred.branchPred) {
                case BHT::STRONGNOT:
                case BHT::WEAKNOT:
                    return branchPc + 1;
                case BHT::WEAKTAKEN:
                case BHT::STRONGTAKEN:
                    return pred.targetPc;
                default:
                    __builtin_unreachable();
                }
            }
        }
        return branchPc + 1;
    }

    size_t robHead() const {
        //* 返回 ROB 头指针, 如不存在, 返回全 1
        return robHeadIdx == robTailIdx ? (size_t)-1 : robHeadIdx;
    }

    size_t robPop() {
        //* 相当于在循环队列中 `popFront`
        if (robHeadIdx == robTailIdx)
            return (size_t)-1;
        auto ret = robHeadIdx;
        rob[ret] = {};
        robHeadIdx += 1;
        robHeadIdx %= ROBSIZE;
        return ret;
    }

    size_t robPush() {
        //* 相当于在循环队列中 `pushBack`
        if ((robHeadIdx - robTailIdx) % ROBSIZE == 1)
            return (size_t)-1;
        auto ret = robTailIdx;
        robTailIdx += 1;
        robTailIdx %= ROBSIZE;
        return ret;
    }

    void loadInstr(word pc, const char* instr) {
        //* 加载一条指令至给定位置, 用来和可视化代码交互
        memcpy(&memory[pc], instr, sizeof(word));
    }

    void setMemorySize(word size) {
        //* 设置内存的可用区间大小, 用来和可视化代码交互
        memorySize = size;
    }

    void commitInstr(word robIdx) {
        //* 提交一条指令, 视指令类型造成相应的后果
        auto& robEntry = rob[robIdx];
        auto op = opcode(robEntry.instr);
        auto result = robEntry.result;
        auto instr = robEntry.instr;
        switch (op) {
        case LW:
        case ADDI:
        case ANDI: {
            auto rd = reg2(instr);
            if (regResult[rd].robIdx == robIdx) {
                regResult[rd] = {};
            }
            regFile[rd] = result;
            robPop();
            return;
        }
        case RR_ALU: {
            auto rd = reg3(instr);
            if (regResult[rd].robIdx == robIdx) {
                regResult[rd] = {};
            }
            regFile[rd] = result;
            robPop();
            return;
        }
        case BEQZ: {
            auto branchTarget = immEx(instr) + 1 + robEntry.pc;
            auto taken = result == 0;
            updateBTB(robEntry.pc, branchTarget, taken);
            if ((taken && robEntry.address != branchTarget) || (!taken && robEntry.address != robEntry.pc + 1)) {
                resetROB();
                resetReserve();
                resetRegResult();
                pc = branchTarget;
            } else {
                robPop();
            }
            return;
        }
        case SW: {
            auto unit = robEntry.execUnit;
            if (unit != STORE1 && unit != STORE2) {
                for (auto reservIdx : {STORE1, STORE2}) {
                    auto& reserv = reservation[reservIdx];
                    if (!reserv.busy) {
                        reserv = {
                            .busy = true,
                            .instr = instr,
                            .Vj = result,
                            .Vk = robEntry.address,
                            .Qj = READY,
                            .Qk = READY,
                            .exTimeLeft = STEXEC - 1,
                            .robIdx = robIdx,
                        };
                        robEntry.execUnit = reservIdx;
                        return;
                    }
                }
            } else if (reservation[unit].exTimeLeft == 0) {
                auto value = reservation[unit].Vj;
                auto address = reservation[unit].Vk;
                memory[address] = value;
                reservation[unit] = {};
                robPop();
            } else {
                reservation[unit].exTimeLeft -= 1;
            }
            break;
        }
        case J:
            robPop();
        default:
            return;
        }
    }

    word getResult(word reservIdx) {
        //* 模拟执行完毕了得到结果
        const auto& reserv = reservation[reservIdx];
        auto instr = reserv.instr;
        auto op = opcode(instr);
        auto imm16 = immEx(instr);
        auto imm26 = jmpOffsetEx(instr);
        auto funccode = func(instr);

        switch (op) {
        case ANDI:
            return reserv.Vj & imm16;
        case ADDI:
            return reserv.Vj + imm16;
        case RR_ALU:
            switch (funccode) {
            case FUNC_ADD:
                return reserv.Vj + reserv.Vk;
            case FUNC_SUB:
                return reserv.Vj - reserv.Vk;
            case FUNC_AND:
                return reserv.Vj & reserv.Vk;
            default:
                __builtin_unreachable();
            }
        case LW:
            return memory[reserv.Vj + imm16];
        case SW:
            return reserv.Vk;
        case BEQZ:
            return reserv.Vj;
        case J:
            return imm26;
        default:
            return 0;
        }
    }

    bool nextStep() {
        //* 模拟时钟前进
        cycles += 1;
        // committing
        if (auto head = robHead(); head != (size_t)-1) {
            const auto& robEntry = rob[head];
            if (robEntry.busy && robEntry.valid && robEntry.instrStatus == COMMITTING) {
                if (opcode(robEntry.instr) != HALT)
                    commitInstr(head);
                else {
                    robPop();
                    return true;
                }
            }
        }

        // processing
        for (size_t robIdx = 0; robIdx < rob.size(); ++robIdx) {
            auto& robEntry = rob[robIdx];
            auto unit = robEntry.execUnit;
            auto& reserv = reservation[unit];
            auto instr = robEntry.instr;

            if (robEntry.busy) {
                if (robEntry.instrStatus == EXECUTING) {
                    if (reserv.exTimeLeft != 0)
                        reserv.exTimeLeft -= 1;
                    else {
                        robEntry.instrStatus = WRITING_RESULT;
                        if (opcode(instr) == SW) {
                            robEntry.address = reserv.Vj + immEx(instr);
                        }
                        broadcastUpdate(unit, getResult(unit));
                        reserv = {};
                    }
                } else if (robEntry.instrStatus == WRITING_RESULT) {
                    robEntry.instrStatus = COMMITTING;
                } else if (robEntry.instrStatus == ISSUING && reserv.Qj == READY && reserv.Qk == READY) {
                    robEntry.instrStatus = EXECUTING;
                    reserv.exTimeLeft -= 1;
                }
            }
        }

        // issuing
        if (pc >= memorySize)
            return false;
        auto instr = memory[pc];
        auto op = opcode(instr);
        word unit = INVALID;
        switch (op) {
        case RR_ALU:
        case SW:
        case ADDI:
        case ANDI:
        case J:
        case HALT:
        case NOOP:
        case BEQZ:
            for (auto idx : {INT1, INT2}) {
                if (!reservation[idx].busy) {
                    unit = idx;
                    break;
                }
            }
            break;
        case LW:
            for (auto idx : {LOAD1, LOAD2}) {
                if (!reservation[idx].busy) {
                    unit = idx;
                    break;
                }
            }
            break;
        default:
            throw TomasuloError("Invalid op:", op, "pc:", pc);
        }

        if (unit != INVALID) {
            auto robIdx = robPush();
            if (robIdx != (size_t)-1) {
                issueInstr(pc, unit, robIdx);
                if (op == BEQZ) {
                    pc = getTarget(pc);
                    rob[robIdx].address = pc;
                } else if (op == J) {
                    pc += jmpOffsetEx(instr) + 1;
                } else if (pc < memorySize - 1) {
                    pc += 1;
                }
            }
        }

        return false;
    }
};

inline void printState(const MachineState* state, word memorySize) {
    word i;

    printf("Cycles: %d\n", state->cycles);

    printf("\tpc = %d\n", state->pc);

    printf("\tReservation stations:\n");
    for (i = 0; i < NUMUNITS; i++) {
        if (state->reservation[i].busy == true) {
            printf("\t\tReservation station %d: ", i);
            if (state->reservation[i].Qj == 0) {
                printf("Vj = %d ", state->reservation[i].Vj);
            } else {
                printf("Qj = '%s' ", unitname[state->reservation[i].Qj - 1]);
            }
            if (state->reservation[i].Qk == 0) {
                printf("Vk = %d ", state->reservation[i].Vk);
            } else {
                printf("Qk = '%s' ", unitname[state->reservation[i].Qk - 1]);
            }
            printf(" ExTimeLeft = %d  ROB Index = %d\n", state->reservation[i].exTimeLeft,
                   state->reservation[i].robIdx);
        }
    }

    printf("\tReorder buffers:\n");
    for (i = 0; i < ROBSIZE; i++) {
        if (state->rob[i].busy == true) {
            printf("\t\tReorder buffer %d: ", i);
            printf("instr %d  executionUnit '%s'  state %s  valid %d  result %d address %d\n", state->rob[i].instr,
                   unitname[state->rob[i].execUnit - 1], statename[state->rob[i].instrStatus], state->rob[i].valid,
                   state->rob[i].result, state->rob[i].address);
        }
    }

    printf("\tRegister result status:\n");
    for (i = 1; i < NUMREGS; i++) {
        if (!state->regResult[i].valid) {
            printf("\t\tRegister %d: ", i);
            printf("waiting for ROB index %d\n", state->regResult[i].robIdx);
        }
    }

    /*
     * [TODO]如果你实现了动态分支预测, 将这里的注释取消
     */

    printf("\tBranch target buffer:\n");
    for (i = 0; i < BTBSIZE; i++) {
        if (state->btb[i].valid) {
            printf("\t\tEntry %d: PC=%d, Target=%d, Pred=%d\n", i, state->btb[i].branchPc, state->btb[i].targetPc,
                   state->btb[i].branchPred);
        }
    }

    printf("\tMemory:\n");
    for (i = 0; i < memorySize; i++) {
        printf("\t\tmemory[%d] = %d\n", i, state->memory[i]);
    }

    printf("\tRegisters:\n");
    for (i = 0; i < NUMREGS; i++) {
        printf("\t\tregFile[%d] = %d\n", i, state->regFile[i]);
    }
}
