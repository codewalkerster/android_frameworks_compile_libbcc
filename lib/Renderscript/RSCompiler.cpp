/*
 * Copyright 2012, The Android Open Source Project
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

#include "bcc/Renderscript/RSCompiler.h"

#if defined(PVR_RSC)
#include <llvm/ADT/Triple.h>
#endif
#include <llvm/Module.h>
#include <llvm/PassManager.h>
#include <llvm/Transforms/IPO.h>

#include "bcc/Renderscript/RSExecutable.h"
#include "bcc/Renderscript/RSInfo.h"
#include "bcc/Renderscript/RSScript.h"
#include "bcc/Renderscript/RSTransforms.h"
#include "bcc/Source.h"
#include "bcc/Support/Log.h"

using namespace bcc;

#if defined(PVR_RSC)
bool RSCompiler::beforeAddLTOPasses(Script &pScript,
                                    llvm::PassManager &pPM,
                                    const char *mTriple) {
#else
bool RSCompiler::beforeAddLTOPasses(Script &pScript, llvm::PassManager &pPM) {
#endif
  // Add a pass to internalize the symbols that don't need to have global
  // visibility.
  RSScript &script = static_cast<RSScript &>(pScript);
  const RSInfo *info = script.getInfo();

  // The vector contains the symbols that should not be internalized.
  std::vector<const char *> export_symbols;

  // Special RS functions should always be global symbols.
  const char **special_functions = RSExecutable::SpecialFunctionNames;
  while (*special_functions != NULL) {
    export_symbols.push_back(*special_functions);
    special_functions++;
  }

  // Visibility of symbols appeared in rs_export_var and rs_export_func should
  // also be preserved.
  const RSInfo::ExportVarNameListTy &export_vars = info->getExportVarNames();
  const RSInfo::ExportFuncNameListTy &export_funcs = info->getExportFuncNames();

  for (RSInfo::ExportVarNameListTy::const_iterator
           export_var_iter = export_vars.begin(),
           export_var_end = export_vars.end();
       export_var_iter != export_var_end; export_var_iter++) {
    export_symbols.push_back(*export_var_iter);
  }

  for (RSInfo::ExportFuncNameListTy::const_iterator
           export_func_iter = export_funcs.begin(),
           export_func_end = export_funcs.end();
       export_func_iter != export_func_end; export_func_iter++) {
    export_symbols.push_back(*export_func_iter);
  }

  // If compiling for CPU, expanded foreach functions should not be
  // internalized, if compiling for PVR, foreach functions should not be
  // internalized instead, and there is no need to expand them.
  const RSInfo::ExportForeachFuncListTy &export_foreach_func =
      info->getExportForeachFuncs();
  std::vector<std::string> expanded_foreach_funcs;
  for (RSInfo::ExportForeachFuncListTy::const_iterator
           foreach_func_iter = export_foreach_func.begin(),
           foreach_func_end = export_foreach_func.end();
       foreach_func_iter != foreach_func_end; foreach_func_iter++) {
    std::string name(foreach_func_iter->first);
#if defined(PVR_RSC)
    if (!strncmp(mTriple, llvm::Triple::getArchTypeName(llvm::Triple::usc), 3))
      expanded_foreach_funcs.push_back(name);
    else
#endif
    expanded_foreach_funcs.push_back(name.append(".expand"));
  }

  // Need to wait until ForEachExpandList is fully populated to fill in
  // exported symbols.
  for (size_t i = 0; i < expanded_foreach_funcs.size(); i++) {
    export_symbols.push_back(expanded_foreach_funcs[i].c_str());
  }

  pPM.add(llvm::createInternalizePass(export_symbols));

  return true;
}

#if defined(PVR_RSC)
bool RSCompiler::beforeExecuteLTOPasses(Script &pScript,
                                        llvm::PassManager &pPM,
                                        const char *mTriple) {
#else
bool RSCompiler::beforeExecuteLTOPasses(Script &pScript,
                                        llvm::PassManager &pPM) {
#endif
  // Execute a pass to expand foreach-able functions
  llvm::PassManager rs_passes;

  // Script passed to RSCompiler must be a RSScript.
  RSScript &script = static_cast<RSScript &>(pScript);
  const RSInfo *info = script.getInfo();
  llvm::Module &module = script.getSource().getModule();

  if (info == NULL) {
    ALOGE("Missing RSInfo in RSScript to run the pass for foreach expansion on "
          "%s!", module.getModuleIdentifier().c_str());
    return false;
  }

#if defined(PVR_RSC)
  // Expand ForEach on CPU path to reduce launch overhead
  // (if not compiling for PVR).
  if (strncmp(mTriple, llvm::Triple::getArchTypeName(llvm::Triple::usc), 3))
    rs_passes.add(createRSForEachExpandPass(info->getExportForeachFuncs(),
                                            /* pEnableStepOpt */ true));
#else
  // Expand ForEach on CPU path to reduce launch overhead.
  rs_passes.add(createRSForEachExpandPass(info->getExportForeachFuncs(),
                                          /* pEnableStepOpt */ true));
#endif

  // Execute the pass.
  rs_passes.run(module);

  return true;
}
