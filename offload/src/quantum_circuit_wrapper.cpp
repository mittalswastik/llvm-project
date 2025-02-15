#include "quantum_circuit_wrapper.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include <iostream>
 
QuantumCircuitWrapper::QuantumCircuitWrapper(int num_qubits) : num_qubits(num_qubits) {}

void QuantumCircuitWrapper::apply_hadamard(int qubit) {
    gates += "circuit.h(" + std::to_string(qubit) + ")\n";
}

void QuantumCircuitWrapper::apply_cnot(int control, int target) {
    gates += "circuit.cx(" + std::to_string(control) + ", " + std::to_string(target) + ")\n";
}

void QuantumCircuitWrapper::apply_x(int qubit) {
    gates += "circuit.x(" + std::to_string(qubit) + ")\n";
}


std::string QuantumCircuitWrapper::generate_python_script(const std::string& circuit_name, int num_qubits, const std::string& gates) {
    std::ostringstream script;
    script << "from qiskit import QuantumCircuit, Aer, execute\n";
    script << "circuit = QuantumCircuit(" << num_qubits << ")\n";
    script << gates;
    script << "backend = Aer.get_backend('statevector_simulator')\n";
    script << "result = execute(circuit, backend).result()\n";
    script << "statevector = result.get_statevector()\n";
    script << "print(statevector)\n";
    return script.str();
}

std::string QuantumCircuitWrapper::execute_python_script(const std::string& script) {
    // Write the script to a temporary file
    std::ofstream file("temp_script.py");
    file << script;
    file.close();

    // Run the script and capture the output
    std::string command = "python3 temp_script.py";
    char buffer[128];
    std::string result;
    FILE* pipe = popen(command.c_str(), "r");
    //if (!pipe) throw std::runtime_error("popen() failed!");
    
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);

    return result;
}

std::string QuantumCircuitWrapper::run() {
    std::string script = generate_python_script("circuit", num_qubits, gates);
    return execute_python_script(script);
}
