#include "quantum_circuit_wrapper.h"
 
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

void QuantumCircuitWrapper::apply_barrier(){
    gates += "circuit.barrier()\n";
}

void QuantumCircuitWrapper::measure(){
    gates += "circuit.measure_all()\n";
}

void QuantumCircuitWrapper::apply_hamiltonian_qiskit(){
    gates += "smarq = supermarq.hamiltonian_simulation.HamiltonianSimulation(" + std::to_string(num_qubits) + ")\n";
    gates += "circuit = smarq.qisket_circuit()\n";
    gates += "print(circuit)\n";
}

void QuantumCircuitWrapper::apply_ghz_qiskit(){
    gates += "smarq = supermarq.ghz.GHZ(" + std::to_string(num_qubits) + ")\n";
    gates += "circuit = smarq.qisket_circuit()\n";
    gates += "print(circuit)\n";
}

void QuantumCircuitWrapper::execute_basic_quantum(){
    scr += "backend_name = 'dax_code_simulator'\n";
    scr += "backend_name = 'dax_code_printer'\n";
    scr += "backend = dax.get_backend(backend_name)\n";
    scr += "backend.load_config(\"resources.toml\")\n";
    scr += "dax_job = execute(circuit, backend, shots=30, optimization_level=0)\n";
    scr += "client = sequre.UserClient()\n";
    scr += "workload = dax_job.get_dax()\n";
    scr += "print(workload)";
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
    script << "import sys\n";
    script << "import json\n";
    script << "import supermarq\n";
    script << "import qiskit\n";
    script << "import matplotlib.pyplot as plt\n";
    script << "import numpy as np\n";
    script << "from qiskit import QuantumCircuit, execute\n";
    script << "from qiskit.providers.dax import DAX\n";
    script << "import sequre\n";
    //processong function
    script << "def process_data(data):\n";
    script << "    # Example processing: square each number\n";
    script << "    return [[x * x for x in row] for row in data]\n\n";

    // Read JSON data from command line argument
    script << "if __name__ == \"__main__\":\n";
    script << "    if len(sys.argv) < 2:\n";
    script << "        print('Error: No input data provided')\n";
    script << "        sys.exit(1)\n\n";
    script << "    input_data = json.loads(sys.argv[1])\n";
    script << "    circuit = QuantumCircuit(" << num_qubits << "," <<num_qubits << ")\n";
    //script << "    processed_data = process_data(input_data)\n";
    std::string line;
    std::istringstream ss(gates);
    while(std::getline(ss, line)) {
        script << "    " << line << "\n"; // Adds indentation to each line
    }

    std::string line2;
    std::istringstream ss2(scr);
    while(std::getline(ss2, line2)) {
        std::cout<<line2<<std::endl;  
        script << "    " << line2 << "\n"; // Adds indentation to each line
    }
    //script << gates;
    // script << "    backend_name = 'dax_code_simulator'\n";
    // script << "    backend_name = 'dax_code_printer'\n";
    // script << "    backend = dax.get_backend(backend_name)\n";
    // script << "    backend.load_config("<<"\"resources.toml\""<<")\n";
    // script << "    dax_job = execute(circuit, backend, shots=30, optimization_level=0)\n";
    // script << "    client = sequre.UserClient()\n";
    // script << "    workload = dax_job.get_dax()\n";
    // script << "    print(workload)";
    return script.str();
}

std::vector<int> QuantumCircuitWrapper::readQubits(const std::string& result) {
    Json::Value root;
    Json::CharReaderBuilder reader;
    std::string errs;
    std::istringstream s(result);
    if (!Json::parseFromStream(reader, s, &root, &errs)) {
        std::cerr << "Failed to parse JSON: " << errs << std::endl;
        return {};
    }

    // Determine the number of possible states (bitstrings)
    size_t num_states = root.size();
    std::vector<int> qubit_results(num_states, 0);

    // Parse JSON into vector<int> where index represents the bitstring
    for (Json::Value::const_iterator it = root.begin(); it != root.end(); ++it) {
        std::string bitstring = it.key().asString();
        int decimal_index = std::stoi(bitstring, nullptr, 2); // Convert "000", "001" -> 0, 1, 2...
        qubit_results[decimal_index] = it->asInt(); // Store the count at the correct index
    }

    return qubit_results;
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

    evaluated_qubits = readQubits(result);

    pclose(pipe);

    return result;
}

std::string QuantumCircuitWrapper::run() {
    std::string script = generate_python_script("circuit", num_qubits, gates);
    return execute_python_script(script);
}
