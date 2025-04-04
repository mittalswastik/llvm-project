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

class QuantumCircuitWrapper {
public:
        std::string test;
        std::vector<std::vector<int32_t> > vec_data;
        std::vector<void*> vec_out_data;
        std::vector<int> evaluated_qubits;
        QuantumCircuitWrapper(int num_qubits);

        void apply_hadamard(int qubit);
        void apply_cnot(int control, int target);
        void apply_x(int qubit);
        void apply_barrier();
        void apply_hamiltonian_qiskit();
        void apply_ghz_qiskit();
        void execute_basic_quantum();
        void measure();
        std::vector<int32_t> parseToVector(void* ptr, size_t size, std::vector<int32_t> vec);
        std::string run();

private:
    int num_qubits;
    std::string gates;
    std::string scr;
    std::string generate_python_script(const std::string& circuit_name, int num_qubits, const std::string& gates);
    std::string execute_python_script(const std::string& script);
    std::vector<int> readQubits(const std::string& result);
};

#endif // QUANTUM_CIRCUIT_WRAPPER_H
