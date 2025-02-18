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

class QuantumCircuitWrapper {
public:
        std::string test;

        QuantumCircuitWrapper(int num_qubits);

        void apply_hadamard(int qubit);
        void apply_cnot(int control, int target);
        void apply_x(int qubit);
        void parseToVector(void* ptr, size_t size, std::vector<int32_t> vec);
        std::string run();

private:
    int num_qubits;
    std::string gates;
    std::vector<std::vector<int32_t> > vec_data;
    std::string generate_python_script(const std::string& circuit_name, int num_qubits, const std::string& gates);
    std::string execute_python_script(const std::string& script);
};

#endif // QUANTUM_CIRCUIT_WRAPPER_H
