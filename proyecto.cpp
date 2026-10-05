#include <iostream>
#include <cstring>   // strcmp, strcpy, strtok, strlen -> funciones para manejar char[] sin usar std::string
#include <fstream>   // ifstream, ofstream, fstream -> leer y escribir archivos de texto y binarios
using namespace std;

// Constante (no variable global mutable) con el nombre del archivo binario donde viven TODAS las sesiones
// de uso. Se declara una sola vez aca para no repetir el string "sesiones.dat" por todo el codigo.
const char* ARCHIVO_SESIONES = "sesiones.dat";

// ---------------------------------------------------------------------------
// STRUCTS
// Los 3 tipos de datos que maneja el sistema. OJO: todos usan char[] de tamano
// fijo (nunca std::string) porque estas structs se escriben/leen directo a
// archivos binarios con sizeof(struct), y std::string guarda un puntero interno
// que rompe ese mecanismo.
// ---------------------------------------------------------------------------

// Representa un equipo de laboratorio (osciloscopio, multimetro, etc.)
struct Equipo {
    int codigo;                    // identificador unico del equipo
    char nombre[50];                // nombre del equipo
    char laboratorio[40];           // en que laboratorio fisico esta ubicado
    char tipo[30];                  // categoria/tipo de equipo
    char estadoOperativo[20];       // "Disponible" | "En uso" | "Mantenimiento"
    float costoEstimado;            // valor economico del equipo (se usa para calcular penalizaciones)
    int semestreMinimo;             // semestre minimo que debe cursar un usuario para poder reservarlo
    char descripcionTecnica[100];   // texto libre con detalles tecnicos
};

// Representa a un estudiante/usuario que puede reservar equipos
struct Usuario {
    int codigoInstitucional;        // identificador unico del usuario (codigo de la universidad)
    char nombre[50];                 // nombre del usuario
    char programaAcademico[50];      // carrera que estudia

    int semestre;                    // semestre que cursa actualmente
};

// Representa UNA reserva/uso de un equipo por parte de un usuario.
// A diferencia de Equipo y Usuario (que se cargan una vez desde texto y viven
// en memoria), las sesiones se escriben directo a un archivo binario apenas se
// crean, porque son un historial que crece con el tiempo y necesita persistir
// aunque el programa se cierre.
struct SesionUso {
    int codigoSesion;               // identificador unico de la sesion (correlativo, se calcula solo)
    int codigoEquipo;                // que equipo se uso (referencia al codigo de Equipo)
    int codigoUsuario;                // quien lo uso (referencia al codigoInstitucional de Usuario)
    char fecha[20];                   // fecha en que se programo la sesion
    int duracionProgramada;           // horas que se reservaron originalmente
    int duracionReal;                 // horas que realmente se usaron (se llena al cerrar la sesion)
    bool cerrada;                     // false = sesion abierta/en curso, true = ya se cerro y liquido
    char observacion[100];            // notas tecnicas que deja quien cierra la sesion
    float penalizacion;               // dinero a pagar si se paso del tiempo programado
};

// ---------------------------------------------------------------------------
// FUNCIONES DE ENTRADA / VALIDACION
// Encapsulan la lectura de datos por teclado para no repetir la logica de
// "pedir, validar, reintentar" en cada opcion del menu.
// ---------------------------------------------------------------------------

// Pide por teclado el nombre de un archivo y lo guarda en nombreArchivo.
// Recibe el arreglo directo (no hace falta doble puntero: no se reserva memoria
// nueva aca, solo se llena un arreglo que ya existe en quien llama).
void ingresarArchivo(char nombreArchivo[], int tam) {
    cout<<"Ingrese nombre del archivo: ";
    cin.getline(nombreArchivo, tam);
}

// Quita los espacios en blanco sobrantes al inicio y al final de una cadena.
// Se usa despues de cada strtok(), porque el formato de los archivos de texto
// separa los campos con "*" y puede quedar espacio pegado a cada token
// (ej: " Laboratorio 3 " -> "Laboratorio 3").
void limpiarEspacios(char texto[]) {
    int inicio = 0;
    while (texto[inicio] == ' ') inicio++;       // avanza mientras haya espacios al inicio

    int fin = strlen(texto) - 1;
    while (fin >= inicio && texto[fin] == ' ') fin--;   // retrocede mientras haya espacios al final

    // Reconstruye el texto sin los espacios sobrantes, sobre el mismo arreglo
    int j = 0;
    for (int i = inicio; i <= fin; i++, j++) {
        texto[j] = texto[i];
    }
    texto[j] = '\0';   // cierra la cadena en su nuevo largo
}

// Limpia el estado de error de cin y descarta lo que haya quedado en el buffer
// de entrada. Se llama SIEMPRE despues de una lectura fallida o despues de usar
// cin >> antes de un cin.getline(), porque cin >> deja el '\n' pendiente en el
// buffer y eso rompe la siguiente lectura de linea completa si no se limpia.
void limpiarEntrada() {
    cin.clear();               // quita las banderas de error (failbit/eofbit) de cin
    cin.ignore(10000, '\n');   // descarta caracteres hasta el proximo salto de linea
}

