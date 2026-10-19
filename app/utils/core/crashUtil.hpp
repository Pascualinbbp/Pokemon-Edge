#pragma once
#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <thread>
#include <windows.h>
#include <dbghelp.h>
#include "pathsUtil.hpp"
#pragma comment(lib, "dbghelp.lib")

// Registro de los cierres que no pasan por una excepción de C++ (fallo de acceso a memoria, abort, parámetro inválido, bloqueo): escribe
// en el registro (app/logs/app.log) qué fallo fue, dónde y qué estaba haciendo el juego (última etapa marcada con stage()), y guarda un
// volcado (app/logs/crash.dmp). Un vigilante avisa también cuando el juego deja de responder.
class CrashUtil {
    public:
    // Instalar una sola vez, lo primero de main.
    static void install() {
        SetUnhandledExceptionFilter(&onException);
        std::set_terminate(&onTerminate);
        std::signal(SIGABRT, &onSignal);
        std::signal(SIGSEGV, &onSignal);
        std::signal(SIGFPE, &onSignal);
        _set_invalid_parameter_handler(&onInvalidParameter);
        _set_purecall_handler(&onPureCall);
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        std::thread(&watchdog).detach();
    }

    // Marca lo que está haciendo el juego ahora (literal con vida de todo el programa) y un detalle opcional (posición, hábitat...).
    static void stage(const char* name) { s_stage.store(name, std::memory_order_relaxed); }
    static void detail(const char* format, double a = 0.0, double b = 0.0, double c = 0.0) {
        char text[sizeof(s_detail)];
        std::snprintf(text, sizeof(text), format, a, b, c);
        std::memcpy(s_detail, text, sizeof(s_detail)); // línea corta: una carrera con el informe solo mezcla caracteres
    }

    // Cada vuelta del bucle principal: sin latidos durante WATCHDOG_SECONDS el juego se da por bloqueado.
    static void heartbeat() { s_beat.store(GetTickCount64(), std::memory_order_relaxed); }
    // El bucle espera a propósito (menús sin animación): el vigilante no cuenta ese tiempo.
    static void idle() { s_beat.store(0, std::memory_order_relaxed); }

    private:
    static constexpr ULONGLONG WATCHDOG_MS = 6000;
    static constexpr int MAX_FRAMES = 32;

    // Escritura directa al archivo: no usa el Logger (su cerrojo o su buffer pueden ser lo que falla).
    static void write(const char* text) {
        const HANDLE file = CreateFileW(PathsUtil::LOG_FILE_PATH.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return;
        DWORD written = 0;
        WriteFile(file, text, static_cast<DWORD>(std::strlen(text)), &written, nullptr);
        CloseHandle(file);
    }

    static void report(const char* title, const char* extra) {
        char text[1024];
        std::snprintf(text, sizeof(text), "[ERROR][CRASH] %s%s\n[ERROR][CRASH] Etapa: %s | %s\n", title, extra, s_stage.load() ? s_stage.load() : "?", s_detail);
        write(text);
    }

    // Módulo y desplazamiento de una dirección ("PokemonEdge.exe+0x1a2b3").
    static void location(const void* address, char* out, size_t size) {
        HMODULE module = nullptr;
        char name[MAX_PATH] = "?";
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, static_cast<const char*>(address), &module) && module) {
            GetModuleFileNameA(module, name, MAX_PATH);
            const char* slash = std::strrchr(name, '\\');
            std::snprintf(out, size, "%s+0x%llx", slash ? slash + 1 : name, static_cast<unsigned long long>(static_cast<const char*>(address) - reinterpret_cast<const char*>(module)));
        } else {
            std::snprintf(out, size, "%p", address);
        }
    }

    static void backtrace() {
        void* frames[MAX_FRAMES];
        const USHORT count = CaptureStackBackTrace(0, MAX_FRAMES, frames, nullptr);
        for (USHORT i = 0; i < count; ++i) {
            char where[300], line[340];
            location(frames[i], where, sizeof(where));
            std::snprintf(line, sizeof(line), "[ERROR][CRASH]   #%u %s\n", static_cast<unsigned>(i), where);
            write(line);
        }
    }

    static void dump(EXCEPTION_POINTERS* info) {
        static std::atomic<bool> done{ false };
        if (done.exchange(true)) return;
        const HANDLE file = CreateFileW(PathsUtil::CRASH_DUMP_PATH.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return;
        MINIDUMP_EXCEPTION_INFORMATION exception = { GetCurrentThreadId(), info, FALSE };
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpNormal, info ? &exception : nullptr, nullptr, nullptr);
        CloseHandle(file);
        write("[ERROR][CRASH] Volcado guardado en app/logs/crash.dmp\n");
    }

    static LONG WINAPI onException(EXCEPTION_POINTERS* info) {
        const EXCEPTION_RECORD* record = info->ExceptionRecord;
        char where[300], extra[420];
        location(record->ExceptionAddress, where, sizeof(where));
        if ((record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION || record->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) && record->NumberParameters >= 2) {
            const ULONG_PTR kind = record->ExceptionInformation[0];
            std::snprintf(extra, sizeof(extra), " en %s (%s la dirección 0x%llx)", where, kind == 0 ? "lectura de" : kind == 1 ? "escritura en" : "ejecución de",
                          static_cast<unsigned long long>(record->ExceptionInformation[1]));
        } else {
            std::snprintf(extra, sizeof(extra), " en %s", where);
        }
        char title[96];
        std::snprintf(title, sizeof(title), "Excepción 0x%08lx", static_cast<unsigned long>(record->ExceptionCode));
        report(title, extra);
        backtrace();
        dump(info);
        return EXCEPTION_EXECUTE_HANDLER;
    }

    static void onTerminate() {
        report("std::terminate (excepción de C++ sin capturar)", "");
        backtrace();
        dump(nullptr);
        std::abort();
    }

    static void onSignal(int signal) {
        char extra[32];
        std::snprintf(extra, sizeof(extra), " (señal %d)", signal);
        report("Señal fatal", extra);
        backtrace();
        dump(nullptr);
        std::_Exit(3);
    }

    static void onInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t) {
        report("Parámetro inválido en una función de la biblioteca de C", "");
        backtrace();
        dump(nullptr);
        std::_Exit(3);
    }

    static void onPureCall() {
        report("Llamada a una función virtual pura", "");
        backtrace();
        dump(nullptr);
        std::_Exit(3);
    }

    // Avisa una vez cuando el bucle principal lleva WATCHDOG_MS sin latir (bucle infinito o espera eterna) y guarda un volcado.
    static void watchdog() {
        heartbeat();
        bool warned = false;
        for (;;) {
            Sleep(1000);
            const ULONGLONG beat = s_beat.load(std::memory_order_relaxed);
            const ULONGLONG silent = beat == 0 ? 0 : GetTickCount64() - beat;
            if (silent < WATCHDOG_MS) {
                warned = false;
                continue;
            }
            if (warned) continue;
            warned = true;
            char extra[64];
            std::snprintf(extra, sizeof(extra), " (%llu s sin responder)", static_cast<unsigned long long>(silent / 1000));
            report("El juego no responde", extra);
            dump(nullptr);
        }
    }

    inline static std::atomic<const char*> s_stage{ "inicio" };
    inline static std::atomic<ULONGLONG> s_beat{ 0 };
    inline static char s_detail[160] = "";
};
