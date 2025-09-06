# add_llvm_pass_plugin(LLVMPreload
#   Preload.cpp
add_llvm_component_library(LLVMPreload
  Preload.cpp

  DEPENDS intrinsics_gen

#   ADDITIONAL_HEADER_DIRS
#   ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms
#   ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms/Preload

  LINK_COMPONENTS
  AggressiveInstCombine
  Analysis
  Core
  InstCombine
  Support
  TransformUtils
  )
