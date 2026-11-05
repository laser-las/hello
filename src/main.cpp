#include "aim.h"
#include "common.h"
#include "config.h"
#include "esp.h"
#include "game.h"
#include "mem.h"
#include "menu.h"
#include "misc.h"
#include "overlay.h"

#include <mmsystem.h>

#include <atomic>
#include <mutex>
#include <thread>

namespace {

std::atomic<bool> g_interrupted{false};

BOOL WINAPI consoleHandler(DWORD type) {
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT) {
        g_interrupted = true;
        return TRUE;
    }
    return FALSE;
}

struct KeyEdge {
    bool down = false;

    bool pressed(int vk) {
        if (vk <= 0) return false;
        const bool now = (GetAsyncKeyState(vk) & 0x8000) != 0;
        const bool edge = now && !down;
        down = now;
        return edge;
    }
};

struct Shared {
    std::mutex mutex;
    hx::World world;

    std::atomic<bool> esp{true};
    std::atomic<bool> aim{true};
    std::atomic<bool> trigger{true};
    std::atomic<bool> stop{false};
    std::atomic<bool> menuOpen{false};

    std::atomic<int> clientWidth{0};
    std::atomic<int> clientHeight{0};

    std::atomic<bool> hasAimTarget{false};
    std::atomic<float> aimX{0.0f};
    std::atomic<float> aimY{0.0f};
};

std::wstring exeDirectory() {
    wchar_t buffer[MAX_PATH * 2]{};
    const DWORD length =
        GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0])));
    std::wstring path(buffer, length);
    const size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash + 1);
}

void printUsage(const char* exe) {
    std::printf(
        "usage: %s [options]\n"
        "  --config <path>   config file (default: config.ini next to the exe)\n"
        "  --import <path>   apply a flat offset dump (dwEntityList = 0x..., m_iHealth: 344)\n"
        "  --save <path>     write the merged configuration back out\n"
        "  --preset <name>   legit | semilegit | semirage | rage\n"
        "  --dump            resolve offsets, print them and exit\n"
        "  --no-overlay      aim/trigger only, no ESP window\n",
        exe);
}

}  // namespace

