#pragma once
#include <cstdint>
#include <functional>
struct Clock {
    int64_t tstates = 0;
    int timeout = 0;
    std::function<void()> listener;
    int64_t getTstates() const { return tstates; }
    void addTstates(int states) {
        tstates += states;
        if (timeout > 0) { timeout -= states; if (timeout < 1 && listener) listener(); }
    }
    void setTimeout(int states) { timeout = states > 0 ? states : 1; }
    void reset() { tstates = 0; timeout = 0; }
};
struct MemIoOps {
    virtual ~MemIoOps() {}
    virtual int fetchOpcode(int) = 0;
    virtual int peek8(int) = 0;
    virtual void poke8(int,int) = 0;
    virtual int peek16(int) = 0;
    virtual void poke16(int,int) = 0;
    virtual int intAck(int) = 0;
    virtual int inPort(int) = 0;
    virtual void outPort(int,int) = 0;
    virtual void contendedStates(int,int64_t) = 0;
};
struct NotifyOps {
    virtual ~NotifyOps() {}
    virtual int atAddress(int,int opcode) { return opcode; }
    virtual void execDone() {}
};
