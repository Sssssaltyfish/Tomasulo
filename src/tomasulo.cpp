#include <cstdint>
#include <iostream>

#include <stdarg.h>

#include "decode.hpp"
#include "defines.hpp"
#include "error.hpp"
#include "state.hpp"

#include "pybind11/attr.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace py = pybind11;

#define d_cls(prop, cls) c.def_readwrite(#prop, &cls::prop);

PYBIND11_MODULE(tomasulo, m) {
    m.doc() = "a naive c++ implementation of Tomasulo algorithm";

    py::enum_<BHT>(m, "BHT")
        .value("STRONGNOT", BHT::STRONGNOT)
        .value("WEAKNOT", BHT::WEAKNOT)
        .value("WEAKTAKEN", BHT::WEAKTAKEN)
        .value("STRONGTAKEN", BHT::STRONGTAKEN);
    {
        auto c = py::class_<ResStation>(m, "ResStation").def(py::init());
#define d(prop) d_cls(prop, ResStation)
        d(busy);
        d(instr);
        d(Vj);
        d(Vk);
        d(Qj);
        d(Qk);
        d(exTimeLeft);
        d(robIdx);
#undef d
    }
    {
        auto c = py::class_<ROBEntry>(m, "ROBEntry").def(py::init());
#define d(prop) d_cls(prop, ROBEntry)
        d(busy);
        d(valid);
        d(pc);
        d(instr);
        d(execUnit);
        d(instrStatus);
        d(result);
        d(address);
#undef d
    }
    {
        auto c = py::class_<RegResultEntry>(m, "RegResultEntry").def(py::init());
#define d(prop) d_cls(prop, RegResultEntry)
        d(valid);
        d(robIdx);
#undef d
    }
    {
        auto c = py::class_<BTBEntry>(m, "BTBEntry").def(py::init());
#define d(prop) d_cls(prop, BTBEntry)
        d(valid);
        d(branchPred);
        d(branchPc);
        d(targetPc);
#undef d
    }
    {
        auto c = py::class_<MachineState>(m, "MachineState").def(py::init());

        c.doc() = "class that represents state of the machine";
        c.def("__copy__", [](const MachineState& self) { return decltype(self)(self); });
        c.def("copy", [](const MachineState& self) { return decltype(self)(self); });
        c.def("__deepcopy__", [](const MachineState& self, py::dict) { return decltype(self)(self); });
        c.def("nextStep", &MachineState::nextStep);
        c.def("loadInstr", &MachineState::loadInstr);
        c.def("setMemorySize", &MachineState::setMemorySize);

#define d(prop) d_cls(prop, MachineState)
        d(pc);
        d(cycles);
        d(reservation);
        d(rob);
        d(btb);
        d(memory);
        d(regFile);
        d(regResult);
#undef d
    }

    m.def("printState", &printState, "print the state of given `MachineState`");
    py::register_exception<TomasuloError>(m, "TomasuloError");
}

#undef d_cls
