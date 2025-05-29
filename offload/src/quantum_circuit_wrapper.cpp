#include "quantum_circuit_wrapper.h"
#include "omptarget.h"
 
QuantumCircuitWrapper::QuantumCircuitWrapper(int num_qubits) : num_qubits(num_qubits) {
    pid = -1;
}

void QuantumCircuitWrapper::apply_hadamard(int qubit) {
    gates += "circuit.h(" + std::to_string(qubit) + ")\n";
}

void QuantumCircuitWrapper::debug()
{
    std::cout<<scr<<std::endl;
}

void QuantumCircuitWrapper::apply_cnot(int control, int target) {
    gates += "circuit.cx(" + std::to_string(control) + ", " + std::to_string(target) + ")\n";
}

void QuantumCircuitWrapper::apply_x(int qubit) {
    gates += "circuit.x(" + std::to_string(qubit) + ")\n";
}

// void QuantumCircuitWrapper::apply_ry(std::vector<double> vec, int val) {
//     std::ostringstream oss;
//     oss << "[";
//     for (size_t i = 0; i < vec.size(); ++i) {
//         oss << vec[i];
//         if (i < vec.size() - 1) oss << ",";
//     }
//     oss << "]";

//     std::string json_vector = oss.str();

//     // Append to the gates string to be written into the Python script
//     gates += "apply_ry(" + json_vector + ", "+  std::to_string(val) + ")\n";
// }

void QuantumCircuitWrapper::close_pipes(){
    close(toPy[1]);
    close(fromPy[0]);
}

void QuantumCircuitWrapper::apply_ry(double angle, int val) {
    gates += "circuit.ry(" + std::to_string(angle) + ", "+  std::to_string(val) + ")\n";
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
    scr += "print(\"itr val is:\", input_itr, file=sys.stderr, flush=True)\n";
    scr += "for i in range(input_itr):\n";
    scr += "    resp = sys.stdin.readline().strip()\n"; // put execute in a loop
    scr += "    resp = re.sub(r',\\s*]', ']', resp)\n";
    scr += "    response_data = json.loads(resp)\n";
    scr += "    if isinstance(response_data, list) and len(response_data)==1 and isinstance(response_data[0], list):\n";
    scr += "        response_data = response_data[0]\n";
    scr += "    user_params = response_data[0]\n";
    scr += "    vals = [float(p) for p in user_params]\n";
    scr += "    bound_qc = qc.assign_parameters({ param: value for param, value in zip(qc.parameters, vals)})\n";
    scr += "    job = simulator.run(bound_qc, shots=10240)\n";
    scr += "    result = job.result()\n";
    scr += "    counts = result.get_counts()\n";
    scr += "    counts = json.dumps(counts)\n";
    // scr += "    print(counts)\n";
    scr += "    sys.stdout.write(counts)\n";
    scr += "    sys.stdout.flush()\n";
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
    script << "import re\n";
    script << "from qiskit import QuantumCircuit\n";
    script << "from qiskit.circuit import Parameter\n";
    script << "from qiskit_aer import AerSimulator\n";

    script << "if __name__ == \"__main__\":";
    script << "    circuit = QuantumCircuit(" << num_qubits << ")\n";
    std::string line;
    
    std::istringstream ss(params);
    while(std::getline(ss, line)) {
        script << "    " << line << "\n"; // Adds indentation to each line
    }

    std::istringstream ss2(gates);
    while(std::getline(ss2, line)) {
        script << "    " << line << "\n"; // Adds indentation to each line
    }

    std::string line2;
    std::istringstream ss3(scr);
    while(std::getline(ss3, line2)) {
        std::cout<<line2<<std::endl;  
        script << "    " << line2 << "\n"; // Adds indentation to each line
    }
    // script << "    while True:\n";
    // script << "        resp = sys.stdin.readline().strip()\n"; // put execute in a loop
    // script << "        resp = re.sub(r',\\s*]', ']', resp)\n";
    // script << "        response_data = json.loads(resp)\n";
    // script << "        if isinstance(response_data, list) and len(response_data)==1 and isinstance(response_data[0], list):\n";
    // script << "            response_data = response_data[0]\n";
    // script << "        user_params = response_data[0]\n";
    // script << "        vals = [float(p) for p in user_params]\n";
    // script << "        bound_qc = qc.assign_parameters({ param: value for param, value in zip(qc.parameters, vals)})\n";
    // script << "        job = simulator.run(bound_qc, shots=10240)\n";
    // script << "        result = job.result()\n";
    // script << "        counts = result.get_counts()\n";
    // script << "        counts = json.dumps(counts)\n";
    // script << "        print(counts)\n";
    return script.str();
}

