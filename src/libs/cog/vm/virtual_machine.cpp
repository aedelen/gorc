#include "virtual_machine.hpp"
#include "opcode.hpp"
#include "continuation.hpp"
#include "restart_exception.hpp"
#include "suspend_exception.hpp"

gorc::cog::value gorc::cog::virtual_machine::internal_execute(verb_table &verbs,
                                                              service_registry &services,
                                                              continuation &cc)
{
    if(cc.call_stack.empty()) {
        // Cannot execute in empty continuation
        return value();
    }

    // Add current continuation to services.
    // System verbs must modify the current continuation.
    services.add_or_replace(cc);

    memory_file::reader sr(cc.frame().cog.program);
    sr.set_position(cc.frame().program_counter);

    while(true) {
        opcode op = read<opcode>(sr);
        switch(op) {
        case opcode::push: {
                cog::value v(deserialization_constructor, sr);
                cc.data_stack.push(v);
            }
            break;

        case opcode::dup: {
                cog::value v(cc.data_stack.top());
                cc.data_stack.push(v);
            }
            break;

        case opcode::load: {
                size_t addr = read<size_t>(sr);
                cc.data_stack.push(cc.frame().memory[addr]);
            }
            break;

        case opcode::loadi: {
                size_t addr = read<size_t>(sr);
                int idx = static_cast<int>(cc.data_stack.top());
                cc.data_stack.pop();

                cc.data_stack.push(cc.frame().memory[addr + idx]);
            }
            break;

        case opcode::stor: {
                size_t addr = read<size_t>(sr);
                cc.frame().memory[addr] = cc.data_stack.top();
                cc.data_stack.pop();
            }
            break;

        case opcode::stori: {
                size_t addr = read<size_t>(sr);
                int idx = static_cast<int>(cc.data_stack.top());
                cc.data_stack.pop();

                cc.frame().memory[addr + idx] = cc.data_stack.top();
                cc.data_stack.pop();
            }
            break;

        case opcode::jmp: {
                size_t addr = read<size_t>(sr);
                sr.set_position(addr);
            }
            break;

        case opcode::jal: {
                size_t addr = read<size_t>(sr);

                // Store current offset in current continuation
                cc.call_stack.top().program_counter = sr.position();

                // Create new stack frame
                cc.call_stack.push(call_stack_frame(cc.frame().cog,
                                                    cc.frame().memory,
                                                    addr,
                                                    cc.frame().sender,
                                                    cc.frame().source,
                                                    cc.frame().param0,
                                                    cc.frame().param1,
                                                    cc.frame().param2,
                                                    cc.frame().param3));

                // Jump
                sr.set_position(addr);
            }
            break;

        case opcode::bt: {
                size_t addr = read<size_t>(sr);
                cog::value v(cc.data_stack.top());
                cc.data_stack.pop();

                if(static_cast<bool>(v)) {
                    sr.set_position(addr);
                }
            }
            break;

        case opcode::bf: {
                size_t addr = read<size_t>(sr);
                cog::value v(cc.data_stack.top());
                cc.data_stack.pop();

                if(!static_cast<bool>(v)) {
                    sr.set_position(addr);
                }
            }
            break;

        case opcode::call: {
                int vid = read<int>(sr);

                // Store current offset in current continuation
                cc.call_stack.top().program_counter = sr.position();

                verbs.get_verb(verb_id(vid)).invoke(cc.data_stack,
                                                    services,
                                                    /* expects value */ false);
            }
            break;

        case opcode::callv: {
                int vid = read<int>(sr);

                // Store current offset in current continuation
                cc.call_stack.top().program_counter = sr.position();

                cog::value rv = verbs.get_verb(verb_id(vid)).invoke(cc.data_stack,
                                                                    services,
                                                                    /* expects value */ true);
                cc.data_stack.push(rv);
            }
            break;

        case opcode::ret: {
                // Retire top stack frame.
                value return_register = cc.frame().return_register;
                bool save_return_register = cc.frame().save_return_register;
                bool push_return_register = cc.frame().push_return_register;

                cc.call_stack.pop();

                if(cc.call_stack.empty()) {
                    return return_register;
                }

                sr = memory_file::reader(cc.call_stack.top().cog.program);
                sr.set_position(cc.call_stack.top().program_counter);

                if(save_return_register) {
                    cc.frame().return_register = return_register;
                }

                if(push_return_register) {
                    cc.data_stack.push(return_register);
                }
            }
            break;

        case opcode::neg: {
                cog::value v = cc.data_stack.top();
                cc.data_stack.pop();
                cc.data_stack.push(-v);
            }
            break;

        case opcode::lnot: {
                cog::value v = cc.data_stack.top();
                cc.data_stack.pop();
                cc.data_stack.push(!v);
            }
            break;

        case opcode::add: {
                cog::value y = cc.data_stack.top();
                cc.data_stack.pop();
                cog::value x = cc.data_stack.top();
                cc.data_stack.pop();
                cc.data_stack.push(x + y);
            }
            break;

        case opcode::sub: {
                cog::value y = cc.data_stack.top();
                cc.data_stack.pop();
                cog::value x = cc.data_stack.top();
                cc.data_stack.pop();
                cc.data_stack.push(x - y);
            }
            break;

        case opcode::mul: {
                cog::value y = cc.data_stack.top();
                cc.data_stack.pop();
                cog::value x = cc.data_stack.top();
                cc.data_stack.pop();
                cc.data_stack.push(x * y);
            }
            break;

        case opcode::div: {
                cog::value y = cc.data_stack.top();
                cc.data_stack.pop();
                cog::value x = cc.data_stack.top();
                cc.data_stack.pop();
                cc.data_stack.push(x / y);
            }
            break;

        case opcode::mod: {
                cog::value y = cc.data_stack.top();
                cc.data_stack.pop();
                cog::value x = cc.data_stack.top();
                cc.data_stack.pop();
                cc.data_stack.push(x % y);
            }
            break;

        case opcode::bor: {
                cog::value y = cc.data_stack.top();
                cc.data_stack.pop();
                cog::value x = cc.data_stack.top();
                cc.data_stack.pop();
                cc.data_stack.push(x | y);
            }
            break;

        case opcode::band: {
                cog::value y = cc.data_stack.top();
                cc.data_stack.pop();
                cog::value x = cc.data_stack.top();
                cc.data_stack.pop();
                cc.data_stack.push(x & y);
            }
            break;

        case opcode::bxor: {
                cog::value y = cc.data_stack.top();
                cc.data_stack.pop();
                cog::value x = cc.data_stack.top();
                cc.data_stack.pop();
                cc.data_stack.push(x ^ y);
            }
            break;

        case opcode::lor: {
                cog::value y = cc.data_stack.top();
                cc.data_stack.pop();
                cog::value x = cc.data_stack.top();
                cc.data_stack.pop();
                cc.data_stack.push(x || y);
            }
            break;

        case opcode::land: {
                cog::value y = cc.data_stack.top();
                cc.data_stack.pop();
                cog::value x = cc.data_stack.top();
                cc.data_stack.pop();
                cc.data_stack.push(x && y);
            }
            break;

        case opcode::eq: {
                cog::value y = cc.data_stack.top();
                cc.data_stack.pop();
                cog::value x = cc.data_stack.top();
                cc.data_stack.pop();
                cc.data_stack.push(x == y);
            }
            break;

        case opcode::ne: {
                cog::value y = cc.data_stack.top();
                cc.data_stack.pop();
                cog::value x = cc.data_stack.top();
                cc.data_stack.pop();
                cc.data_stack.push(x != y);
            }
            break;

        case opcode::gt: {
                cog::value y = cc.data_stack.top();
                cc.data_stack.pop();
                cog::value x = cc.data_stack.top();
                cc.data_stack.pop();
                cc.data_stack.push(x > y);
            }
            break;

        case opcode::ge: {
                cog::value y = cc.data_stack.top();
                cc.data_stack.pop();
                cog::value x = cc.data_stack.top();
                cc.data_stack.pop();
                cc.data_stack.push(x >= y);
            }
            break;

        case opcode::lt: {
                cog::value y = cc.data_stack.top();
                cc.data_stack.pop();
                cog::value x = cc.data_stack.top();
                cc.data_stack.pop();
                cc.data_stack.push(x < y);
            }
            break;

        case opcode::le: {
                cog::value y = cc.data_stack.top();
                cc.data_stack.pop();
                cog::value x = cc.data_stack.top();
                cc.data_stack.pop();
                cc.data_stack.push(x <= y);
            }
            break;
        }
    }
}

gorc::cog::value gorc::cog::virtual_machine::execute(verb_table &verbs,
                                                     service_registry &services,
                                                     continuation &cc)
{
    while(true) {
        try {
            return internal_execute(verbs, services, cc);
        }
        catch(restart_exception const &) {
            // Some engine component has changed the current continuation and
            // is requesting immediate out-of-band execution restart.
            continue;
        }
        catch(suspend_exception const &) {
            // Some engine component has requested immediate out-of-band execution
            // suspension. The current context is stored in the continuation.
            break;
        }

        break;
    }

    // Execution has halted out-of-band. Use whatever value is in the current
    // return register.
    return cc.frame().return_register;
}