// Pide un texto por teclado con reintentos: hasta 3 intentos si el usuario deja
// el campo vacio o si la entrada falla por algun motivo. Devuelve true si logro
// leer algo valido, false si se agotaron los intentos (asi quien llama puede
// cancelar la operacion en vez de seguir con datos invalidos).
bool leerTexto(const char* mensaje, char* destino, int tam) {
    int intentos = 0;

    while (intentos < 3) {
        cout << mensaje;

        if (!cin.getline(destino, tam)) {
            // la lectura fallo (por ejemplo el texto no cupo en el buffer)
            limpiarEntrada();
            cout << "Texto invalido (maximo " << (tam - 1) << " caracteres).\n";
            intentos++;
            continue;
        }

        if (destino[0] == '\0') {
            // el usuario solo presiono Enter, cadena vacia
            cout << "No puede quedar vacio.\n";
            intentos++;
            continue;
        }
        return true;   // exito: hay texto valido en destino
    }

    cout << "Demasiados intentos, se cancela la operacion.\n";
    return false;
}

// Pide un numero entero por teclado, validando que sea realmente un numero y
// que caiga dentro de [minimo, maximo]. Recibe destino como puntero (int*)
// para poder escribir el resultado directo en la variable de quien llama, sin
// necesidad de devolverlo con return (el return se reserva para avisar si la
// operacion tuvo exito o no).
bool leerEntero(const char* mensaje, int* destino, int minimo, int maximo) {
    int intentos = 0;

    while (intentos < 3) {
        cout << mensaje;

        if (!(cin >> *destino)) {
            // el usuario escribio algo que no es un numero (ej. letras)
            limpiarEntrada();
            cout << "Debe escribir un numero.\n";
            intentos++;
            continue;
        }
        limpiarEntrada();   // limpia el '\n' que deja pendiente cin >> antes de cualquier getline posterior

        if (*destino < minimo || *destino > maximo) {
            cout << "El valor debe estar entre " << minimo << " y " << maximo << ".\n";
            intentos++;
            continue;
        }
        return true;
    }

    cout << "Demasiados intentos, se cancela la operacion.\n";
    return false;
}

// ---------------------------------------------------------------------------
// BUSQUEDA Y UTILIDADES SOBRE LOS ARREGLOS EN MEMORIA
// Recorridos SIEMPRE con aritmetica de punteros (nunca con [i]), como exige
// el curso.
// ---------------------------------------------------------------------------

// Busca un equipo por su codigo dentro del arreglo dinamico de equipos.
// Devuelve un puntero al equipo encontrado, o nullptr si no existe ninguno con
// ese codigo. Quien llama SIEMPRE debe revisar si el resultado es nullptr
// antes de usar -> sobre el, porque usar -> sobre nullptr es un error grave
// (undefined behavior / caida del programa).
Equipo* buscarEquipo(Equipo* equipos, int numEquipos, int codigo) {
    Equipo* p   = equipos;              // puntero que arranca en el primer elemento
    Equipo* fin = equipos + numEquipos; // puntero "un pasito despues del ultimo" (limite del recorrido)
    while (p < fin) {
        if (p->codigo == codigo) return p;   // encontrado: se devuelve la direccion de este elemento
        p++;                                  // avanza al siguiente Equipo del arreglo
    }
    return nullptr;   // se recorrio todo el arreglo y no aparecio ese codigo
}

// Igual que buscarEquipo, pero para el arreglo de usuarios, comparando por
// codigoInstitucional.
Usuario* buscarUsuario(Usuario* usuarios, int numUsuarios, int codigo) {
    Usuario* p   = usuarios;
    Usuario* fin = usuarios + numUsuarios;
    while (p < fin) {
        if (p->codigoInstitucional == codigo) return p;
        p++;
    }
    return nullptr;
}

// Indica si un equipo esta en estado "Disponible". Se compara con strcmp()
// (NUNCA con ==) porque estadoOperativo es un char[], y == entre char[]
// compararia direcciones de memoria, no el contenido del texto.
bool estaDisponible(Equipo* eq) {
    return strcmp(eq->estadoOperativo, "Disponible") == 0;
}

// ---------------------------------------------------------------------------
// CARGA DE ARCHIVOS DE TEXTO (opciones 1 y 2 del menu)
// Ambas funciones siguen el mismo patron de "dos pasadas":
//   1) recorrer el archivo solo para CONTAR cuantas lineas/registros hay
//   2) rebobinar el archivo y recorrerlo de nuevo para LLENAR un arreglo
//      dinamico que ya se reservo del tamano exacto necesario
// Reciben Equipo**/Usuario** (doble puntero) porque necesitan reasignar la
// variable puntero que vive en main() (equipos/usuarios) -- para modificar
// una variable puntero desde otra funcion hace falta la direccion de esa
// variable, es decir, un puntero a puntero.
// ---------------------------------------------------------------------------

