# AirQuality-IoT-FusionSystem

## Descripción del Proyecto

Este proyecto consiste en el diseño e implementación de un sistema embebido IoT de bajo costo para el monitoreo en tiempo real de la calidad del aire en la región Sabana Centro (Cundinamarca, Colombia).

El sistema integra múltiples variables ambientales (material particulado, gases y condiciones meteorológicas), aplica una lógica de fusión de datos para generar alertas tempranas in situ y complementa su funcionalidad mediante un sistema de monitoreo remoto a través de un tablero web embebido.

A diferencia de la primera fase, esta versión incorpora conectividad WiFi y un servidor web local, permitiendo la visualización en tiempo real y el acceso restringido a la información desde dispositivos conectados a la WLAN.

El sistema está desarrollado utilizando un ESP32 como microcontrolador principal, aprovechando sus capacidades de conectividad y multitarea.

---

## Objetivo

Diseñar e implementar un sistema IoT capaz de:

- Medir material particulado (PM2.5 / PM10)
- Detectar gases contaminantes
- Registrar variables meteorológicas (temperatura, humedad y presión)
- Integrar múltiples señales mediante lógica de fusión ambiental
- Generar alertas locales en tiempo real mediante actuadores físicos
- Publicar información en un dashboard web accesible dentro de la red local
- Permitir interacción con el sistema (control de alarmas)

---

## Arquitectura del Sistema

El sistema se organiza en cinco niveles funcionales que separan medición, análisis, comunicación y visualización:

### 1. Capa de Sensado
Encargada de capturar variables ambientales:
- **PMS5003** → Material particulado (UART)
- **MQ135** → Gases (entrada analógica)
- **BME280** → Temperatura, humedad y presión (I2C)

Cada sensor entrega datos independientes que luego son procesados por el microcontrolador.

---

### 2️. Capa de Procesamiento
El ESP32 realiza:

- Lectura periódica de sensores (cada 2 segundos)
- Validación de datos
- Cálculo de un índice compuesto de calidad del aire
- Clasificación del estado del aire mediante umbrales

La lógica de fusión asigna mayor peso al material particulado y a los gases por su impacto directo en la salud.

---

### 3️. Capa de Actuación e Interfaz Local
Comunica el estado del aire al usuario mediante:

- **Pantalla LCD** (visualización alternada de variables y estado)
- **Buzzer** (alerta sonora según nivel de riesgo)
- **LED RGB** (indicador visual inmediato)

El sistema mantiene la actualización de información de forma continua durante la operación.

---

### 4. Capa de Comunicación (IoT)

Se implementa conectividad mediante:

- Servidor web embebido en el ESP32
- Comunicación HTTP dentro de la red WLAN
- Acceso restringido a dispositivos conectados a la red local
- Autenticación de usuarios según rol

---

### 5. Capa de Aplicación (Dashboard Web)

Permite la interacción con el sistema a través de un navegador:

- Visualización en tiempo real de variables
- Histórico reciente de datos
- Indicadores visuales del estado del aire
- Control remoto de alarmas físicas

---

## Restricciones de Diseño

 **Restricción Energética**  
El sistema requiere alimentación externa (5V vía USB), por lo que no es completamente autónomo.

 **Restricción Temporal**  
La disponibilidad de sensores dependía de tiempos de envío internacionales, lo que condicionó el desarrollo.

 **Restricción Económica**  
Se estableció un presupuesto máximo de **50 USD**, priorizando componentes de bajo costo.

 **Restricción de Espacio**  
El sistema se implementa sobre una protoboard compacta con un ESP32.

 **Restricción Funcional del Reto**
- No se permite Raspberry Pi  
- No se permite el uso de MQTT  
- El sistema debe operar en una red WLAN local  
- El dashboard debe estar alojado en un servidor web embebido  

---

## Clasificación de la Calidad del Aire

El sistema calcula un índice compuesto a partir de:

- PM2.5
- Nivel estimado de gases
- Temperatura
- Humedad

Con base en este valor, el aire se clasifica en:

- 🟢 **Bueno (OK)**
- 🟡 **Advertencia (ADV)**
- 🔴 **Peligroso (PEL)**

Cada categoría activa un patrón específico de LED y buzzer.

---

## Validación Experimental

Se realizaron pruebas variando de manera controlada:

- Incremento de gases (simulación con alcohol)
- Aumento de material particulado
- Cambios en condiciones ambientales

Se verificó que:

- El sistema cambia de estado al superar umbrales definidos
- Las alertas locales se activan correctamente
- El dashboard refleja los cambios en tiempo real
- El sistema mantiene estabilidad durante la operación continua

---

## Hardware Utilizado

- ESP32 DevKit
- Sensor PMS5003
- Sensor MQ135
- Sensor BME280
- Pantalla LCD I2C
- Buzzer activo
- LED RGB

---

## Cómo ejecutar (rápido)

1. Abrir el archivo `.ino` en Arduino IDE  
2. Seleccionar la placa: **ESP32 Dev Module**  
3. Configurar credenciales WiFi en el código  
4. Subir el programa al ESP32  
5. Conectarse a la red WLAN  
6. Acceder al dashboard mediante la IP del dispositivo  

> Nota: Los detalles de conexión (pines) se documentan en la **Wiki**.

---

## Integrantes

- Brainer Steven Jimenez Gonzalez  
- Bruno Elias Pérez Merino  

Curso: Internet de las Cosas  
Facultad de Ingeniería  
Universidad de La Sabana  
2026-1  

---

## Documentación Técnica Completa

La documentación técnica completa (arquitectura detallada, diagramas, modelo de fusión, configuración experimental, resultados y análisis) se encuentra en la sección **Wiki** del repositorio.

[Ir a la Wiki](https://github.com/BrunoPMerino/AirQuality-IoT-FusionSystem/wiki)

---

## Estado del Proyecto

- Sistema IoT funcional  
- Lógica de fusión implementada  
- Dashboard web operativo  
- Comunicación local estable  
- Histórico de datos implementado  
- Sistema validado experimentalmente  