int main(int argc, char** argv) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleTitleW(L"halcyon");
    SetProcessDPIAware();
    timeBeginPeriod(1);
    SetConsoleCtrlHandler(consoleHandler, TRUE);

    std::string configPath;
    std::string presetName;
    std::string importPath;
    std::string savePath;
    bool dumpOnly = false;
    bool withOverlay = true;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        } else if (arg == "--preset" && i + 1 < argc) {
            presetName = argv[++i];
        } else if (arg == "--import" && i + 1 < argc) {
            importPath = argv[++i];
        } else if (arg == "--save" && i + 1 < argc) {
            savePath = argv[++i];
        } else if (arg == "--dump") {
            dumpOnly = true;
        } else if (arg == "--no-overlay") {
            withOverlay = false;
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }
    }

    if (configPath.empty()) configPath = hx::narrow(exeDirectory() + L"config.ini");

    hx::logOpenFile(hx::narrow(exeDirectory() + L"halcyon.log").c_str());

    hx::Config cfg;
    if (!cfg.load(configPath.c_str())) {
        hx::logf("cannot read config: %s", configPath.c_str());
        return 1;
    }

    if (!importPath.empty()) {
        cfg.importOffsets(importPath.c_str());
    }

    if (!presetName.empty()) {
        if (presetName == "legit") {
            cfg.applyPreset(hx::Preset::Legit);
        } else if (presetName == "semilegit") {
            cfg.applyPreset(hx::Preset::SemiLegit);
        } else if (presetName == "semirage") {
            cfg.applyPreset(hx::Preset::SemiRage);
        } else if (presetName == "rage") {
            cfg.applyPreset(hx::Preset::Rage);
        } else {
            hx::logf("unknown preset: %s", presetName.c_str());
            return 1;
        }
        hx::logf("preset applied: %s", presetName.c_str());
    }

    if (!savePath.empty()) {
        if (cfg.save(savePath.c_str())) {
            hx::logf("config written to %s", savePath.c_str());
        } else {
            hx::logf("could not write %s", savePath.c_str());
        }
    }

    hx::logf("config loaded: %s", configPath.c_str());

    const hx::Layout layout = cfg.layout == "source1" ? hx::Layout::Source1 : hx::Layout::Source2;

    hx::Process process;
    while (!g_interrupted) {
        if (process.attach(cfg.process)) break;
        if (dumpOnly) {
            hx::logf("process %s is not running", hx::narrow(cfg.process).c_str());
            return 1;
        }
        hx::logf("waiting for %s ...", hx::narrow(cfg.process).c_str());
        Sleep(1000);
    }
    if (g_interrupted) return 0;

    hx::logf("attached to pid %u (%s), layout=%s", process.pid(),
             hx::narrow(cfg.process).c_str(), cfg.layout.c_str());

    const hx::ModuleInfo* client = process.module(cfg.clientModule);
    if (client) {
        hx::logf("module %s base=%p size=0x%zX", hx::narrow(client->name).c_str(),
                 reinterpret_cast<void*>(client->base), client->size);
    } else {
        hx::logf("module %s not found", hx::narrow(cfg.clientModule).c_str());
    }

    hx::Game game;
    game.init(process, cfg, layout);
    const hx::ResolvedOffsets offsets = game.resolve();

    if (dumpOnly) {
        hx::logf("fields: health=0x%X team=0x%X life=0x%X origin=0x%X bone=0x%X head_bone=%d stride=0x%X",
                 cfg.health, cfg.team, cfg.lifeState, cfg.origin, cfg.boneMatrix, cfg.headBone,
                 cfg.entityStride);
        hx::logf("optional: armor=0x%X weapon=0x%X ammo=0x%X velocity=0x%X punch=0x%X", cfg.armor,
                 cfg.weapon, cfg.ammo, cfg.velocity, cfg.aimPunch);
        hx::logf("resolved: entity_list=%p local_player=%p view_matrix=%p",
                 reinterpret_cast<void*>(offsets.entityList),
                 reinterpret_cast<void*>(offsets.localPlayer),
                 reinterpret_cast<void*>(offsets.viewMatrix));
        hx::logf("%s", offsets.valid() ? "all required offsets resolved"
                                       : "incomplete offsets, check config.ini");
        return offsets.valid() ? 0 : 2;
    }

    if (!offsets.valid()) {
        hx::logf("warning: offsets incomplete, nothing will be drawn until config.ini is filled in");
    }

    Shared shared;
    shared.esp = cfg.esp.enabled;
    shared.aim = cfg.aim.enabled;
    shared.trigger = cfg.trigger.enabled;

    std::thread scanner([&] {
        hx::Aim aim;
        hx::Misc misc;
        hx::World world;
        const double period = 1000.0 / static_cast<double>(std::max(30, cfg.aimHz));
        hx::Clock clock;

        while (!shared.stop.load() && !g_interrupted.load()) {
            clock.reset();

            if (!process.running()) {
                hx::logf("target process exited");
                shared.stop = true;
                break;
            }

            if (!game.update(world)) {
                hx::logf("memory read failed, stopping");
                shared.stop = true;
                break;
            }

            const bool menuOpen = shared.menuOpen.load(std::memory_order_relaxed);
            const int width = shared.clientWidth.load(std::memory_order_relaxed);
            const int height = shared.clientHeight.load(std::memory_order_relaxed);

            if (menuOpen) {
                aim.reset();
                misc.reset();
                shared.hasAimTarget = false;
            } else if (width > 0 && height > 0) {
                aim.tick(world, cfg, width, height, process.pid(), shared.aim.load(),
                         shared.trigger.load());
                const hx::AimStatus& status = aim.status();
                shared.hasAimTarget = status.hasTarget;
                shared.aimX = status.targetScreen.x;
                shared.aimY = status.targetScreen.y;

                misc.tick(world, cfg, process.pid());
            }

            {
                std::lock_guard<std::mutex> lock(shared.mutex);
                shared.world = world;
            }

            const double remaining = period - clock.ms();
            if (remaining >= 1.0) {
                Sleep(static_cast<DWORD>(remaining));
            } else if (remaining > 0.0) {
                std::this_thread::yield();
            }
        }

        aim.reset();
        misc.reset();
    });

    if (!withOverlay) {
        hx::logf("running without overlay, press the quit key to stop");
        KeyEdge quit;
        while (!shared.stop.load() && !g_interrupted.load()) {
            if (quit.pressed(cfg.quitKey)) break;
            Sleep(20);
        }
        shared.stop = true;
        scanner.join();
        hx::logf("stopped");
        return 0;
    }

    hx::Overlay overlay;
    if (!overlay.create(cfg.windowTitle, cfg.windowClass)) {
        hx::logf("overlay creation failed");
        shared.stop = true;
        scanner.join();
        return 1;
    }

    cfg.logSummary();

    hx::Menu menu;
    hx::MenuHost menuHost;
    menuHost.cfg = &cfg;
    menuHost.esp = &shared.esp;
    menuHost.aim = &shared.aim;
    menuHost.trigger = &shared.trigger;
    menuHost.stop = &shared.stop;
    menuHost.configPath = configPath;
    bool menuWasOpen = false;

    KeyEdge menuKey;
    KeyEdge espKey;
    KeyEdge aimKey;
    KeyEdge triggerKey;
    KeyEdge panicKey;
    KeyEdge quitKey;

    const double framePeriod = cfg.renderFps > 0 ? 1000.0 / cfg.renderFps : 0.0;
    hx::Clock frameClock;
    hx::Clock fpsClock;
    int frames = 0;
    float fps = 0.0f;

    while (!shared.stop.load() && !g_interrupted.load()) {
        const bool menuPressed = menuKey.pressed(cfg.menuKey);
        const bool quitPressed = quitKey.pressed(cfg.quitKey);
        const bool espPressed = espKey.pressed(cfg.espToggleKey);
        const bool aimPressed = aimKey.pressed(cfg.aimToggleKey);
        const bool triggerPressed = triggerKey.pressed(cfg.triggerToggleKey);
        const bool panicPressed = panicKey.pressed(cfg.panicKey);

        if (!menu.capturing()) {
            if (menuPressed) menu.setOpen(!menu.open());
            if (quitPressed) break;

            if (!menu.open()) {
                if (espPressed) shared.esp = !shared.esp.load();
                if (aimPressed) shared.aim = !shared.aim.load();
                if (triggerPressed) shared.trigger = !shared.trigger.load();
            }

            if (panicPressed) {
                shared.esp = false;
                shared.aim = false;
                shared.trigger = false;
                hx::logf("panic: everything disabled");
            }
        }

        if (menu.open() != menuWasOpen) {
            overlay.setInteractive(menu.open());
            if (!menu.open()) overlay.refocusGame();
            menuWasOpen = menu.open();
        }
        shared.menuOpen = menu.open();

        frameClock.reset();

        if (!process.running()) {
            hx::logf("target process exited");
            break;
        }

        const bool drawn = overlay.beginFrame();
        if (drawn) {
            shared.clientWidth = overlay.width();
            shared.clientHeight = overlay.height();

            hx::World world;
            {
                std::lock_guard<std::mutex> lock(shared.mutex);
                world = shared.world;
            }

            hx::EspState state;
            state.espOn = shared.esp.load();
            state.aimOn = shared.aim.load();
            state.triggerOn = shared.trigger.load();
            state.hasAimTarget = shared.hasAimTarget.load();
            state.aimTarget = hx::Vec2{shared.aimX.load(), shared.aimY.load()};
            state.fps = fps;
            state.scanMs = world.scanMs;
            state.players = world.players.size();
            state.preset = hx::widen(hx::presetName(cfg.preset));

            hx::drawEsp(overlay, world, cfg, state);
            overlay.endFrame();
        }

        ++frames;
        if (fpsClock.ms() >= 500.0) {
            fps = static_cast<float>(frames * 1000.0 / fpsClock.ms());
            frames = 0;
            fpsClock.reset();
        }

        const double remaining = framePeriod - frameClock.ms();
        if (remaining >= 1.0) Sleep(static_cast<DWORD>(remaining));
    }

    shared.stop = true;
    scanner.join();
    overlay.destroy();

    timeEndPeriod(1);
    hx::logf("stopped");
    return 0;
}
