

  <h1>Real-Time Sensor Data Logging to SD Card</h1>

  <h2>Overview</h2>
  <p>This project implements a real-time sensor data logging system using an ARM-based microcontroller (STM32L072CZ). It reads data from a Bosch BMP280 pressure sensor via I2C communication, collects solar current value, temperature, and capacitor voltage sensor data, and logs this information to an SD card in real-time using SPI communication.</p>

  <h2>Tech Stack</h2>
  <ul>
    <li>Languages: Embedded C++</li>
    <li>Tools and Platforms:
      <ul>
        <li>IDE: Mbed Studio integrated into Visual Studio Code</li>
        <li>MBED OS: Open-source operating system</li>
        <li>Version Control System: Git</li>
      </ul>
    </li>
  </ul>

  <h2>Libraries and Frameworks</h2>
  <ul>
    <li>MBED OS Library: Provides standard C++ functions for STM32 microcontrollers, facilitating I/O and peripheral control.</li>
  </ul>

  <h2>Hardware</h2>
  <ul>
    <li>Microcontroller: STM32L072CZ</li>
    <li>Sensors:
      <ul>
        <li>Bosch BMP280 pressure sensor</li>
      </ul>
    </li>
    <li>Other Components:
      <ul>
        <li>SD card for data logging</li>
      </ul>
    </li>
  </ul>

  <h2>Features</h2>
  <ul>
    <li>I2C Communication: Configures and reads data from the BMP280 pressure sensor.</li>
    <li>SPI Communication: Utilizes SDBlockDevice and FATFileSystem APIs to log sensor data to an SD card.</li>
  </ul>

</body>
</html>
