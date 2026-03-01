# 🌫️ Sistema IoT de Monitoreo de Calidad del Aire – Sabana Centro

## 📌 Descripción del Proyecto

Este proyecto consiste en el desarrollo de un prototipo funcional IoT de bajo costo para el monitoreo en tiempo real de la calidad del aire en la región Sabana Centro (Cundinamarca).

El sistema mide múltiples variables ambientales críticas y aplica lógica de fusión de datos para generar alertas tempranas **in situ**, sin uso de redes de comunicación, cumpliendo las restricciones técnicas del reto académico.

---

## 🎯 Objetivo

Diseñar e implementar un sistema embebido capaz de:

- Medir material particulado (PM2.5 / PM10)
- Detectar gases contaminantes
- Registrar variables meteorológicas (temperatura, humedad y presión)
- Integrar múltiples señales mediante lógica de fusión
- Generar alertas locales mediante buzzer, LEDs y visualización en pantalla

---

## 🧠 Arquitectura del Sistema

El sistema se compone de tres subsistemas principales:

### 1️⃣ Sensado
- PMS5003 (Material particulado)
- MQ135 (Gases)
- BME280 (Temperatura, Humedad, Presión)

### 2️⃣ Procesamiento
- Microcontrolador ESP32
- Filtrado y validación de datos
- Normalización de variables
- Motor de fusión ambiental
- Clasificación del estado del aire

### 3️⃣ Actuación
- Pantalla LCD/OLED (visualización en tiempo real)
- Buzzer (alerta sonora)
- LEDs indicadores de severidad

---

## ⚙️ Restricciones del Reto

- No uso de Raspberry Pi
- No uso de redes de comunicación para alertas
- Sistema completamente autónomo
- Alertas exclusivamente físicas y visuales

---

## 📊 Clasificación del Aire

El sistema genera un índice compuesto de calidad del aire y lo clasifica en:

- 🟢 Bueno
- 🟡 Moderado
- 🟠 Dañino
- 🔴 Peligroso

Dependiendo del nivel, se activan patrones específicos de alerta.

---

## 🧪 Validación Experimental

Se realizaron pruebas controladas variando:

- Concentración de partículas
- Presencia de alcohol/gases
- Cambios ambientales simulados

Se verificó el comportamiento de la lógica de fusión y la activación correcta de alarmas.

---

## 👥 Integrantes

- Nombre 1
- Nombre 2
- Nombre 3

Curso: Internet de las Cosas  
Facultad de Ingeniería  
Universidad de La Sabana  
2026-1  

---

## 🎥 Video Demostrativo

[Enlace al video en MS Teams o plataforma correspondiente]

---

## 📚 Documentación Completa

La documentación técnica completa del proyecto se encuentra en la sección **Wiki** del repositorio:

👉 [Ir a la Wiki del Proyecto]

---

## 🤖 Uso de Inteligencia Artificial

En caso de haber utilizado herramientas de IA para apoyo técnico o redacción, se detalla su uso y validación en la sección correspondiente dentro de la Wiki.

---

## 📌 Estado del Proyecto

✔ Prototipo funcional  
✔ Lógica de fusión implementada  
✔ Validación experimental realizada  
✔ Documentación técnica en desarrollo  

---
