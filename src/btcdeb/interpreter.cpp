// Copyright (c) 2018 Karl-Johan Alm
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <btcdeb/interpreter.h>
#include <btcdeb/script.h>

InterpreterEnv::InterpreterEnv(std::vector<valtype>& stack_in, const CScript& script_in, unsigned int flags_in, const BaseSignatureChecker& checker_in, SigVersion sigversion_in, ScriptError* error_in)
: ScriptExecutionEnvironment(script_in, stack_in, flags_in, checker_in)
, pc(script_in.begin())
, scriptIn(script_in)
, curr_op_seq(0)
, done(pc == pend)
{
    sigversion = sigversion_in;
    serror = error_in;

    operational = true;
    set_error(serror, SCRIPT_ERR_UNKNOWN_ERROR);
    if (script.size() > MAX_SCRIPT_SIZE) {
        set_error(serror, SCRIPT_ERR_SCRIPT_SIZE);
        operational = false;
        return;
    }
    nOpCount = 0;
    fRequireMinimal = (flags & SCRIPT_VERIFY_MINIMALDATA) != 0;
    // figure out if p2sh
    is_p2sh = (
        script.size() == 23 &&
        script[0] == OP_HASH160 &&
        script[1] == 20 &&
        script[22] == OP_EQUAL
    );
    if (is_p2sh) {
        // we have "executed" the sigscript already (in the form of pushes onto the stack),
        // so we need to copy the stack here
        p2shstack = stack_in;
    }
}

bool CastToBool(const valtype& vch);

bool StepScript(InterpreterEnv& env)
{
    auto& pend = env.pend;
    auto& pc = env.pc;

    if (pc < pend) {
        return StepScript(env, pc);
    }

    auto& vfExec = env.vfExec;
    auto& script = env.script;
    auto& stack = env.stack;
    auto& is_p2sh = env.is_p2sh;
    auto& serror = env.serror;

    if (is_p2sh) {
        if (stack.empty())
            return set_error(serror, SCRIPT_ERR_EVAL_FALSE);
        if (CastToBool(stack.back()) == false)
            return set_error(serror, SCRIPT_ERR_EVAL_FALSE);
        // Additional validation for spend-to-script-hash transactions:
        if (env.scriptIn.IsPayToScriptHash()) {
            printf("Drop-in P2SH redeem script\n");
            // // scriptSig must be literals-only or validation fails
            // if (!scriptSig.IsPushOnly())
            //     return set_error(serror, SCRIPT_ERR_SIG_PUSHONLY);

            // Restore stack.
            is_p2sh = false;
            stack = env.p2shstack;
            // swap(stack, stackCopy);

            // stack cannot be empty here, because if it was the
            // P2SH  HASH <> EQUAL  scriptPubKey would be evaluated with
            // an empty stack and the EvalScript above would return false.
            assert(!stack.empty());

            const valtype& pubKeySerialized = stack.back();
            CScript pubKey2(pubKeySerialized.begin(), pubKeySerialized.end());
            script = pubKey2;
            popstack(stack);

            printf("Restoring stack:\n");
            for (auto& it : stack) {
                printf("- %s\n", HexStr(it).c_str());
            }
            printf("Script:\n");
            CScript::const_iterator it = script.begin();
            valtype vchPushValue;
            opcodetype opcode;
            while (script.GetOp(it, opcode, vchPushValue)) {
                if (vchPushValue.size() > 0) {
                    printf("- %s\n", HexStr(vchPushValue).c_str());
                } else {
                    printf("- %s\n", GetOpName(opcode));
                }
            }

            pc = env.pbegincodehash = script.begin();
            pend = script.end();
            return true; // ExecIterator(env, script, pc, true);
        }
        return set_error(serror, SCRIPT_ERR_BAD_OPCODE);
    }

    // we are at end; set done var
    env.done = true;

    if (!vfExec.empty())
        return set_error(serror, SCRIPT_ERR_UNBALANCED_CONDITIONAL);

    return set_success(serror);
}
