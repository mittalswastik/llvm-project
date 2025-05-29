#ifndef QUANTUM_CIRCUIT_WRAPPER_H
#define QUANTUM_CIRCUIT_WRAPPER_H

#include <string>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <cstdint> 
#include <json/json.h>
#include <unistd.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

class QuantumCircuitWrapper {
public:
        std::string test;
        std::vector<std::vector<int32_t> > vec_data;
        std::vector<void*> vec_out_data;
        std::vector<int> evaluated_qubits;
        int num_iterations;
        pid_t pid;
        pid_t child_pid;
        int toPy[2];
        int fromPy[2];
        QuantumCircuitWrapper(int num_qubits);

        void apply_hadamard(int qubit);
        void debug();
        void apply_cnot(int control, int target);
        void apply_x(int qubit);
        void apply_barrier();
        void apply_hamiltonian_qiskit();
        void apply_ghz_qiskit();
        //void apply_ry(std::vector<double> vec, int val);
        void apply_ry(double angle, int val);
        void execute_basic_quantum();
        void measure();
        void close_pipes();
        void exec_pipes();
        std::vector<int32_t> parseToVector(void* ptr, size_t size, std::vector<int32_t> vec);
        void run();

private:
    int num_qubits;
    std::string gates;
    std::string scr;
    std::string params;
    std::string generate_python_script(const std::string& circuit_name, int num_qubits, const std::string& gates);
    void execute_python_script(const std::string& script);
    std::vector<int> readQubits(const std::string& result, int num_qs);
    std::string returnJsonString();
};

#endif // QUANTUM_CIRCUIT_WRAPPER_H