// Carga los equipos desde un archivo de texto donde cada linea trae los 8
// campos de un Equipo separados por el caracter '*'.
// nombreArchivo: ruta del archivo a leer.
// equipos: direccion del puntero Equipo* que vive en main (se le asigna el
//          arreglo nuevo reservado aca dentro).
// numEquipos: direccion del contador que vive en main (se actualiza con la
//             cantidad de equipos leidos).
void cargarEquipos(char nombreArchivo[], Equipo** equipos, int* numEquipos) {
    ifstream archivo(nombreArchivo);   // intenta abrir el archivo en modo lectura de texto

    if (!archivo) {
        cout<<"No se pudo abrir el archivo.\n";
        return;
    }

    // --- PRIMERA PASADA: solo contar cuantas lineas (=cuantos equipos) hay ---
    char linea[200];
    while (archivo.getline(linea, 200)) {
        (*numEquipos)++;   // se incrementa el contador que vive en main, via el puntero recibido
    }

    // Despues de leer hasta el final, el stream queda en estado de error/EOF.
    // Hay que limpiarlo (clear) antes de poder rebobinar y volver a leer.
    archivo.clear();
    archivo.seekg(0);   // mueve el cursor de lectura de vuelta al inicio del archivo

    // Ahora que se conoce el total exacto, se reserva el arreglo dinamico del
    // tamano correcto (no se puede reservar antes de saber cuantos hay).
    *equipos = new Equipo[*numEquipos];
    Equipo* p = *equipos;   // puntero de trabajo que va recorriendo el arreglo recien creado

    // --- SEGUNDA PASADA: leer cada linea de verdad y llenar el arreglo ---
    while (archivo.getline(linea, 200)) {
        // strtok() va cortando la linea en pedazos ("tokens") separados por '*'.
        // La primera llamada recibe la cadena original; las siguientes llamadas
        // sobre la MISMA linea usan NULL como primer argumento para indicar
        // "seguir cortando donde quedo la vez anterior". OJO: strtok MODIFICA
        // la cadena original (pone '\0' donde estaban los '*').
        char* token = strtok(linea, "*");
        limpiarEspacios(token);          // saca espacios sobrantes alrededor del dato
        p->codigo = atoi(token);          // atoi convierte el texto del token a int

        token = strtok(NULL, "*");
        limpiarEspacios(token);
        strcpy(p->nombre, token);         // strcpy copia el texto del token al campo char[] de la struct

        token = strtok(NULL, "*");
        limpiarEspacios(token);
        strcpy(p->laboratorio, token);

        token = strtok(NULL, "*");
        limpiarEspacios(token);
        strcpy(p->tipo, token);

        token = strtok(NULL, "*");
        limpiarEspacios(token);
        strcpy(p->estadoOperativo, token);

        token = strtok(NULL, "*");
        limpiarEspacios(token);
        p->costoEstimado = atof(token);   // atof convierte texto a float/double

        token = strtok(NULL, "*");
        limpiarEspacios(token);
        p->semestreMinimo = atoi(token);

        token = strtok(NULL, "*");
        limpiarEspacios(token);
        strcpy(p->descripcionTecnica, token);

        p++;   // avanza al siguiente casillero del arreglo para el proximo equipo
    }

    archivo.close();
    cout<<"Se cargaron "<<*numEquipos<<" equipos correctamente.\n";
}

// Misma logica que cargarEquipos, pero para Usuario (que tiene menos campos:
// codigo institucional, nombre, programa academico y semestre).
void cargarUsuarios(char nombreArchivo[], Usuario** usuarios, int* numUsuarios) {

    ifstream archivo(nombreArchivo);

    if (!archivo) {
        cout<<"No se pudo abrir el archivo.\n";
        return;
    }

    // Primera pasada: contar lineas/usuarios
    char linea[200];
    while (archivo.getline(linea, 200)) {
        (*numUsuarios)++;
    }

    archivo.clear();
    archivo.seekg(0);

    // Reservar el arreglo del tamano exacto
    *usuarios = new Usuario[*numUsuarios];
    Usuario* p = *usuarios;

    // Segunda pasada: leer y llenar
    while (archivo.getline(linea, 200)) {
        char* token = strtok(linea, "*");
        limpiarEspacios(token);
        p->codigoInstitucional = atoi(token);

        token = strtok(NULL, "*");
        limpiarEspacios(token);
        strcpy(p->nombre, token);

        token = strtok(NULL, "*");
        limpiarEspacios(token);
        strcpy(p->programaAcademico, token);

        token = strtok(NULL, "*");
        limpiarEspacios(token);
        p->semestre = atoi(token);

        p++;
    }

    archivo.close();
    cout<<"Se cargaron "<<*numUsuarios<<" usuarios correctamente.\n";
}

// ---------------------------------------------------------------------------
// OPCION 3 DEL MENU
// ---------------------------------------------------------------------------

