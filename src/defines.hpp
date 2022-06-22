#pragma once

#include <array>
#include <cstdint>
#include <cstdio>

using word = uint32_t;

constexpr word MAXLINELENGTH = 1000; /* 机器指令的最大长度 */
constexpr word MEMSIZE = 10000;      /* 内存的最大容量     */
constexpr word NUMREGS = 32;         /* 寄存器数量         */

constexpr word INVALID = (word)-1;

/*
 * 操作码和功能码定义
 */

constexpr word RR_ALU = 0; /* 寄存器-寄存器的 ALU 运算的操作码为 0 */
constexpr word LW = 35;
constexpr word SW = 43;
constexpr word ADDI = 8;
constexpr word ANDI = 12;
constexpr word BEQZ = 4;
constexpr word J = 2;
constexpr word HALT = 1;
constexpr word NOOP = 3;

/* ALU 运算的功能码 */
constexpr word FUNC_ADD = 32;
constexpr word FUNC_SUB = 34;
constexpr word FUNC_AND = 36;

constexpr word NOOP_INSTR = 0x0c000000;
;

/*
 * 执行单元
 */
constexpr word READY = 0;
constexpr word LOAD1 = 1;
constexpr word LOAD2 = 2;
constexpr word STORE1 = 3;
constexpr word STORE2 = 4;
constexpr word INT1 = 5;
constexpr word INT2 = 6;

constexpr word NUMUNITS = 6;                                                                    /* 执行单元数量 */
inline const char* unitname[NUMUNITS] = {"LOAD1", "LOAD2", "STORE1", "STORE2", "INT1", "INT2"}; /* 执行单元的名称 */

/*
 * 不同操作所需要的周期数
 */
constexpr word BRANCHEXEC = 3; /* 分支操作 */
constexpr word LDEXEC = 2;     /* Load     */
constexpr word STEXEC = 2;     /* Store    */
constexpr word INTEXEC = 1;    /* 整数运算 */

/*
 * 指令状态
 */
constexpr word ISSUING = 0;                                                                 /* 发射   */
constexpr word EXECUTING = 1;                                                               /* 执行   */
constexpr word WRITING_RESULT = 2;                                                           /* 写结果 */
constexpr word COMMITTING = 3;                                                              /* 提交   */
inline const char* statename[4] = {"ISSUING", "EXECUTING", "WRITINGRESULT", "COMMTITTING"}; /*  状态名称 */

constexpr word ROBSIZE = 16; /* ROB 有 16 个单元 */
constexpr word BTBSIZE = 8;  /* 分支预测缓冲栈有 8 个单元 */

/*
 * 2 bit 分支预测状态
 */
enum BHT {
    STRONGNOT = 0,
    WEAKNOT = 1,
    WEAKTAKEN = 2,
    STRONGTAKEN = 3,
};
/*
 * 分支跳转结果
 */
constexpr bool NOTTAKEN = false;
constexpr bool TAKEN = true;

struct ResStation { /* 保留栈的数据结构 */
    bool busy;      /* 空闲标志位 */
    word instr;     /*    指令    */
    word Vj;        /* Vj, Vk 存放操作数 */
    word Vk;
    word Qj;         /* Qj, Qk 存放将会生成结果的执行单元编号 */
    word Qk;         /* 为零则表示对应的 V 有效 */
    word exTimeLeft; /* 指令执行的剩余时间 */
    word robIdx;     /* 该指令对应的 ROB 项编号 */
};

struct ROBEntry {     /* ROB 项的数据结构 */
    bool busy;        /* 空闲标志位 */
    bool valid;       /* 表明结果是否有效的标志位 */
    word pc;
    word instr;       /* 指令 */
    word execUnit;    /* 执行单元编号 */
    word instrStatus; /* 指令的当前状态 */
    word result;      /* 在提交之前临时存放结果 */
    word address;     /* store 指令的内存地址, 也用作 `beqz` 的预测地址 */
};

struct RegResultEntry { /* 寄存器状态的数据结构 */
    bool valid = true;  /* 1 表示寄存器值有效, 否则 0 */
    word robIdx{};      /* 如果值无效, 记录 ROB 中哪个项目会提交结果 */
};

struct BTBEntry {
    bool valid;     /* 有效位 */
    BHT branchPred; /* 预测: 2-bit 分支历史 */
    word branchPc;  /* 分支指令的 PC 值 */
    word targetPc;  /* when predict taken, update PC with target */
};