std::vector<int> QuantumCircuitWrapper::readQubits(const std::string& result, int num_qs) {
    Json::Value root;
    Json::CharReaderBuilder reader;
    std::string errs;
    std::istringstream s(result);
    if (!Json::parseFromStream(reader, s, &root, &errs)) {
        std::cerr << "Failed to parse JSON: " << errs << std::endl;
        return {};
    }

    // Determine the number of possible states (bitstrings)
    size_t num_states = 1 << num_qs;
    std::vector<int> qubit_results(num_states, 0);

    // Parse JSON into vector<int> where index represents the bitstring
    for (Json::Value::const_iterator it = root.begin(); it != root.end(); ++it) {
        std::string bitstring = it.key().asString();
        int decimal_index = std::stoi(bitstring, nullptr, 2); // Convert "000", "001" -> 0, 1, 2...
        qubit_results[decimal_index] = it->asInt(); // Store the count at the correct index
    }

    return qubit_results;
}

std::string QuantumCircuitWrapper::returnJsonString(){
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
        if (i + 1 != vec_data.size())                  // **array** delimiter
            json_data += ',';
    }

    json_data += "]";

    return json_data;
}

void QuantumCircuitWrapper::exec_pipes(){
    std::string dat = returnJsonString() + '\n';
    std::cout<<"printing data send to python pipe"<<std::endl;
    std::cout<<dat<<std::endl;
    const char *msg = dat.c_str();
    write(toPy[1], msg, strlen(msg));
    char buffer[10000];
    ssize_t n = read(fromPy[0], buffer, sizeof(buffer)-1);
    if (n < 0) {
        perror("read");          // ← diagnose the real problem
        return;                  //   or throw / handle as you prefer
    }
    std::string result(buffer, n);
    std::cout<<"result from pipe is: "<<std::endl;
    std::cout<<result<<std::endl;
    evaluated_qubits = readQubits(result, num_qubits);
}

void QuantumCircuitWrapper::execute_python_script(const std::string& script) {
    
    std::string json_data = returnJsonString();

    int tid = omp_get_thread_num();
    pid_t pid = getpid();
    std::ostringstream oss;
    oss << "temp_script_" << pid << "_" << tid << ".py";
    std::string filename = oss.str();


    // Write the script to a temporary file
    std::ofstream file(filename);
    file << script;
    file.close();

    pipe2(toPy, O_CLOEXEC);// | O_NONBLOCK);
    pipe2(fromPy, O_CLOEXEC);// | O_NONBLOCK);

    child_pid = fork();

    if (child_pid == 0) {

        close(toPy[1]);
        close(fromPy[0]);

        // redirect stdin / stdout
        dup2(toPy[0],   STDIN_FILENO);   // stdin  ← pipe read‑end
        dup2(fromPy[1], STDOUT_FILENO);  // stdout → pipe write‑end
        // close the original fds (the dup’d copies are already marked CLOEXEC)
        close(toPy[0]);
        close(fromPy[1]);

        // build argv:  python3  <file>  <json>  <iterations>  NULL
        //std::string command = "python3 "+ filename + " " + json_data + " " + (char) num_iterations;
        std::string num_it_str = std::to_string(num_iterations);
        execlp("python3","python3", filename.c_str(), json_data.c_str(), num_it_str.c_str(), (char *)nullptr);
    }

    // // Run the script and capture the output
    // std::string command = "python3 "+ filename + " " + json_data + " " + (char) num_iterations;
    // char buffer[10000];
    // std::string result;
    // FILE* pipe = popen(command.c_str(), "r");
    // //if (!pipe) throw std::runtime_error("popen() failed!");
    
    // while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    //     result += buffer;
    // }

    // evaluated_qubits = readQubits(result, num_qubits);

    // pclose(pipe);

    //return result;
}

void QuantumCircuitWrapper::run() {
    std::string script = generate_python_script("circuit", num_qubits, gates);
    execute_python_script(script);
}
