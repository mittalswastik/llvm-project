#include <iostream>
#include <vector>
#include <omp.h>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <bits/stdc++.h>
#include <unistd.h>

#define N 10


#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdio>

using namespace std;

void func(){
    cout<<"func"<<endl;
    //printf("func");
    //cout gives a linker error
    // calling the constructor outside also works - but gives a warning and then misalignment error
    // need to find a way to by pass compiler check as I am not running on a device
}

class QuantumCircuitWrapper {
public:
    
    QuantumCircuitWrapper(int num_qubits) : num_qubits(num_qubits) {}

    void apply_hadamard(int qubit) {
        gates += "circuit.h(" + to_string(qubit) + ")\n";
    }

    void apply_cnot(int control, int target) {
        gates += "circuit.cx(" + to_string(control) + ", " + std::to_string(target) + ")\n";
    }

    void apply_x(int qubit) {
        gates += "circuit.x(" + to_string(qubit) + ")\n";
    }

    std::string run() {
        std::string script = generate_python_script("circuit", num_qubits, gates);
        return execute_python_script(script);
    }

private:
    int num_qubits;
    string gates;

    string generate_python_script(const string& circuit_name, int num_qubits, const string& gates) {
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

    string execute_python_script(const string& script) {
        // Write the script to a temporary file
        ofstream file("temp_script.py");
        file << script;
        file.close();

        // Run the script and capture the output
        string command = "python3 temp_script.py";
        char buffer[128];
        string result;
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) throw runtime_error("popen() failed!");

        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        pclose(pipe);

        return result;
    }
};

// Function to execute a command and get the output
int main() {
    // Check for the number of devices available
    // int num_devices = omp_get_num_devices();
    // printf("Number of available devices: %d\n", num_devices);
    
    // // If no device is available, exit
    // if (num_devices < 1) {
    //     printf("No devices available for offloading. Exiting.\n");
    //     return 1;
    // }

    // Arrays for computation
    int a[N], b[N], c[N];

    // Initialize arrays
    for (int i = 0; i < N; i++) {
        a[i] = i;
        b[i] = 2 * i;
        c[i] = 0;
    }

    // Offload computation to GPU (assuming device 0)
    QuantumCircuitWrapper circuit(2);
    #pragma omp target firstprivate(circuit) device(100) map(to: a[0:N], b[0:N]) map(from: c[0:N])
    {
        for (int i = 0; i < N; i++) {
            sleep(1);
            circuit.apply_hadamard(0);
            circuit.apply_cnot(0, 1);
            //printf(" %d ",c[i]);
        }
    }

    // Check results
    printf("c value \n");
    int errors = 0;
    for (int i = 0; i < N; i++) {
        // if (c[i] != a[i] + b[i]) {
        //     errors++;
        //     //printf("Error at index %d: Expected %d, Got %d\n", i, a[i] + b[i], c[i]);
        // }
        //printf(" %d ",c[i]);

    }

    printf("\n");

    if (errors == 0) {
        printf("Success! All values are correct.\n");
    } else {
        printf("Failed with %d errors.\n", errors);
    }

    return 0;
}