// Muestra todos los equipos de un laboratorio (pedido por teclado) y un
// resumen con conteos por estado, costo total y porcentaje disponible.
// Es un solo recorrido del arreglo que filtra por laboratorio y va
// ACUMULANDO estadisticas sobre la marcha (no necesita guardar nada en un
// arreglo nuevo, solo va sumando contadores mientras pasa por cada equipo).
void consultarEstadoLaboratorio(Equipo* equipos, int numEquipos) {
    if (numEquipos == 0) {
        cout << "Primero debe cargar los equipos (opcion 1).\n";
        return;
    }

    char laboratorio[40];
    if (!leerTexto("Nombre del laboratorio: ", laboratorio, 40)) return;

    // Contadores que se van a ir acumulando mientras se recorre el arreglo
    int encontrados   = 0;
    int disponibles   = 0;
    int enUso         = 0;
    int mantenimiento = 0;
    int otroEstado    = 0;
    double costoTotal = 0;

    cout << "\n===== ESTADO OPERATIVO DEL LABORATORIO " << laboratorio << " =====\n";

    Equipo* p   = equipos;
    Equipo* fin = equipos + numEquipos;
    while (p < fin) {
        // Solo procesa el equipo si pertenece al laboratorio que se pidio
        if (strcmp(p->laboratorio, laboratorio) == 0) {
            cout << "\nCodigo: " << p->codigo << " | " << p->nombre << "\n";
            cout << "  Tipo: " << p->tipo << " | Estado: " << p->estadoOperativo << "\n";
            cout << "  Semestre minimo: " << p->semestreMinimo
                 << " | Costo estimado: " << (long)p->costoEstimado << "\n";
            cout << "  " << p->descripcionTecnica << "\n";

            encontrados++;
            costoTotal = costoTotal + p->costoEstimado;

            // Clasifica el estado del equipo en uno de los 4 contadores
            if (strcmp(p->estadoOperativo, "Disponible") == 0)         disponibles++;
            else if (strcmp(p->estadoOperativo, "En uso") == 0)        enUso++;
            else if (strcmp(p->estadoOperativo, "Mantenimiento") == 0) mantenimiento++;
            else                                                       otroEstado++;
        }
        p++;
    }

    if (encontrados == 0) {
        cout << "No hay equipos registrados en ese laboratorio.\n";
        return;
    }

    cout << "\n--- RESUMEN ---\n";
    cout << "Equipos en el laboratorio: " << encontrados << "\n";
    cout << "  Disponibles   : " << disponibles << "\n";
    cout << "  En uso        : " << enUso << "\n";
    cout << "  Mantenimiento : " << mantenimiento << "\n";
    cout << "  Otro estado   : " << otroEstado << "\n";
    cout << "Costo total de los equipos: " << (long)costoTotal << "\n";
    // Se calcula el porcentaje solo despues de confirmar encontrados > 0,
    // para no dividir entre cero.
    cout << "Porcentaje disponible: " << (disponibles * 100) / encontrados << " %\n";

    if (disponibles == 0)
        cout << "ATENCION: no hay equipos libres para programar en este laboratorio.\n";
}

// ---------------------------------------------------------------------------
// ARCHIVOS BINARIOS DE SESIONES
// ---------------------------------------------------------------------------

// Calcula el proximo codigo de sesion disponible, contando cuantos registros
// SesionUso ya existen en el archivo binario. No hace falta guardar un
// contador aparte (ni una variable global): el propio tamano del archivo en
// bytes, dividido entre el tamano de un registro, dice cuantos registros hay.
int siguienteCodigoSesion() {
    ifstream archivo(ARCHIVO_SESIONES, ios::binary);
    if (!archivo) return 1;   // el archivo no existe todavia: sera la primera sesion (codigo 1)

    archivo.seekg(0, ios::end);   // mueve el cursor al final del archivo
    // tellg() devuelve la posicion actual del cursor, que en el final del
    // archivo coincide con el tamano total en bytes. Dividiendo entre el
    // tamano de un registro se obtiene cuantos registros completos hay.
    long total = (long)archivo.tellg() / (long)sizeof(SesionUso);
    archivo.close();

    return (int)total + 1;   // el siguiente codigo es uno mas que el total actual
}

// Agrega una nueva sesion al final del archivo binario. ios::app asegura que
// se escriba despues del ultimo registro existente, sin sobrescribir nada.
// write() copia byte por byte la memoria de la struct 's' directo al archivo
// -- por eso es indispensable que SesionUso no tenga ningun std::string
// adentro (solo char[] y tipos primitivos), o esos bytes no significarian
// nada al releerlos despues.
void guardarSesion(SesionUso s) {
    ofstream archivo(ARCHIVO_SESIONES, ios::binary | ios::app);
    archivo.write((char*)&s, sizeof(SesionUso));
    archivo.close();
}

// ---------------------------------------------------------------------------
// OPCION 4 DEL MENU
// ---------------------------------------------------------------------------

