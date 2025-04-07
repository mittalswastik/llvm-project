//===-------- interface.cpp - Target independent OpenMP target RTL --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implementation of the interface to be used by Clang during the codegen of a
// target region.
//
//===----------------------------------------------------------------------===//

#include "OpenMP/OMPT/Interface.h"
#include "OffloadPolicy.h"
#include "OpenMP/OMPT/Callback.h"
#include "OpenMP/omp.h"
#include "PluginManager.h"
#include "omptarget.h"
#include "private.h"
#include "quantum_circuit_wrapper.h"

#include "Shared/EnvironmentVar.h"
#include "Shared/Profile.h"

#include "Utils/ExponentialBackoff.h"

#include "llvm/Frontend/OpenMP/OMPConstants.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <sstream>

// #include <pybind11/pybind11.h>
// #include <pybind11/embed.h>
// #include <pybind11/stl.h>

//namespace py = pybind11;

#ifdef OMPT_SUPPORT
using namespace llvm::omp::target::ompt;
#endif

// If offload is enabled, ensure that device DeviceID has been initialized.
//
// The return bool indicates if the offload is to the host device
// There are three possible results:
// - Return false if the taregt device is ready for offload
// - Return true without reporting a runtime error if offload is
//   disabled, perhaps because the initial device was specified.
// - Report a runtime error and return true.
//
// If DeviceID == OFFLOAD_DEVICE_DEFAULT, set DeviceID to the default device.
// This step might be skipped if offload is disabled.
bool checkDevice(int64_t &DeviceID, ident_t *Loc) {
  if (OffloadPolicy::get(*PM).Kind == OffloadPolicy::DISABLED) {
    DP("Offload is disabled\n");
    return true;
  }

  if (DeviceID == OFFLOAD_DEVICE_DEFAULT) {
    DeviceID = omp_get_default_device();
    DP("Use default device id %" PRId64 "\n", DeviceID);
  }

  // Proposed behavior for OpenMP 5.2 in OpenMP spec github issue 2669.
  if (omp_get_num_devices() == 0) {
    DP("omp_get_num_devices() == 0 but offload is manadatory\n");
    handleTargetOutcome(false, Loc);
    return true;
  }

  if (DeviceID == omp_get_initial_device()) {
    DP("Device is host (%" PRId64 "), returning as if offload is disabled\n",
       DeviceID);
    return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// adds requires flags
EXTERN void __tgt_register_requires(int64_t Flags) {
  MESSAGE("The %s function has been removed. Old OpenMP requirements will not "
          "be handled",
          __PRETTY_FUNCTION__);
}

EXTERN void __tgt_rtl_init() { initRuntime(); }
EXTERN void __tgt_rtl_deinit() { deinitRuntime(); }

////////////////////////////////////////////////////////////////////////////////
/// adds a target shared library to the target execution image
EXTERN void __tgt_register_lib(__tgt_bin_desc *Desc) {
  initRuntime();
  if (PM->delayRegisterLib(Desc))
    return;

  PM->registerLib(Desc);
}

////////////////////////////////////////////////////////////////////////////////
/// Initialize all available devices without registering any image
EXTERN void __tgt_init_all_rtls() {
  assert(PM && "Runtime not initialized");
  PM->initializeAllDevices();
}

////////////////////////////////////////////////////////////////////////////////
/// unloads a target shared library
EXTERN void __tgt_unregister_lib(__tgt_bin_desc *Desc) {
  PM->unregisterLib(Desc);

  deinitRuntime();
}

template <typename TargetAsyncInfoTy>
static inline void
targetData(ident_t *Loc, int64_t DeviceId, int32_t ArgNum, void **ArgsBase,
           void **Args, int64_t *ArgSizes, int64_t *ArgTypes,
           map_var_info_t *ArgNames, void **ArgMappers,
           TargetDataFuncPtrTy TargetDataFunction, const char *RegionTypeMsg,
           const char *RegionName) {
  assert(PM && "Runtime not initialized");
  static_assert(std::is_convertible_v<TargetAsyncInfoTy, AsyncInfoTy>,
                "TargetAsyncInfoTy must be convertible to AsyncInfoTy.");

  TIMESCOPE_WITH_DETAILS_AND_IDENT("Runtime: Data Copy",
                                   "NumArgs=" + std::to_string(ArgNum), Loc);

  DP("Entering data %s region for device %" PRId64 " with %d mappings\n",
     RegionName, DeviceId, ArgNum);

  if (checkDevice(DeviceId, Loc)) {
    DP("Not offloading to device %" PRId64 "\n", DeviceId);
    return;
  }

  if (getInfoLevel() & OMP_INFOTYPE_KERNEL_ARGS)
    printKernelArguments(Loc, DeviceId, ArgNum, ArgSizes, ArgTypes, ArgNames,
                         RegionTypeMsg);
#ifdef OMPTARGET_DEBUG
  for (int I = 0; I < ArgNum; ++I) {
    DP("Entry %2d: Base=" DPxMOD ", Begin=" DPxMOD ", Size=%" PRId64
       ", Type=0x%" PRIx64 ", Name=%s\n",
       I, DPxPTR(ArgsBase[I]), DPxPTR(Args[I]), ArgSizes[I], ArgTypes[I],
       (ArgNames) ? getNameFromMapping(ArgNames[I]).c_str() : "unknown");
  }
#endif

  auto DeviceOrErr = PM->getDevice(DeviceId);
  if (!DeviceOrErr)
    FATAL_MESSAGE(DeviceId, "%s", toString(DeviceOrErr.takeError()).c_str());

  TargetAsyncInfoTy TargetAsyncInfo(*DeviceOrErr);
  AsyncInfoTy &AsyncInfo = TargetAsyncInfo;

  /// RAII to establish tool anchors before and after data begin / end / update
  OMPT_IF_BUILT(assert((TargetDataFunction == targetDataBegin ||
                        TargetDataFunction == targetDataEnd ||
                        TargetDataFunction == targetDataUpdate) &&
                       "Encountered unexpected TargetDataFunction during "
                       "execution of targetData");
                auto CallbackFunctions =
                    (TargetDataFunction == targetDataBegin)
                        ? RegionInterface.getCallbacks<ompt_target_enter_data>()
                    : (TargetDataFunction == targetDataEnd)
                        ? RegionInterface.getCallbacks<ompt_target_exit_data>()
                        : RegionInterface.getCallbacks<ompt_target_update>();
                InterfaceRAII TargetDataRAII(CallbackFunctions, DeviceId,
                                             OMPT_GET_RETURN_ADDRESS);)

  int Rc = OFFLOAD_SUCCESS;
  Rc = TargetDataFunction(Loc, *DeviceOrErr, ArgNum, ArgsBase, Args, ArgSizes,
                          ArgTypes, ArgNames, ArgMappers, AsyncInfo,
                          false /*FromMapper=*/);

  if (Rc == OFFLOAD_SUCCESS)
    Rc = AsyncInfo.synchronize();

  handleTargetOutcome(Rc == OFFLOAD_SUCCESS, Loc);
}

/// creates host-to-target data mapping, stores it in the
/// libomptarget.so internal structure (an entry in a stack of data maps)
/// and passes the data to the device.
EXTERN void __tgt_target_data_begin_mapper(ident_t *Loc, int64_t DeviceId,
                                           int32_t ArgNum, void **ArgsBase,
                                           void **Args, int64_t *ArgSizes,
                                           int64_t *ArgTypes,
                                           map_var_info_t *ArgNames,
                                           void **ArgMappers) {
  OMPT_IF_BUILT(ReturnAddressSetterRAII RA(__builtin_return_address(0)));
  targetData<AsyncInfoTy>(Loc, DeviceId, ArgNum, ArgsBase, Args, ArgSizes,
                          ArgTypes, ArgNames, ArgMappers, targetDataBegin,
                          "Entering OpenMP data region with being_mapper",
                          "begin");
}

EXTERN void __tgt_target_data_begin_nowait_mapper(
    ident_t *Loc, int64_t DeviceId, int32_t ArgNum, void **ArgsBase,
    void **Args, int64_t *ArgSizes, int64_t *ArgTypes, map_var_info_t *ArgNames,
    void **ArgMappers, int32_t DepNum, void *DepList, int32_t NoAliasDepNum,
    void *NoAliasDepList) {
  OMPT_IF_BUILT(ReturnAddressSetterRAII RA(__builtin_return_address(0)));
  targetData<TaskAsyncInfoWrapperTy>(
      Loc, DeviceId, ArgNum, ArgsBase, Args, ArgSizes, ArgTypes, ArgNames,
      ArgMappers, targetDataBegin,
      "Entering OpenMP data region with being_nowait_mapper", "begin");
}

/// passes data from the target, releases target memory and destroys
/// the host-target mapping (top entry from the stack of data maps)
/// created by the last __tgt_target_data_begin.
EXTERN void __tgt_target_data_end_mapper(ident_t *Loc, int64_t DeviceId,
                                         int32_t ArgNum, void **ArgsBase,
                                         void **Args, int64_t *ArgSizes,
                                         int64_t *ArgTypes,
                                         map_var_info_t *ArgNames,
                                         void **ArgMappers) {
  OMPT_IF_BUILT(ReturnAddressSetterRAII RA(__builtin_return_address(0)));
  targetData<AsyncInfoTy>(Loc, DeviceId, ArgNum, ArgsBase, Args, ArgSizes,
                          ArgTypes, ArgNames, ArgMappers, targetDataEnd,
                          "Exiting OpenMP data region with end_mapper", "end");
}

EXTERN void __tgt_target_data_end_nowait_mapper(
    ident_t *Loc, int64_t DeviceId, int32_t ArgNum, void **ArgsBase,
    void **Args, int64_t *ArgSizes, int64_t *ArgTypes, map_var_info_t *ArgNames,
    void **ArgMappers, int32_t DepNum, void *DepList, int32_t NoAliasDepNum,
    void *NoAliasDepList) {
  OMPT_IF_BUILT(ReturnAddressSetterRAII RA(__builtin_return_address(0)));
  targetData<TaskAsyncInfoWrapperTy>(
      Loc, DeviceId, ArgNum, ArgsBase, Args, ArgSizes, ArgTypes, ArgNames,
      ArgMappers, targetDataEnd,
      "Exiting OpenMP data region with end_nowait_mapper", "end");
}

EXTERN void __tgt_target_data_update_mapper(ident_t *Loc, int64_t DeviceId,
                                            int32_t ArgNum, void **ArgsBase,
                                            void **Args, int64_t *ArgSizes,
                                            int64_t *ArgTypes,
                                            map_var_info_t *ArgNames,
                                            void **ArgMappers) {
  OMPT_IF_BUILT(ReturnAddressSetterRAII RA(__builtin_return_address(0)));
  targetData<AsyncInfoTy>(
      Loc, DeviceId, ArgNum, ArgsBase, Args, ArgSizes, ArgTypes, ArgNames,
      ArgMappers, targetDataUpdate,
      "Updating data within the OpenMP data region with update_mapper",
      "update");
}

EXTERN void __tgt_target_data_update_nowait_mapper(
    ident_t *Loc, int64_t DeviceId, int32_t ArgNum, void **ArgsBase,
    void **Args, int64_t *ArgSizes, int64_t *ArgTypes, map_var_info_t *ArgNames,
    void **ArgMappers, int32_t DepNum, void *DepList, int32_t NoAliasDepNum,
    void *NoAliasDepList) {
  OMPT_IF_BUILT(ReturnAddressSetterRAII RA(__builtin_return_address(0)));
  targetData<TaskAsyncInfoWrapperTy>(
      Loc, DeviceId, ArgNum, ArgsBase, Args, ArgSizes, ArgTypes, ArgNames,
      ArgMappers, targetDataUpdate,
      "Updating data within the OpenMP data region with update_nowait_mapper",
      "update");
}

static KernelArgsTy *upgradeKernelArgs(KernelArgsTy *KernelArgs,
                                       KernelArgsTy &LocalKernelArgs,
                                       int32_t NumTeams, int32_t ThreadLimit) {
  if (KernelArgs->Version > OMP_KERNEL_ARG_VERSION)
    DP("Unexpected ABI version: %u\n", KernelArgs->Version);

  uint32_t UpgradedVersion = KernelArgs->Version;
  if (KernelArgs->Version < OMP_KERNEL_ARG_VERSION) {
    // The upgraded version will be based on the kernel launch environment.
    if (KernelArgs->Version < OMP_KERNEL_ARG_MIN_VERSION_WITH_DYN_PTR)
      UpgradedVersion = OMP_KERNEL_ARG_MIN_VERSION_WITH_DYN_PTR - 1;
    else
      UpgradedVersion = OMP_KERNEL_ARG_VERSION;
  }
  if (UpgradedVersion != KernelArgs->Version) {
    LocalKernelArgs.Version = UpgradedVersion;
    LocalKernelArgs.NumArgs = KernelArgs->NumArgs;
    LocalKernelArgs.ArgBasePtrs = KernelArgs->ArgBasePtrs;
    LocalKernelArgs.ArgPtrs = KernelArgs->ArgPtrs;
    LocalKernelArgs.ArgSizes = KernelArgs->ArgSizes;
    LocalKernelArgs.ArgTypes = KernelArgs->ArgTypes;
    LocalKernelArgs.ArgNames = KernelArgs->ArgNames;
    LocalKernelArgs.ArgMappers = KernelArgs->ArgMappers;
    LocalKernelArgs.Tripcount = KernelArgs->Tripcount;
    LocalKernelArgs.Flags = KernelArgs->Flags;
    LocalKernelArgs.DynCGroupMem = 0;
    LocalKernelArgs.NumTeams[0] = NumTeams;
    LocalKernelArgs.NumTeams[1] = 0;
    LocalKernelArgs.NumTeams[2] = 0;
    LocalKernelArgs.ThreadLimit[0] = ThreadLimit;
    LocalKernelArgs.ThreadLimit[1] = 0;
    LocalKernelArgs.ThreadLimit[2] = 0;
    return &LocalKernelArgs;
  }

  return KernelArgs;
}

//execute python quantum circuit
std::string exec(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        // throw std::runtime_error("popen() failed!");
        std::cout<<"pipe connections failed"<<std::endl;
        return NULL;
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

std::string execPythonScript(std::vector< std::vector<int32_t> > data){
  std::string json_data = "[";
  for (int32_t i = 0; i < data.size(); ++i) {
    json_data += "[";
    for(int32_t j = 0; j < data[i].size(); ++j){
      json_data += std::to_string(data[i][j]);
      if (j < data[i].size() - 1) {
          json_data += ",";
      }
    }

    json_data += "]";
  }

  json_data += "]";

  // Prepare command
  std::string command = "bash /home/swastik/dev/openmp_offload/run_python.sh '" + json_data + "'";

  // Execute the command and get the result
  std::string result = exec(command);

  // Print the result
  std::cout << " received result: " << result << std::endl;
  return result;
}

//convert void* to a vector
// void parseToVector(void *ptr, size_t size, std::vector<int> &vec) {
//     std::cout<<" size of the the vector is: "<<size<<std::endl;
//     int32_t *intPtr = static_cast<int32_t*> (ptr); // Cast void* to int*
//     size = size/sizeof(int32_t);
//     vec.assign(intPtr, intPtr + size);   // Populate vector using a range
// }

template <typename TargetAsyncInfoTy>
static inline int targetKernel(ident_t *Loc, int64_t DeviceId, int32_t NumTeams,
                               int32_t ThreadLimit, void *HostPtr,
                               KernelArgsTy *KernelArgs) {
  assert(PM && "Runtime not initialized");
  static_assert(std::is_convertible_v<TargetAsyncInfoTy, AsyncInfoTy>,
                "Target AsyncInfoTy must be convertible to AsyncInfoTy.");
  DP("Entering target region for device %" PRId64 " with entry point " DPxMOD
     "\n",
     DeviceId, DPxPTR(HostPtr));
  
  int64_t temp_device_id = 0;

  //swastik
  // if(DeviceId == 100){

  //   // QuantumCircuitWrapper* qc = new QuantumCircuitWrapper(2);
  //   // qc->apply_hadamard(2);
  //   // qc->run();
  //   std::vector<int32_t> input_arg;
  //   std::vector<int32_t> output_arg;
  //   std::cout<<"offload to quantum to circuit"<<std::endl;
  //   for (int32_t I = 0; I < KernelArgs->NumArgs; ++I){
  //     if(KernelArgs->ArgTypes[I] & OMP_TGT_MAPTYPE_TO){
  //       input_arg.push_back(I);
  //     }

  //     if((KernelArgs->ArgTypes[I] & OMP_TGT_MAPTYPE_TO) && (KernelArgs->ArgTypes[I] & OMP_TGT_MAPTYPE_FROM)){
  //       std::cout<<"toFrom is of both types"<<std::endl;
  //     }

  //     if(KernelArgs->ArgTypes[I] & OMP_TGT_MAPTYPE_FROM){
  //       output_arg.push_back(I);
  //     }
  //   }

  //   std::vector< std::vector<int32_t> > vec(input_arg.size());
  //   for (int32_t I = 0; I < input_arg.size(); ++I){
  //     // store these values as arrays to pass to python script
  //     parseToVector(KernelArgs->ArgPtrs[input_arg[I]], KernelArgs->ArgSizes[input_arg[I]], vec[I]);
  //   }

  //   //DP("vecotr values are\n");
  //   for(int32_t i = 0 ; i < vec.size() ; i++){
  //     for(int32_t j = 0 ; j < vec[i].size() ; j++){
  //       //DP("%" PRId32 " ", vec[i][j]);
  //       std::cout << vec[i][j] <<" ";
  //     }
  //     std::cout<<std::endl;
  //     //DP("\n");
  //   }

  //   std::string result = execPythonScript(vec);
  //   if (!result.empty() && result.front() == '[') {
  //       result.erase(0, 1); // Remove the first character
  //   }
  //   if (!result.empty() && result.back() == ']') {
  //       result.pop_back(); // Remove the last character
  //   }
  //   std::string item;
  //   std::stringstream ss(result);
  //   std::vector<int32_t> ret_result;
  //   while (std::getline(ss, item, ',')) {
  //       // Convert each substring to an integer and add to the vector
  //       ret_result.push_back(std::stoi(item));
  //   }

  //   // std::cout<<"pr8ingting the result"<<std::endl;

  //   // for(int32_t i = 0 ; i < ret_result.size() ; i++){
  //   //   std::cout<<ret_result[i]<<std::endl;
  //   // }

  //   // std::cout<<std::endl;

  //   int32_t ctr = 0;

  //   auto DeviceOrErr = PM->getDevice(DeviceId);
  //   if (!DeviceOrErr)
  //     FATAL_MESSAGE(DeviceId, "%s", toString(DeviceOrErr.takeError()).c_str());
  //   TargetAsyncInfoTy TargetAsyncInfo(*DeviceOrErr);
  //   AsyncInfoTy &AsyncInfo = TargetAsyncInfo;

  //   while(ctr < output_arg.size()){
  //     std::vector<int32_t> temp(ret_result.begin() + ctr, ret_result.begin() + (KernelArgs->ArgSizes[output_arg[ctr]]/sizeof(int32_t)));
  //     // std::cout<<"temp value is"<<std::endl;
  //     // for(int32_t i = 0 ; i < temp.size() ; i++){
  //     //   std::cout<<temp[i]<<std::endl;
  //     // }
  //     KernelArgs->ArgPtrs[output_arg[ctr]] = static_cast<void*>(temp.data());
  //     ctr++;

  //     /**storing it at the host memory of the destination*/

  //     void *HstPtrBegin = KernelArgs->ArgPtrs[output_arg[ctr]];
  //     int64_t DataSize = KernelArgs->ArgSizes[output_arg[ctr]];
  //     bool IsImplicit = KernelArgs->ArgTypes[output_arg[ctr]] & OMP_TGT_MAPTYPE_IMPLICIT;
  //     bool UpdateRef = (!(KernelArgs->ArgTypes[output_arg[ctr]] & OMP_TGT_MAPTYPE_MEMBER_OF) ||
  //                       (KernelArgs->ArgTypes[output_arg[ctr]] & OMP_TGT_MAPTYPE_PTR_AND_OBJ)) &&
  //                     !(true && output_arg[ctr] == 0);
  //     bool ForceDelete = KernelArgs->ArgTypes[output_arg[ctr]] & OMP_TGT_MAPTYPE_DELETE;
  //     bool HasPresentModifier = KernelArgs->ArgTypes[output_arg[ctr]] & OMP_TGT_MAPTYPE_PRESENT;
  //     bool HasHoldModifier = KernelArgs->ArgTypes[output_arg[ctr]] & OMP_TGT_MAPTYPE_OMPX_HOLD;

  //     // If PTR_AND_OBJ, HstPtrBegin is address of pointee
  //     TargetPointerResultTy TPR = DeviceOrErr->getMappingInfo().getTgtPtrBegin(
  //         HstPtrBegin, DataSize, UpdateRef, HasHoldModifier, !IsImplicit,
  //         ForceDelete, /*FromDataEnd=*/true);
  //     void *TgtPtrBegin = TPR.TargetPointer;

  //     int Ret = DeviceOrErr->retrieveData(HstPtrBegin, TgtPtrBegin, DataSize, AsyncInfo,
  //                               TPR.getEntry());
  //     if (Ret != OFFLOAD_SUCCESS) {
  //       REPORT("Copying data from device failed.\n");
  //       return OFFLOAD_FAIL;
  //     }
  //   }

  //   std::vector< std::vector<int32_t> > vec_temp(output_arg.size());
  //   for (int32_t I = 0; I < output_arg.size(); ++I){
  //     // store these values as arrays to pass to python script
  //     parseToVector(KernelArgs->ArgPtrs[output_arg[I]], KernelArgs->ArgSizes[output_arg[I]], vec_temp[I]);
  //   }

  //   std::cout<<"Printing output kernel arg"<<std::endl;

  //   //DP("vecotr values are\n");
  //   for(int32_t i = 0 ; i < vec_temp.size() ; i++){
  //     for(int32_t j = 0 ; j < vec_temp[i].size() ; j++){
  //       //DP("%" PRId32 " ", vec[i][j]);
  //       std::cout << vec_temp[i][j] <<" ";
  //     }
  //     std::cout<<std::endl;
  //     //DP("\n");
  //   }

  //   // call the python script and store the results back in argtype to
  //   //return OMP_TGT_SUCCESS;
  // }
  std::vector<int32_t> input_arg;
  std::vector<int32_t> output_arg;
  std::vector<size_t> output_sizes;
  QuantumCircuitWrapper *c;
  if(DeviceId == 100){
    std::cout<<"args base pts"<<std::endl;
    c = (QuantumCircuitWrapper*) KernelArgs->ArgBasePtrs[1]; // forced fix - find a better way
    //std::cout<<"circuit test value: "<<(c)->test<<std::endl;
    DeviceId = 0; //default to cpu id -- modifying interface to handle quantum offloading later
    temp_device_id = 100;

    std::cout<<"offload to quantum to circuit"<<std::endl;
    for (int32_t I = 0; I < KernelArgs->NumArgs; ++I){
      if(KernelArgs->ArgTypes[I] & OMP_TGT_MAPTYPE_TO){
        input_arg.push_back(I);
      }

      // if((KernelArgs->ArgTypes[I] & OMP_TGT_MAPTYPE_TO) && (KernelArgs->ArgTypes[I] & OMP_TGT_MAPTYPE_FROM)){
      //   std::cout<<"toFrom is of both types"<<std::endl;
      // }

      if(KernelArgs->ArgTypes[I] & OMP_TGT_MAPTYPE_FROM){
        c->vec_out_data.push_back(KernelArgs->ArgBasePtrs[I]);
        output_sizes.push_back(KernelArgs->ArgSizes[I]);
        //output_arg.push_back(I);
      }
    }

    std::vector< std::vector<int32_t> > vec(input_arg.size());
    for (int32_t I = 0; I < input_arg.size(); ++I){
      // store these values as arrays to pass to python script
      c->vec_data.push_back(c->parseToVector(KernelArgs->ArgPtrs[input_arg[I]], KernelArgs->ArgSizes[input_arg[I]], vec[I]));
      //c->vec_output_data.push_back(c->parseToVector(KernelArgs->ArgPtrs[output_arg[I]], KernelArgs->ArgSizes[output_arg[I]], vec[I]));
    }
  }

  std::cout<<"Target Offload Policy Value Is: "<<__kmpc_get_target_offload()<<std::endl;
  
  if (checkDevice(DeviceId, Loc)) {
    //swastik
    DP("Not offloading to device %" PRId64 "\n", DeviceId);
    std::cout<<"failed"<<std::endl;
    return OMP_TGT_FAIL;
  }

  bool IsTeams = NumTeams != -1;
  if (!IsTeams)
    KernelArgs->NumTeams[0] = NumTeams = 1;

  // Auto-upgrade kernel args version 1 to 2.
  KernelArgsTy LocalKernelArgs;
  KernelArgs =
      upgradeKernelArgs(KernelArgs, LocalKernelArgs, NumTeams, ThreadLimit);

  assert(KernelArgs->NumTeams[0] == static_cast<uint32_t>(NumTeams) &&
         !KernelArgs->NumTeams[1] && !KernelArgs->NumTeams[2] &&
         "OpenMP interface should not use multiple dimensions");
  assert(KernelArgs->ThreadLimit[0] == static_cast<uint32_t>(ThreadLimit) &&
         !KernelArgs->ThreadLimit[1] && !KernelArgs->ThreadLimit[2] &&
         "OpenMP interface should not use multiple dimensions");
  TIMESCOPE_WITH_DETAILS_AND_IDENT(
      "Runtime: target exe",
      "NumTeams=" + std::to_string(NumTeams) +
          ";NumArgs=" + std::to_string(KernelArgs->NumArgs),
      Loc);

  if (getInfoLevel() & OMP_INFOTYPE_KERNEL_ARGS)
    printKernelArguments(Loc, DeviceId, KernelArgs->NumArgs,
                         KernelArgs->ArgSizes, KernelArgs->ArgTypes,
                         KernelArgs->ArgNames, "Entering OpenMP kernel");
#ifdef OMPTARGET_DEBUG
  for (uint32_t I = 0; I < KernelArgs->NumArgs; ++I) {
    DP("Entry %2d: Base=" DPxMOD ", Begin=" DPxMOD ", Size=%" PRId64
       ", Type=0x%" PRIx64 ", Name=%s\n",
       I, DPxPTR(KernelArgs->ArgBasePtrs[I]), DPxPTR(KernelArgs->ArgPtrs[I]),
       KernelArgs->ArgSizes[I], KernelArgs->ArgTypes[I],
       (KernelArgs->ArgNames)
           ? getNameFromMapping(KernelArgs->ArgNames[I]).c_str()
           : "unknown");
  }
#endif

  auto DeviceOrErr = PM->getDevice(DeviceId);
  if (!DeviceOrErr)
    FATAL_MESSAGE(DeviceId, "%s", toString(DeviceOrErr.takeError()).c_str());

  TargetAsyncInfoTy TargetAsyncInfo(*DeviceOrErr);
  AsyncInfoTy &AsyncInfo = TargetAsyncInfo;
  /// RAII to establish tool anchors before and after target region
  OMPT_IF_BUILT(InterfaceRAII TargetRAII(
                    RegionInterface.getCallbacks<ompt_target>(), DeviceId,
                    /*CodePtr=*/OMPT_GET_RETURN_ADDRESS);)

  int Rc = OFFLOAD_SUCCESS;
  Rc = target(Loc, *DeviceOrErr, HostPtr, *KernelArgs, AsyncInfo);
  { // required to show syncronization
    TIMESCOPE_WITH_DETAILS_AND_IDENT("Runtime: syncronize", "", Loc);
    if (Rc == OFFLOAD_SUCCESS)
      Rc = AsyncInfo.synchronize();

    handleTargetOutcome(Rc == OFFLOAD_SUCCESS, Loc);
    assert(Rc == OFFLOAD_SUCCESS && "__tgt_target_kernel unexpected failure!");
  }

  // if(temp_device_id == 100){
  //   // for (int32_t I = 0; I < KernelArgs->NumArgs; ++I){
  //   //   std::cout<<"Kernel arg types left are: "<<KernelArgs->ArgTypes[I]<<std::endl;
  //   //   if(KernelArgs->ArgTypes[I] & OMP_TGT_MAPTYPE_FROM){
  //   //     output_arg.push_back(I);
  //   //   }
  //   // }


  if(temp_device_id == 100){
    std::cout<<"circuit test value changed to: "<<c->test<<std::endl;
    c->run();
    // }

    for (int32_t i = 0; i < c->vec_out_data.size(); ++i){
        // size_t size = KernelArgs->ArgSizes[output_arg[i]]/sizeof(int32_t);
        // std::vector<int32_t> output_vec;
        // for(int32_t j = 0 ; j < size ; ++j){
        //   output_vec.push_back(j);
        // }
        std::vector<int32_t> output_vec_test;
        //output_vec_test = c->parseToVector(*(c->vec_out_data[i]), output_sizes[i], output_vec_test);
        intptr_t intPtr = reinterpret_cast<intptr_t> (c->vec_out_data[i]); // Cast void* to int*
        int32_t *intVal = reinterpret_cast<int32_t*>(intPtr);
        size_t tsize = output_sizes[i]/sizeof(int32_t);
        output_vec_test.assign(intVal, intVal+tsize);
        std::cout<<"output vec val is: "<<std::endl;
        for(int j = 0 ; j < output_vec_test.size(); j++){
          intVal[j] = 3;
          std::cout<<output_vec_test[j]<<" "<<std::endl;
        }

        std::cout<<std::endl;
        //KernelArgs->ArgPtrs[output_arg[i]] = &output_vec;
    }
  }

  return OMP_TGT_SUCCESS;
}

/// Implements a kernel entry that executes the target region on the specified
/// device.
///
/// \param Loc Source location associated with this target region.
/// \param DeviceId The device to execute this region, -1 indicated the default.
/// \param NumTeams Number of teams to launch the region with, -1 indicates a
///                 non-teams region and 0 indicates it was unspecified.
/// \param ThreadLimit Limit to the number of threads to use in the kernel
///                    launch, 0 indicates it was unspecified.
/// \param HostPtr  The pointer to the host function registered with the kernel.
/// \param Args     All arguments to this kernel launch (see struct definition).
EXTERN int __tgt_target_kernel(ident_t *Loc, int64_t DeviceId, int32_t NumTeams,
                               int32_t ThreadLimit, void *HostPtr,
                               KernelArgsTy *KernelArgs) {
  OMPT_IF_BUILT(ReturnAddressSetterRAII RA(__builtin_return_address(0)));
  if (KernelArgs->Flags.NoWait)
    return targetKernel<TaskAsyncInfoWrapperTy>(
        Loc, DeviceId, NumTeams, ThreadLimit, HostPtr, KernelArgs);
  return targetKernel<AsyncInfoTy>(Loc, DeviceId, NumTeams, ThreadLimit,
                                   HostPtr, KernelArgs);
}

/// Activates the record replay mechanism.
/// \param DeviceId The device identifier to execute the target region.
/// \param MemorySize The number of bytes to be (pre-)allocated
///                   by the bump allocator
/// /param IsRecord Activates the record replay mechanism in
///                 'record' mode or 'replay' mode.
/// /param SaveOutput Store the device memory after kernel
///                   execution on persistent storage
EXTERN int __tgt_activate_record_replay(int64_t DeviceId, uint64_t MemorySize,
                                        void *VAddr, bool IsRecord,
                                        bool SaveOutput,
                                        uint64_t &ReqPtrArgOffset) {
  assert(PM && "Runtime not initialized");
  OMPT_IF_BUILT(ReturnAddressSetterRAII RA(__builtin_return_address(0)));
  auto DeviceOrErr = PM->getDevice(DeviceId);
  if (!DeviceOrErr)
    FATAL_MESSAGE(DeviceId, "%s", toString(DeviceOrErr.takeError()).c_str());

  [[maybe_unused]] int Rc = target_activate_rr(
      *DeviceOrErr, MemorySize, VAddr, IsRecord, SaveOutput, ReqPtrArgOffset);
  assert(Rc == OFFLOAD_SUCCESS &&
         "__tgt_activate_record_replay unexpected failure!");
  return OMP_TGT_SUCCESS;
}

/// Implements a target kernel entry that replays a pre-recorded kernel.
/// \param Loc Source location associated with this target region (unused).
/// \param DeviceId The device identifier to execute the target region.
/// \param HostPtr A pointer to an address that uniquely identifies the kernel.
/// \param DeviceMemory A pointer to an array storing device memory data to move
///                     prior to kernel execution.
/// \param DeviceMemorySize The size of the above device memory data in bytes.
/// \param TgtArgs An array of pointers of the pre-recorded target kernel
///                arguments.
/// \param TgtOffsets An array of pointers of the pre-recorded target kernel
///                   argument offsets.
/// \param NumArgs The number of kernel arguments.
/// \param NumTeams Number of teams to launch the target region with.
/// \param ThreadLimit Limit to the number of threads to use in kernel
///                    execution.
/// \param LoopTripCount The pre-recorded value of the loop tripcount, if any.
/// \return OMP_TGT_SUCCESS on success, OMP_TGT_FAIL on failure.
EXTERN int __tgt_target_kernel_replay(ident_t *Loc, int64_t DeviceId,
                                      void *HostPtr, void *DeviceMemory,
                                      int64_t DeviceMemorySize, void **TgtArgs,
                                      ptrdiff_t *TgtOffsets, int32_t NumArgs,
                                      int32_t NumTeams, int32_t ThreadLimit,
                                      uint64_t LoopTripCount) {
  assert(PM && "Runtime not initialized");
  OMPT_IF_BUILT(ReturnAddressSetterRAII RA(__builtin_return_address(0)));
  if (checkDevice(DeviceId, Loc)) {
    DP("Not offloading to device %" PRId64 "\n", DeviceId);
    return OMP_TGT_FAIL;
  }
  auto DeviceOrErr = PM->getDevice(DeviceId);
  if (!DeviceOrErr)
    FATAL_MESSAGE(DeviceId, "%s", toString(DeviceOrErr.takeError()).c_str());

  /// RAII to establish tool anchors before and after target region
  OMPT_IF_BUILT(InterfaceRAII TargetRAII(
                    RegionInterface.getCallbacks<ompt_target>(), DeviceId,
                    /*CodePtr=*/OMPT_GET_RETURN_ADDRESS);)

  AsyncInfoTy AsyncInfo(*DeviceOrErr);
  int Rc = target_replay(Loc, *DeviceOrErr, HostPtr, DeviceMemory,
                         DeviceMemorySize, TgtArgs, TgtOffsets, NumArgs,
                         NumTeams, ThreadLimit, LoopTripCount, AsyncInfo);
  if (Rc == OFFLOAD_SUCCESS)
    Rc = AsyncInfo.synchronize();
  handleTargetOutcome(Rc == OFFLOAD_SUCCESS, Loc);
  assert(Rc == OFFLOAD_SUCCESS &&
         "__tgt_target_kernel_replay unexpected failure!");
  return OMP_TGT_SUCCESS;
}

// Get the current number of components for a user-defined mapper.
EXTERN int64_t __tgt_mapper_num_components(void *RtMapperHandle) {
  auto *MapperComponentsPtr = (struct MapperComponentsTy *)RtMapperHandle;
  int64_t Size = MapperComponentsPtr->Components.size();
  DP("__tgt_mapper_num_components(Handle=" DPxMOD ") returns %" PRId64 "\n",
     DPxPTR(RtMapperHandle), Size);
  return Size;
}

// Push back one component for a user-defined mapper.
EXTERN void __tgt_push_mapper_component(void *RtMapperHandle, void *Base,
                                        void *Begin, int64_t Size, int64_t Type,
                                        void *Name) {
  DP("__tgt_push_mapper_component(Handle=" DPxMOD
     ") adds an entry (Base=" DPxMOD ", Begin=" DPxMOD ", Size=%" PRId64
     ", Type=0x%" PRIx64 ", Name=%s).\n",
     DPxPTR(RtMapperHandle), DPxPTR(Base), DPxPTR(Begin), Size, Type,
     (Name) ? getNameFromMapping(Name).c_str() : "unknown");
  auto *MapperComponentsPtr = (struct MapperComponentsTy *)RtMapperHandle;
  MapperComponentsPtr->Components.push_back(
      MapComponentInfoTy(Base, Begin, Size, Type, Name));
}

EXTERN void __tgt_set_info_flag(uint32_t NewInfoLevel) {
  assert(PM && "Runtime not initialized");
  std::atomic<uint32_t> &InfoLevel = getInfoLevelInternal();
  InfoLevel.store(NewInfoLevel);
}

EXTERN int __tgt_print_device_info(int64_t DeviceId) {
  assert(PM && "Runtime not initialized");
  auto DeviceOrErr = PM->getDevice(DeviceId);
  if (!DeviceOrErr)
    FATAL_MESSAGE(DeviceId, "%s", toString(DeviceOrErr.takeError()).c_str());

  return DeviceOrErr->printDeviceInfo();
}

EXTERN void __tgt_target_nowait_query(void **AsyncHandle) {
  assert(PM && "Runtime not initialized");
  OMPT_IF_BUILT(ReturnAddressSetterRAII RA(__builtin_return_address(0)));

  if (!AsyncHandle || !*AsyncHandle) {
    FATAL_MESSAGE0(
        1, "Receive an invalid async handle from the current OpenMP task. Is "
           "this a target nowait region?\n");
  }

  // Exponential backoff tries to optimally decide if a thread should just query
  // for the device operations (work/spin wait on them) or block until they are
  // completed (use device side blocking mechanism). This allows the runtime to
  // adapt itself when there are a lot of long-running target regions in-flight.
  static thread_local utils::ExponentialBackoff QueryCounter(
      Int64Envar("OMPTARGET_QUERY_COUNT_MAX", 10),
      Int64Envar("OMPTARGET_QUERY_COUNT_THRESHOLD", 5),
      Envar<float>("OMPTARGET_QUERY_COUNT_BACKOFF_FACTOR", 0.5f));

  auto *AsyncInfo = (AsyncInfoTy *)*AsyncHandle;

  // If the thread is actively waiting on too many target nowait regions, we
  // should use the blocking sync type.
  if (QueryCounter.isAboveThreshold())
    AsyncInfo->SyncType = AsyncInfoTy::SyncTy::BLOCKING;

  if (AsyncInfo->synchronize())
    FATAL_MESSAGE0(1, "Error while querying the async queue for completion.\n");
  // If there are device operations still pending, return immediately without
  // deallocating the handle and increase the current thread query count.
  if (!AsyncInfo->isDone()) {
    QueryCounter.increment();
    return;
  }

  // When a thread successfully completes a target nowait region, we
  // exponentially backoff its query counter by the query factor.
  QueryCounter.decrement();

  // Delete the handle and unset it from the OpenMP task data.
  delete AsyncInfo;
  *AsyncHandle = nullptr;
}
