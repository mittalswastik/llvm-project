// RUN: %libomptarget-compilexx-nvptx64-nvidia-cuda && %libomptarget-run-fail-nvptx64-nvidia-cuda
<<<<<<< HEAD
// REQUIRES: nvptx64-nvidia-cuda
=======
>>>>>>> 0826268d59c6e1bb3530dffd9dc5f6038774486d

int main(int argc, char *argv[]) {
#pragma omp target
  { __builtin_trap(); }

  return 0;
}