// Programa una sesion de uso de un equipo para un usuario, validando en
// cadena una serie de condiciones antes de crear la sesion. Si cualquier
// validacion falla, la funcion termina ahi mismo (return) sin crear nada.
void programarSesion(Equipo* equipos, int numEquipos, Usuario* usuarios, int numUsuarios) {
    // Validacion 0: que ya haya equipos y usuarios cargados en memoria
    if (numEquipos == 0 || numUsuarios == 0) {
        cout << "Debe cargar equipos (opcion 1) y usuarios (opcion 2) antes de programar.\n";
        return;
    }

    int codigoUsuario;
    if (!leerEntero("Codigo institucional del usuario: ", &codigoUsuario, 1, 999999999)) return;

    // Validacion 1: que exista un usuario con ese codigo
    Usuario* usuario = buscarUsuario(usuarios, numUsuarios, codigoUsuario);
    if (usuario == nullptr) {
        cout << "No existe un usuario con ese codigo.\n";
        return;
    }
    cout << "Usuario: " << usuario->nombre << " | " << usuario->programaAcademico
         << " | semestre " << usuario->semestre << "\n";

    int codigoEquipo;
    if (!leerEntero("Codigo del equipo: ", &codigoEquipo, 1, 999999999)) return;

    // Validacion 2: que exista un equipo con ese codigo
    Equipo* equipo = buscarEquipo(equipos, numEquipos, codigoEquipo);
    if (equipo == nullptr) {
        cout << "No existe un equipo con ese codigo.\n";
        return;
    }
    cout << "Equipo: " << equipo->nombre << " | laboratorio " << equipo->laboratorio
         << " | estado " << equipo->estadoOperativo << "\n";

    // Validacion 3: que el equipo este disponible (no en uso ni en mantenimiento)
    if (!estaDisponible(equipo)) {
        cout << "No se puede programar: el equipo esta en estado "
             << equipo->estadoOperativo << ".\n";
        return;
    }

    // Validacion 4: que el usuario curse un semestre igual o mayor al minimo exigido
    if (usuario->semestre < equipo->semestreMinimo) {
        cout << "No se puede programar: el equipo exige semestre "
             << equipo->semestreMinimo << " y el usuario cursa semestre "
             << usuario->semestre << ".\n";
        return;
    }

    // Todas las validaciones pasaron: se arma la nueva sesion
    SesionUso nueva;
    nueva.codigoSesion  = siguienteCodigoSesion();     // codigo correlativo automatico
    nueva.codigoEquipo  = equipo->codigo;
    nueva.codigoUsuario = usuario->codigoInstitucional;

    if (!leerTexto("Fecha de la sesion (dd/mm/aaaa): ", nueva.fecha, 20)) return;

    if (!leerEntero("Duracion estimada en horas (1 a 8): ", &nueva.duracionProgramada, 1, 8)) return;

    // Campos que se inicializan en su valor "de sesion recien creada"
    nueva.duracionReal = 0;                            // aun no se ha usado de verdad
    nueva.cerrada = false;                              // sigue abierta hasta que se use la opcion 5
    strcpy(nueva.observacion, "Sesion programada");     // observacion por defecto
    nueva.penalizacion = 0;                             // todavia no hay penalizacion que calcular

    guardarSesion(nueva);   // persiste la sesion en sesiones.dat

    // El equipo pasa a "En uso" -- se modifica DIRECTO en memoria a traves del
    // puntero que devolvio buscarEquipo, sin tocar ningun archivo de equipos
    // (el archivo de texto original de equipos nunca se reescribe).
    strcpy(equipo->estadoOperativo, "En uso");

    cout << "\nSESION PROGRAMADA\n";
    cout << "  Codigo de sesion: " << nueva.codigoSesion << "\n";
    cout << "  Equipo          : " << equipo->nombre << " (" << equipo->codigo << ")\n";
    cout << "  Usuario         : " << usuario->nombre << " (" << usuario->codigoInstitucional << ")\n";
    cout << "  Fecha           : " << nueva.fecha << "\n";
    cout << "  Duracion        : " << nueva.duracionProgramada << " horas\n";
    cout << "  El equipo queda en estado: " << equipo->estadoOperativo << "\n";
}

// ---------------------------------------------------------------------------
// OPCION 5 DEL MENU
// ---------------------------------------------------------------------------

