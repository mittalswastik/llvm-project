#include "quantum_circuit_wrapper.h"
 
QuantumCircuitWrapper::QuantumCircuitWrapper(int num_qubits) : num_qubits(num_qubits) {}

void QuantumCircuitWrapper::apply_hadamard(int qubit) {
    gates += " circuit.h(" + std::to_string(qubit) + ")\n";
}

void QuantumCircuitWrapper::apply_cnot(int control, int target) {
    gates += " circuit.cx(" + std::to_string(control) + ", " + std::to_string(target) + ")\n";
}

void QuantumCircuitWrapper::apply_x(int qubit) {
    gates += " circuit.x(" + std::to_string(qubit) + ")\n";
}

std::vector<int32_t> QuantumCircuitWrapper::parseToVector(void* ptr, size_t size, std::vector<int32_t> vec){
    std::cout<<" size of the the vector is: "<<size<<std::endl;
    intptr_t intPtr = reinterpret_cast<intptr_t> (ptr); // Cast void* to int*
    int32_t *intVal = reinterpret_cast<int32_t*>(intPtr);
    size = size/sizeof(int32_t);
    std::cout<<"array value is"<<std::endl;
    for(int i = 0 ; i < size ; i++){
        std::cout<<intVal[i]<<" "<<std::endl;
    }

    std::cout<<"end of array value"<<std::endl;
    vec.assign(intVal, intVal + size);   // Populate vector using a range
    return vec;
}


std::string QuantumCircuitWrapper::generate_python_script(const std::string& circuit_name, int num_qubits, const std::string& gates) {
    std::ostringstream script;
    script << "from qiskit import QuantumCircuit, Aer, execute\n";
    script << "circuit = QuantumCircuit(" << num_qubits << ")\n";
    //processong function
    script << "def process_data(data):\n";
    script << "    # Example processing: square each number\n";
    script << "    return [[x * x for x in row] for row in data]\n\n";

    // Read JSON data from command line argument
    script << "if __name__ == \"__main__\":\n";
    script << " if len(sys.argv) < 2:\n";
    script << "     print('Error: No input data provided')\n";
    script << "     sys.exit(1)\n\n";

    script << " input_data = json.loads(sys.argv[1])\n";
    script << " processed_data = process_data(input_data)\n";
    script << " print('Processed Data:', processed_data)\n\n";
    script << gates;
    script << " backend = Aer.get_backend('statevector_simulator')\n";
    script << " result = execute(circuit, backend).result()\n";
    script << " statevector = result.get_statevector()\n";
    script << " print(statevector)\n";
    return script.str();
}

std::string QuantumCircuitWrapper::execute_python_script(const std::string& script) {
    std::string json_data = "[";
    for (int32_t i = 0; i < vec_data.size(); ++i) {
        json_data += "[";
        for(int32_t j = 0; j < vec_data[i].size(); ++j){
        json_data += std::to_string(vec_data[i][j]);
        if (j < vec_data[i].size() - 1) {
            json_data += ",";
        }
        }

        json_data += "]";
    }

    json_data += "]";
    
    // Write the script to a temporary file
    std::ofstream file("temp_script.py");
    file << script;
    file.close();

    // Run the script and capture the output
    std::string command = "python3 temp_script.py "+json_data;
    char buffer[4000];
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
