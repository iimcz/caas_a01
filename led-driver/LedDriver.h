#ifndef _LED_DRIVER_H
#define _LED_DRIVER_H

#include <stdint.h>
#include <vector>
#include <queue>
#include <mutex>
#include <memory>
#include <libconfig.h++>

//#define RENDER_DEBUG
//#define CONTROL_SPI
#define CONTROL_ARTNET

#include "LedDefs.h"

#ifdef CONTROL_SPI
#include "rpi_ws281x/ws2811.h"
#endif //CONTROL_SPI
#ifdef CONTROL_ARTNET
#include <sys/socket.h>
#include <netinet/in.h>
#endif //CONTROL_ARTNET

enum AnimStage {
    kDark = 0,
    kStarting = 1,
    kIdle = 2,
    kWindup = 3,
    kExplosion = 4,
    kFade = 5,
};

class IAnimStageData {
public:
    AnimStage forStage() { return _stage; }

    float elapsed_time = 0.f;

    void update(float delta) { elapsed_time += delta; }

protected:
    IAnimStageData(AnimStage stage) : _stage(stage) {}
private:
    AnimStage _stage;
};

class DarkStageData : public IAnimStageData {
public:
    DarkStageData() : IAnimStageData(AnimStage::kDark) {}

    bool auto_reset = true;
    float reset_time = 30.f;
};

class StartingStageData : public IAnimStageData {
public:
    StartingStageData() : IAnimStageData(AnimStage::kStarting) {}

    float particle_position = 0.f;
    float particle_speed = 1.f;
    float particle_accel = 360.f / 15.0f;
    float target_speed = 360.f;
};

class IdleStageData : public IAnimStageData {
public:
    IdleStageData() : IAnimStageData(AnimStage::kIdle) {}

    float particle_position = 0.f;
    float particle_speed = 360.f;
    bool auto_advance = true;
    float advance_time = 2.0f;
};

class WindupStageData : public IAnimStageData {
public:
    WindupStageData() : IAnimStageData(AnimStage::kWindup) {}

    float first_particle_position = 0.f;
    float first_particle_speed = 0.f;
    float first_particle_accel = 360.f / 15.0f;
    float second_particle_position = 0.f;
    float second_particle_speed = 1.f;
    float second_particle_accel = 360.f / 15.0f;
    float target_speed = 355.0f;
};

class ExplosionStageData : public IAnimStageData {
public:
    ExplosionStageData() : IAnimStageData(AnimStage::kExplosion) {}

    float fill_ratio = 0.4f;
    float fill_rate = 0.2f;
};

class FadeStageData : public IAnimStageData {
public:
    FadeStageData() : IAnimStageData(AnimStage::kFade) {}
    float target_time = 10.f;
};

class LedDriver
{
public:
    LedDriver();

    void addLeds(uint32_t start, uint32_t count);
    void addHole(uint32_t size);
    void finalize();

    void advanceStage(AnimStage stage, bool force = false);
    void applyConfig(const libconfig::Config &config);

    void setPulsing(bool pulsing);
    void setColorScheme(color_t primary, color_t secondary, color_t fill);

    bool getPulsing() const;

    void update(float deltaTime);
    void render();
    void clear();

private:
    void initDark();
    void initStarting();
    void initIdle();
    void initWindup();
    void initExplosion();
    void initFade();

    void updateDark(float deltaTime);
    void updateStarting(float deltaTime);
    void updateIdle(float deltaTime);
    void updateWindup(float deltaTime);
    void updateExplosion(float deltaTime);
    void updateFade(float deltaTime);

    void drawCWLine(float angleFrom, float angleTo, const color_t& color);
    void drawCCWLine(float angleFrom, float angleTo, const color_t& color);
    void drawFill(float fillRatio, const color_t& color);
    void dimLeds(float multiplier, color_data_t addition);

    inline float partialLedFromAngle(float angle);

    // Setup
    std::queue<int64_t> _ledSetupQueue;

    color_t _primary;
    color_t _secondary;
    color_t _fill;

    // Runtime
    bool _running = false;
    bool _pulsing = false;
    float _pulseTime = 0.f;
    float _pulseValue = 1.f;
    std::shared_ptr<IAnimStageData> _stageData;
    std::shared_ptr<IAnimStageData> _nextStageData;

    std::vector<color_t> _ledsRing;

    std::mutex _stageMtx;

    struct {
        double starting_time = 1.0;
        double idle_speed = 80.0;
        double blink_rate = 1.0;
        double collision_time = 5.0;
        double collision_speed = 180.0;
        double reset_time = 30.0;
        bool auto_advance = true;
        bool allow_lower_stage_advance = false;
    } _configuration;

    // Rendering
#ifdef CONTROL_ARTNET
    int _sockfd;
    int _ledPaddingStart = 3;
    int _ledCountStart = 38;
    int _ledPaddingEnd = 3;
    int _ledCountEnd = 38;
    sockaddr_in _remote;
    std::string _remoteAddress = "127.0.0.1";
    void sendArtNet();
#endif // CONTROL_ARTNET
};

#endif // _LED_DRIVER_H