// Cierra una sesion existente: lee el registro binario correspondiente, pide
// los datos reales de uso, calcula la penalizacion si aplica, y reescribe el
// MISMO registro en el archivo (no se agrega uno nuevo, se actualiza el que
// ya existia).
void cerrarSesion(Equipo* equipos, int numEquipos) {
    if (equipos == nullptr || numEquipos == 0) {
        cout << "Primero cargue los equipos (opcion 1).\n";
        return;
    }

    // Se abre en modo lectura Y escritura (ios::in | ios::out) porque esta
    // funcion necesita las dos operaciones sobre el mismo archivo.
    fstream archivo(ARCHIVO_SESIONES, ios::in | ios::out | ios::binary);
    if (!archivo) {
        cout << "No hay sesiones registradas.\n";
        return;
    }

    // Calcula cuantos registros hay en total (misma formula de siempre)
    archivo.seekg(0, ios::end);
    long total = (long)archivo.tellg() / (long)sizeof(SesionUso);

    int codigo;
    cout << "Codigo de la sesion a cerrar: ";
    cin >> codigo;
    cin.ignore(1000, '\n');   // limpia el '\n' pendiente antes del proximo getline

    // Valida que el codigo pedido este dentro del rango de sesiones existentes
    if (codigo < 1 || codigo > total) {
        cout << "Esa sesion no existe.\n";
        archivo.close();
        return;
    }

    // Formula de oro del acceso binario indexado: la posicion en bytes del
    // registro numero "codigo" es (codigo-1) * sizeof(struct). El -1 es
    // porque los codigos empiezan en 1 pero las posiciones de archivo
    // empiezan en 0.
    long posicion = (long)(codigo - 1) * (long)sizeof(SesionUso);

    SesionUso s;
    archivo.seekg(posicion, ios::beg);          // salta directo a ese registro
    archivo.read((char*)&s, sizeof(SesionUso)); // lo carga completo a la variable local s

    if (s.cerrada) {
        cout << "Esa sesion ya fue cerrada.\n";
        archivo.close();
        return;
    }

    // Busca el equipo de esta sesion en el arreglo en memoria (para poder
    // actualizar su estado operativo mas abajo)
    Equipo* pEquipo = buscarEquipo(equipos, numEquipos, s.codigoEquipo);
    if (pEquipo == nullptr) {
        cout << "El equipo de esa sesion no esta cargado en memoria.\n";
        archivo.close();
        return;
    }

    cout << "Equipo: " << pEquipo->nombre << "\n";
    cout << "Duracion programada: " << s.duracionProgramada << " horas\n";

    cout << "Duracion real (horas): ";
    cin >> s.duracionReal;
    cin.ignore(1000, '\n');

    cout << "Observaciones tecnicas: ";
    cin.getline(s.observacion, 100);

    // Calculo de la penalizacion: 3% del costo del equipo por cada hora que
    // se paso del tiempo programado. Si no hubo exceso, la penalizacion es 0.
    int horasExtra = s.duracionReal - s.duracionProgramada;
    if (horasExtra > 0)
        s.penalizacion = horasExtra * 0.03 * pEquipo->costoEstimado;
    else
        s.penalizacion = 0;

    // Si se reporta dano, el equipo queda en Mantenimiento; si no, vuelve a
    // estar Disponible para la siguiente reserva.
    char respuesta[10];
    cout << "Se reporta dano en el equipo? (si/no): ";
    cin.getline(respuesta, 10);
    if (strcmp(respuesta, "si") == 0) {
        strcpy(pEquipo->estadoOperativo, "Mantenimiento");
        cout << "Estado del equipo cambiado a mantenimiento.\n";
    } else {
        strcpy(pEquipo->estadoOperativo, "Disponible");
    }

    s.cerrada = true;   // marca la sesion como liquidada

    // IMPORTANTE: despues de archivo.read(), el cursor del archivo quedo
    // apuntando justo DESPUES del registro leido. Si se escribiera ahi sin
    // reposicionar, se pisaria el SIGUIENTE registro por error. Por eso es
    // obligatorio volver a posicionar el cursor con seekp() en el mismo lugar
    // antes de escribir.
    archivo.seekp(posicion, ios::beg);
    archivo.write((char*)&s, sizeof(SesionUso));   // reescribe el registro actualizado en su lugar original
    archivo.close();

    cout << "Sesion cerrada. Penalizacion: $" << s.penalizacion << "\n";
}

// ---------------------------------------------------------------------------
// OPCION 6 DEL MENU
// ---------------------------------------------------------------------------

// Genera un reporte de cual es el equipo con mas horas de uso acumuladas en
// cada laboratorio, recorriendo TODO el historial de sesiones cerradas.
void informeUsoIntensivo(Equipo* equipos, int numEquipos) {
    if (equipos == nullptr || numEquipos == 0) {
        cout << "Primero cargue los equipos (opcion 1).\n";
        return;
    }

    ifstream archivo(ARCHIVO_SESIONES, ios::binary);
    if (!archivo) {
        cout << "No hay sesiones registradas.\n";
        return;
    }

    // Arreglo dinamico "paralelo" a equipos: horas[i] va a guardar el total de
    // horas usadas del equipo que esta en equipos[i]. No tiene su propio
    // identificador -- la POSICION es lo que lo conecta con el arreglo de
    // equipos.
    int* horas = new int[numEquipos];
    int* pIni = horas;
    while (pIni < horas + numEquipos) {
        *pIni = 0;   // inicializa todo el arreglo en 0 antes de empezar a sumar
        pIni++;
    }

    // Recorre TODO el archivo de sesiones, registro por registro, mientras la
    // lectura tenga exito (el while termina solo cuando ya no hay mas registros)
    SesionUso s;
    while (archivo.read((char*)&s, sizeof(SesionUso))) {
        if (!s.cerrada) continue;   // las sesiones abiertas todavia no aportan horas reales

        // Busca el equipo de esta sesion recorriendo equipos y horas EN
        // SINCRONIA: pEq y pH avanzan siempre juntos, para que cuando pEq
        // encuentre el equipo correcto, pH este apuntando a SU casillero de
        // horas correspondiente (misma posicion en los dos arreglos).
        Equipo* pEq = equipos;
        int* pH = horas;
        while (pEq < equipos + numEquipos) {
            if (pEq->codigo == s.codigoEquipo) {
                *pH += s.duracionReal;   // suma las horas de esta sesion al total del equipo
                break;                    // ya se encontro y sumo, no hace falta seguir buscando
            }
            pEq++;
            pH++;
        }
    }
    archivo.close();

    cout << "\n--- USO INTENSIVO POR LABORATORIO ---\n";

    // Recorre los equipos agrupando por laboratorio (sin usar un arreglo de
    // "laboratorios ya vistos": en vez de eso, para cada equipo se revisa si
    // su laboratorio ya aparecio ANTES en el recorrido)
    Equipo* pLab = equipos;
    while (pLab < equipos + numEquipos) {

        // Revisa si el laboratorio de pLab ya se proceso en una vuelta anterior
        bool repetido = false;
        Equipo* pAnt = equipos;
        while (pAnt < pLab) {
            if (strcmp(pAnt->laboratorio, pLab->laboratorio) == 0) {
                repetido = true;
                break;
            }
            pAnt++;
        }

        // Si es la primera vez que aparece este laboratorio, se calcula y
        // muestra cual es su equipo con mas horas
        if (!repetido) {
            Equipo* pMejor = nullptr;
            int maxHoras = -1;

            // Recorre otra vez TODOS los equipos, pero solo se fija en los que
            // son de este mismo laboratorio, buscando el de mayor horas[]
            Equipo* pBusca = equipos;
            int* pHB = horas;
            while (pBusca < equipos + numEquipos) {
                if (strcmp(pBusca->laboratorio, pLab->laboratorio) == 0 && *pHB > maxHoras) {
                    maxHoras = *pHB;
                    pMejor = pBusca;
                }
                pBusca++;
                pHB++;
            }

            if (pMejor != nullptr && maxHoras > 0)
                cout << "--" << pLab->laboratorio << ": " << pMejor->nombre
                     << " (" << maxHoras << " horas)\n";
            else
                cout << "--" << pLab->laboratorio << ": sin horas registradas\n";
        }

        pLab++;
    }

    delete[] horas;   // libera el arreglo temporal: ya cumplio su proposito dentro de esta funcion
}

