# 🌫️ ArduinoUno-AirQuality-FusionSystem

## 📌 Project Overview

This project consists of the design and implementation of a low-cost embedded IoT prototype for real-time air quality monitoring in Sabana Centro (Cundinamarca, Colombia).

The system integrates multiple environmental signals and applies data fusion logic to generate reliable in situ alerts without the use of communication networks.

Developed using an Arduino Uno as the central embedded system.

---

## 🎯 Objective

To design and implement an embedded system capable of:

- Measuring particulate matter (PM2.5 / PM10)
- Detecting pollutant gases
- Monitoring meteorological variables (temperature, humidity, pressure)
- Combining multiple environmental signals through data fusion
- Generating real-time local alerts using physical actuators

---

## 🧠 System Architecture

The system is structured into three main subsystems:

### 1️⃣ Sensing Layer
- PMS5003 → Particulate Matter (UART)
- MQ135 → Gas concentration (Analog input)
- BME280 → Temperature, Humidity, Pressure (I2C)

### 2️⃣ Processing Layer
- Arduino Uno
- Signal acquisition module
- Data filtering and validation
- Environmental data normalization
- Fusion engine (composite air quality index)
- Threshold classification logic

### 3️⃣ Actuation & Interface Layer
- LCD display (real-time visualization)
- Buzzer (acoustic alert)
- RGB LED indicators (air quality status)

---

## 🚫 Design Constraints

- No Raspberry Pi allowed
- No communication networks for alerting
- Fully autonomous operation
- In situ alerts only (visual + sound)

---

## 📊 Air Quality Classification

The system computes a composite environmental score and classifies air quality into:

- 🟢 Good
- 🟡 Moderate
- 🟠 Harmful
- 🔴 Dangerous

Each level triggers specific visual and acoustic patterns.

---

## 🧪 Experimental Validation

The system was tested under controlled variations of:

- Particulate exposure
- Gas presence (alcohol simulation)
- Environmental parameter changes

The fusion logic and alert activation were validated under critical scenarios.

---

## 🛠️ Hardware Used

- Arduino Uno
- PMS5003
- MQ135
- BME280
- LCD Display (I2C)
- Active Buzzer
- RGB LED

---

## 👥 Team Members

- Name 1
- Name 2
- Name 3

Course: Internet of Things  
Faculty of Engineering  
Universidad de La Sabana  
2026-1  

---

## 🎥 Demonstration Video

[Insert MS Teams or public video link here]

---

## 📚 Full Technical Documentation

The complete technical documentation, including:

- Design constraints
- System architecture diagrams
- UML diagrams
- Fusion logic design
- Experimental configuration
- Results and analysis
- AI usage disclosure (if applicable)

is available in the **Wiki section** of this repository.

👉 [Go to Wiki]

---

## 📌 Project Status

✔ Functional prototype  
✔ Data fusion implemented  
✔ In situ alert system validated  
✔ Technical documentation in progress  

---
