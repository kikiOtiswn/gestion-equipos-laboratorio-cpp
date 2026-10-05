# 🔬 Sistema de Gestión de Equipos de Laboratorio (C++)

Aplicación de consola para administrar equipos de laboratorio universitario: carga de inventario, reservas por usuario, cierre de sesiones con penalizaciones y reportes de uso.

Proyecto académico de Programación Avanzada, construido sin STL (`std::string`, `vector`) para trabajar directamente con memoria dinámica, punteros y archivos.

## ✨ Funcionalidades

- **Carga de datos** desde archivos de texto delimitados (`*`) con reserva dinámica de memoria de tamaño exacto (lectura en dos pasadas).
- **Consulta por laboratorio:** listado de equipos y resumen con conteo por estado, costo total y % de disponibilidad.
- **Programación de sesiones** con validaciones encadenadas: existencia de usuario y equipo, disponibilidad y semestre mínimo requerido.
- **Cierre de sesiones:** actualización in-place del registro en archivo binario, cálculo de penalización (3% del costo del equipo por hora excedida) y paso a mantenimiento si se reporta daño.
- **Informe de uso intensivo:** equipo con más horas acumuladas en cada laboratorio.
- **Ranking de usuarios críticos:** top 3 por penalización promedio por sesión.

## 🛠️ Conceptos técnicos aplicados

- Memoria dinámica (`new[]` / `delete[]`) y doble puntero para reasignar desde funciones
- Recorridos con aritmética de punteros y arreglos paralelos
- Archivos binarios con acceso aleatorio (`seekg`, `seekp`, `tellg`) y registros de tamaño fijo
- Parsing con `strtok` y manejo de cadenas `char[]`
- Validación robusta de entradas con reintentos

## ▶️ Cómo ejecutarlo

```bash
g++ proyecto.cpp -o laboratorio
./laboratorio
```

Cuando el programa pida el nombre del archivo, usa `equipos.txt` (opción 1) y `usuarios.txt` (opción 2), incluidos en este repo como datos de ejemplo.

### Formato de los archivos de entrada

**equipos.txt** — `codigo * nombre * laboratorio * tipo * estado * costo * semestreMinimo * descripcion`
```
101 * Osciloscopio Digital * Lab 1 * Medicion * Disponible * 2500000 * 3 * 100 MHz, 2 canales
```

**usuarios.txt** — `codigoInstitucional * nombre * programa * semestre`
```
20231001 * Ana Perez * Ingenieria Electronica * 4
```

Las sesiones se guardan automáticamente en `sesiones.dat`.

## 👤 Autor

Kiki · Estudiante de Ciencia de Datos