// ---------------------------------------------------------------------------
// OPCION 7 DEL MENU
// ---------------------------------------------------------------------------

// Genera el top 3 de usuarios "mas criticos", entendido como el promedio de
// penalizacion por sesion cerrada (penalizacion total dividida entre numero
// de sesiones) -- asi no se premia/castiga solo por usar mucho el sistema,
// sino por comportarse mal EN PROMEDIO cada vez que lo usa.
void rankingUsuariosCriticos(Usuario* usuarios, int numUsuarios) {
    if (usuarios == nullptr || numUsuarios == 0) {
        cout << "Primero cargue los usuarios (opcion 2).\n";
        return;
    }

    ifstream archivo(ARCHIVO_SESIONES, ios::binary);
    if (!archivo) {
        cout << "No hay sesiones registradas.\n";
        return;
    }

    // Tres arreglos "paralelos" a usuarios (misma idea que "horas" en la
    // opcion 6, pero aca se necesitan tres datos por usuario en vez de uno):
    float* penal = new float[numUsuarios];    // penalizacion acumulada de cada usuario
    int* conteo = new int[numUsuarios];        // cuantas sesiones cerradas tiene cada usuario
    bool* mostrado = new bool[numUsuarios];    // si ese usuario ya salio en el ranking (para no repetirlo)

    // Inicializa los tres arreglos en su valor "vacio" recorriendolos con tres
    // punteros que avanzan sincronizados
    float* pP = penal;
    int* pC = conteo;
    bool* pM = mostrado;
    while (pP < penal + numUsuarios) {
        *pP = 0;
        *pC = 0;
        *pM = false;
        pP++; pC++; pM++;
    }

    // Recorre todo el historial de sesiones cerradas, acumulando penalizacion
    // y conteo por usuario (misma tecnica de "recorrido sincronizado" que en
    // informeUsoIntensivo, pero buscando en el arreglo de usuarios)
    SesionUso s;
    while (archivo.read((char*)&s, sizeof(SesionUso))) {
        if (!s.cerrada) continue;

        Usuario* pU = usuarios;
        pP = penal;
        pC = conteo;
        while (pU < usuarios + numUsuarios) {
            if (pU->codigoInstitucional == s.codigoUsuario) {
                *pP += s.penalizacion;
                (*pC)++;
                break;
            }
            pU++; pP++; pC++;
        }
    }
    archivo.close();

    cout << "\n--- TOP 3 USUARIOS CRITICOS ---\n";

    // En vez de ordenar los 3 arreglos completos, se hace 3 veces una
    // busqueda del "mejor" (mayor indice) que todavia no haya sido mostrado
    // -- equivalente a las primeras 3 pasadas de un ordenamiento por
    // seleccion, pero deteniendose ahi porque solo se necesita el top 3.
    int puesto = 1;
    while (puesto <= 3) {
        Usuario* pMejor = nullptr;
        bool* pMarcaMejor = nullptr;
        float mejorIndice = -1;

        Usuario* pU = usuarios;
        pP = penal;
        pC = conteo;
        pM = mostrado;
        while (pU < usuarios + numUsuarios) {
            // Solo se considera un usuario si tiene al menos una sesion
            // cerrada Y todavia no ha sido mostrado en un puesto anterior
            if (*pC > 0 && *pM == false) {
                float indice = *pP / *pC;   // promedio de penalizacion por sesion
                if (indice > mejorIndice) {
                    mejorIndice = indice;
                    pMejor = pU;
                    pMarcaMejor = pM;   // recuerda cual casillero de "mostrado" hay que marcar
                }
            }
            pU++; pP++; pC++; pM++;
        }

        if (pMejor == nullptr) break;   // ya no quedan usuarios elegibles (menos de 3 con sesiones)

        cout << puesto << ". " << pMejor->nombre
             << " (cod " << pMejor->codigoInstitucional << ")"
             << " - indice de criticidad: " << mejorIndice << "\n";

        *pMarcaMejor = true;   // marca a este usuario como ya mostrado, para que no salga otra vez
        puesto++;
    }

    if (puesto == 1) cout << "Ningun usuario tiene sesiones cerradas.\n";

    // Libera los tres arreglos temporales -- ya cumplieron su funcion dentro
    // de esta llamada
    delete[] penal;
    delete[] conteo;
    delete[] mostrado;
}

