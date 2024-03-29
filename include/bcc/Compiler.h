/*
 * Copyright 2010-2012, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef BCC_COMPILER_H
#define BCC_COMPILER_H

namespace llvm {

class raw_ostream;
class PassManager;
class TargetData;
class TargetMachine;

} // end namespace llvm

namespace bcc {

class CompilerConfig;
class OutputFile;
class Script;

//===----------------------------------------------------------------------===//
// Design of Compiler
//===----------------------------------------------------------------------===//
// 1. A compiler instance can be constructed provided an "initial config."
// 2. A compiler can later be re-configured using config().
// 3. Once config() is invoked, it'll re-create TargetMachine instance (i.e.,
//    mTarget) according to the configuration supplied. TargetMachine instance
//    is *shared* across the different calls to compile() before the next call
//    to config().
// 4. Once a compiler instance is created, you can use the compile() service
//    to compile the file over and over again. Each call uses TargetMachine
//    instance to construct the compilation passes.
class Compiler {
public:
  enum ErrorCode {
    kSuccess,

    kInvalidConfigNoTarget,
    kErrCreateTargetMachine,
    kErrSwitchTargetMachine,
    kErrNoTargetMachine,
    kErrTargetDataNoMemory,
    kErrMaterialization,
    kErrInvalidOutputFileState,
    kErrPrepareOutput,
    kPrepareCodeGenPass,

    kErrHookBeforeAddLTOPasses,
    kErrHookAfterAddLTOPasses,
    kErrHookBeforeExecuteLTOPasses,
    kErrHookAfterExecuteLTOPasses,

    kErrHookBeforeAddCodeGenPasses,
    kErrHookAfterAddCodeGenPasses,
    kErrHookBeforeExecuteCodeGenPasses,
    kErrHookAfterExecuteCodeGenPasses,

    kMaxErrorCode,
  };

  static const char *GetErrorString(enum ErrorCode pErrCode);

private:
  llvm::TargetMachine *mTarget;
  // LTO is enabled by default.
  bool mEnableLTO;

  enum ErrorCode runLTO(Script &pScript);
  enum ErrorCode runCodeGen(Script &pScript, llvm::raw_ostream &pResult);

public:
  Compiler();
  Compiler(const CompilerConfig &pConfig);

  enum ErrorCode config(const CompilerConfig &pConfig);

  // Compile a script and output the result to a LLVM stream.
  enum ErrorCode compile(Script &pScript, llvm::raw_ostream &pResult);

  // Compile a script and output the result to a file.
  enum ErrorCode compile(Script &pScript, OutputFile &pResult);

  const llvm::TargetMachine& getTargetMachine() const
  { return *mTarget; }

  void enableLTO(bool pEnable = true)
  { mEnableLTO = pEnable; }

  virtual ~Compiler();

protected:
  //===--------------------------------------------------------------------===//
  // Plugin callbacks for sub-class.
  //===--------------------------------------------------------------------===//
  // Called before adding first pass to code-generation passes.
#ifdef PVR_RSC
  virtual bool beforeAddLTOPasses(Script &pScript,
                                  llvm::PassManager &pPM,
                                  const char *mTriple)
  { return true; }
#else
  virtual bool beforeAddLTOPasses(Script &pScript, llvm::PassManager &pPM)
  { return true; }
#endif

  // Called after adding last pass to code-generation passes.
  virtual bool afterAddLTOPasses(Script &pScript, llvm::PassManager &pPM)
  { return true; }

  // Called before executing code-generation passes.
#ifdef PVR_RSC
  virtual bool beforeExecuteLTOPasses(Script &pScript,
                                          llvm::PassManager &pPM,
                                          const char *mTriple)
  { return true; }
#else
  virtual bool beforeExecuteLTOPasses(Script &pScript,
                                          llvm::PassManager &pPM)
  { return true; }
#endif

  // Called after executing code-generation passes.
  virtual bool afterExecuteLTOPasses(Script &pScript)
  { return true; }

  // Called before adding first pass to code-generation passes.
  virtual bool beforeAddCodeGenPasses(Script &pScript, llvm::PassManager &pPM)
  { return true; }

  // Called after adding last pass to code-generation passes.
  virtual bool afterAddCodeGenPasses(Script &pScript, llvm::PassManager &pPM)
  { return true; }

  // Called before executing code-generation passes.
  virtual bool beforeExecuteCodeGenPasses(Script &pScript,
                                          llvm::PassManager &pPM)
  { return true; }

  // Called after executing code-generation passes.
  virtual bool afterExecuteCodeGenPasses(Script &pScript)
  { return true; }
};

} // end namespace bcc

#endif // BCC_COMPILER_H
