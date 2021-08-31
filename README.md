# TFM_BLEMesh
This repository contains all code, research and other resources related with my Master Final Project in Internet of Things about BLE Mesh. The goal is to create a firmware which can be flashed into a ESP32 dev kit in order to work in a BLE Mesh network using the sensor model. Moreover, in order to provide to the user a way to monitoring and interact with the network, It will be created a CLI and a dashboard.

### 1. Project Structure

* **espCode:** Contains original code from [espressif](https://github.com/espressif/esp-idf) repository
* **samples:** Some C code that, after testing, have been included into src.
* **src:** Contains the code developed for this final project. It is splitted into differents folders depends on what piece of the architecture belongs to
  * **cli:** Contains a simple cli to interact with the BLE Mesh network. It is developed in Ruby.
  * **dashboard:** Contains configurations files and a dashboard ready to use. Also, there is a collect_data.rb script but it is not used.
  * **sensor_client:** Contains all the necessary code to build the client firmware.
  * **sensor_server:** Contains all the necessary code to build the server firmware.

### 2. System Architecture
![System Architecture](/img/BleMesh-Architecture.png)

### 3. Client Flow
![Client Flow](/img/ClientBLEMesh_Flow.png)