// ---------------------------------------------------------------------------
// OPCION 8 DEL MENU
// ---------------------------------------------------------------------------

// Libera toda la memoria dinamica principal del programa antes de salir.
// Recibe doble puntero por la misma razon que cargarEquipos/cargarUsuarios:
// necesita reasignar (a nullptr) las variables puntero que viven en main.
void liberarMemoria(Equipo** equipos, int* numEquipos,
                    Usuario** usuarios, int* numUsuarios) {
    delete[] *equipos;      // libera el bloque de memoria de los equipos
    *equipos = nullptr;      // deja el puntero en nullptr para evitar un "dangling pointer"
    *numEquipos = 0;

    delete[] *usuarios;
    *usuarios = nullptr;
    *numUsuarios = 0;

    cout << "Memoria liberada. Saliendo del sistema.\n";
}

// ---------------------------------------------------------------------------
// FUNCION PRINCIPAL: bucle del menu
// ---------------------------------------------------------------------------
int main() {
    int menu;

    // Punteros inicializados en nullptr: todavia no hay equipos ni usuarios
    // cargados en memoria (se reservan recien cuando se usan las opciones 1 y 2)
    Equipo* equipos = nullptr;
    int numEquipos = 0;

    Usuario* usuarios = nullptr;
    int numUsuarios = 0;

    char nombreArchivoEquipos[100];
    char nombreArchivoUsuarios[100];

    int lecturasFallidas = 0;   // cuenta errores consecutivos de lectura del menu, para no quedar en loop infinito

    do {
        cout<<"\n--------MENU PRINCIPAL--------\n";
        cout<<"1.Cargar equipos desde archivo de texto \n";
        cout<<"2.Cargar usuarios desde archivo de texto \n";
        cout<<"3.Consultar estado operativo del laboratorio \n";
        cout<<"4.Programar una sesion de uso de equipo \n";
        cout<<"5.Registrar cierre de sesion y observaciones \n";
        cout<<"6.Generar informe de uso intensivo de equipos \n";
        cout<<"7.Generar ranking de usuarios criticos\n";
        cout<<"8.Salir \n";
        cout<<"Elige una opcion: ";

        if (!(cin >> menu)) {
            // el usuario escribio algo que no es un numero
            limpiarEntrada();
            menu = 0;                  // fuerza que caiga en el "default" del switch
            lecturasFallidas++;
            if (lecturasFallidas >= 3) {
                cout<<"\nDemasiados errores de lectura. Saliendo del sistema\n";
                break;   // corta el do-while de una vez, sin pasar por liberarMemoria
            }
        } else {
            limpiarEntrada();          // limpia el '\n' pendiente antes de cualquier getline posterior
            lecturasFallidas = 0;      // como esta lectura si funciono, se reinicia el contador de errores
        }

        switch (menu) {
            case 1:
                ingresarArchivo(nombreArchivoEquipos, 100);
                cargarEquipos(nombreArchivoEquipos, &equipos, &numEquipos);   // se pasa la DIRECCION de equipos/numEquipos
                break;
            case 2:
                ingresarArchivo(nombreArchivoUsuarios, 100);
                cargarUsuarios(nombreArchivoUsuarios, &usuarios, &numUsuarios);
                break;
            case 3:
                consultarEstadoLaboratorio(equipos, numEquipos);   // aca solo se LEE, no se reasigna el puntero -> puntero simple alcanza
                break;
            case 4:
                programarSesion(equipos, numEquipos, usuarios, numUsuarios);
                break;
            case 5:
                cerrarSesion(equipos, numEquipos);
                break;
            case 6:
                informeUsoIntensivo(equipos, numEquipos);
                break;
            case 7:
                rankingUsuariosCriticos(usuarios, numUsuarios);
                break;
            case 8:
                liberarMemoria(&equipos, &numEquipos, &usuarios, &numUsuarios);
                break;
            default:
                cout<<"Opcion invalida.\n";
        }
    } while (menu != 8);   // el bucle se repite hasta que el usuario elige salir

    return 0;
}