# ArduinoUno-AirQuality-FusionSystem

## Descripción del Proyecto

Este proyecto consiste en el diseño e implementación de un prototipo embebido IoT de bajo costo para el monitoreo en tiempo real de la calidad del aire en la región Sabana Centro (Cundinamarca, Colombia).

El sistema integra múltiples variables ambientales (material particulado, gases y condiciones meteorológicas) y aplica una lógica de fusión de datos para generar alertas tempranas **in situ**, sin utilizar redes de comunicación, cumpliendo con las restricciones técnicas del reto académico.

El sistema está desarrollado utilizando un **Arduino Uno** como microcontrolador principal.

---

## Objetivo

Diseñar e implementar un sistema embebido capaz de:

- Medir material particulado (PM2.5 / PM10)
- Detectar gases contaminantes
- Registrar variables meteorológicas (temperatura, humedad y presión)
- Integrar múltiples señales mediante lógica de fusión ambiental
- Generar alertas locales en tiempo real mediante actuadores físicos

---

## Arquitectura del Sistema

El sistema se organiza en tres niveles funcionales que separan medición, análisis y respuesta:

### 1 Capa de Sensado
Encargada de capturar variables ambientales:
- **PMS5003** → Material particulado (UART)
- **MQ135** → Gases (entrada analógica)
- **BME280** → Temperatura, humedad y presión (I2C)

Cada sensor entrega datos independientes que luego son procesados por el microcontrolador.

### 2️ Capa de Procesamiento
El Arduino Uno realiza:
- Lectura periódica de sensores
- Validación básica de datos
- Cálculo de un **índice compuesto** de calidad del aire (combinando las variables con distinta importancia)
- Clasificación del estado del aire mediante umbrales

La lógica de fusión asigna mayor peso al material particulado y a los gases por su impacto directo en la salud.

### 3️ Capa de Actuación e Interfaz
Comunica el estado del aire al usuario mediante:
- **Pantalla LCD** (visualización alternada de variables y estado)
- **Buzzer** (alerta sonora según nivel de riesgo)
- **LED RGB** (indicador visual inmediato)

El sistema mantiene la actualización de información de forma continua durante la operación.

---

## Restricciones de Diseño

 **Restricción Energética**  
El sistema requiere alimentación externa (5V vía USB), por lo que no es completamente autónomo. No se implementó sistema de baterías dentro del alcance del proyecto.

 **Restricción Temporal**  
La disponibilidad de sensores dependía de tiempos de envío internacionales, lo que condicionó la selección de componentes y la planificación del montaje.

 **Restricción Económica**  
Aunque el reto sugiere “bajo costo” (< 100 USD), el equipo estableció un presupuesto máximo de **50 USD**, influyendo en la selección de componentes accesibles.

 **Restricción de Espacio**  
El montaje se realizó con un Arduino Uno y una protoboard compacta, definiendo el tamaño mínimo físico del sistema.

 **Restricción Funcional del Reto**
- No se permite Raspberry Pi  
- No se permiten redes de comunicación para alertas  
- Las notificaciones deben ser exclusivamente locales (visuales y sonoras)

---

##  Clasificación de la Calidad del Aire

El sistema calcula un índice compuesto a partir de:
- PM2.5
- Nivel estimado de gases
- Temperatura
- Humedad

Con base en este valor, el aire se clasifica en:
- 🟢 **Bueno**
- 🟡 **Moderado**
- 🔴 **Peligroso**

Cada categoría activa un patrón específico de LED y buzzer, permitiendo identificar el nivel de riesgo de forma inmediata.

---

## Validación Experimental

Se realizaron pruebas variando de manera controlada:
- Incremento de gases (simulación con alcohol)
- Aumento de material particulado
- Cambios en condiciones ambientales

Se verificó que:
- El sistema cambia de estado al superar umbrales definidos
- Las alertas visuales y sonoras se activan según el nivel detectado
- La visualización permanece estable y actualizada durante la operación

---

## Hardware Utilizado

- Arduino Uno
- Sensor PMS5003
- Sensor MQ135
- Sensor BME280
- Pantalla LCD I2C
- Buzzer activo
- LED RGB

---

## Cómo ejecutar (rápido)

1. Abrir el archivo `.ino` del proyecto en Arduino IDE.  
2. Instalar las librerías necesarias (ver Wiki).  
3. Conectar el Arduino Uno por USB y cargar el programa.

> Nota: Los detalles de conexión (pines), librerías y configuración se documentan en la **Wiki**.

---

##  Integrantes

- Brainer Steven Jimenez Gonzalez  
- Bruno Elias Pérez Merino  

Curso: Internet de las Cosas  
Facultad de Ingeniería  
Universidad de La Sabana  
2026-1  

---

##  Video Demostrativo

[Insertar enlace al video en MS Teams]

---

##  Documentación Técnica Completa

La documentación técnica completa (arquitectura detallada, diagramas, modelo de fusión, configuración experimental, resultados y análisis) se encuentra en la sección **Wiki** del repositorio.

[Ir a la Wiki]

---

## Estado del Proyecto

✔ Prototipo funcional  
✔ Lógica de fusión implementada  
✔ Sistema de alertas validado  
✔ Documentación técnica en desarrollo  
