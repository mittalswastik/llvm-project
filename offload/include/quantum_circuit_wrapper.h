#ifndef QUANTUM_CIRCUIT_WRAPPER_H
#define QUANTUM_CIRCUIT_WRAPPER_H

#include <string>

class QuantumCircuitWrapper {
public:
        std::string test;

        QuantumCircuitWrapper(int num_qubits);

        void apply_hadamard(int qubit);
        void apply_cnot(int control, int target);
        void apply_x(int qubit);

        std::string run();

private:
    int num_qubits;
    std::string gates;

    std::string generate_python_script(const std::string& circuit_name, int num_qubits, const std::string& gates);
    std::string execute_python_script(const std::string& script);
};

#endif // QUANTUM_CIRCUIT_WRAPPER_H
