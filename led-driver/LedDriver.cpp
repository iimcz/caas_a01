#include <cmath>
#include <iostream>
#include <iomanip>
#include "LedDriver.h"
#include "ArtNet.h"

#ifdef CONTROL_ARTNET
#include <arpa/inet.h>
#include <unistd.h>
#endif // CONTROL_ARTNET

inline uint32_t RGBW(const color_t &color)
{
    return (static_cast<uint32_t>(color.r) << 24) +
           (static_cast<uint32_t>(color.g) << 16) +
           (static_cast<uint32_t>(color.b) << 8) +
           static_cast<uint32_t>(color.w);
}

void LedDriver::drawFill(float fillRatio, bool inner, const color_t &color)
{
    if (_pulsing)
    {
        fillRatio = fillRatio * _pulseValue;
    }
    if (inner)
    {
        for (auto &led : _ledsRingInner)
        {
            led.r = std::max(led.r, static_cast<uint8_t>(color.r * fillRatio));
            led.g = std::max(led.g, static_cast<uint8_t>(color.g * fillRatio));
            led.b = std::max(led.b, static_cast<uint8_t>(color.b * fillRatio));
            led.w = std::max(led.w, static_cast<uint8_t>(color.w * fillRatio));
        }
    }
    else
    {
        for (auto &led : _ledsRingOuter)
        {
            led.r = std::max(led.r, static_cast<uint8_t>(color.r * fillRatio));
            led.g = std::max(led.g, static_cast<uint8_t>(color.g * fillRatio));
            led.b = std::max(led.b, static_cast<uint8_t>(color.b * fillRatio));
            led.w = std::max(led.w, static_cast<uint8_t>(color.w * fillRatio));
        }
    }
}

void LedDriver::dimLeds(float multiplier, uint8_t addition)
{
    for (int i = 0; i < _ledsRingInner.size(); ++i)
    {
        _ledsRingInner[i].r = static_cast<uint8_t>(_ledsRingInner[i].r * multiplier) + addition;
        _ledsRingInner[i].g = static_cast<uint8_t>(_ledsRingInner[i].g * multiplier) + addition;
        _ledsRingInner[i].b = static_cast<uint8_t>(_ledsRingInner[i].b * multiplier) + addition;
        _ledsRingInner[i].w = static_cast<uint8_t>(_ledsRingInner[i].w * multiplier) + addition;
    }

    for (int i = 0; i < _ledsRingOuter.size(); ++i)
    {
        _ledsRingOuter[i].r = static_cast<uint8_t>(_ledsRingOuter[i].r * multiplier) + addition;
        _ledsRingOuter[i].g = static_cast<uint8_t>(_ledsRingOuter[i].g * multiplier) + addition;
        _ledsRingOuter[i].b = static_cast<uint8_t>(_ledsRingOuter[i].b * multiplier) + addition;
        _ledsRingOuter[i].w = static_cast<uint8_t>(_ledsRingOuter[i].w * multiplier) + addition;
    }
}

inline float LedDriver::partialLedFromAngle(float angle, bool innerRing)
{
    return innerRing ? _ledsRingInner.size() / 360.0f * angle : _ledsRingOuter.size() / 360.0f * angle;
}

#ifdef CONTROL_ARTNET
void LedDriver::sendArtNet()
{
    uint8_t buffer[ARTNET_FULL_PACKET_SIZE];

    constructArtNetPacket(buffer, &_ledsRingInner[0], (_ledInnerCountStart - _ledPaddingInnerStart), 1, 0);
    ssize_t sent = sendto(_sockfd, buffer, ARTNET_FULL_PACKET_SIZE, 0, (struct sockaddr *)&_remote, sizeof(_remote));
    if (sent < 0)
    {
        std::cout << "Failed to send ArtNet packet! Error: " << errno << std::endl;
    }

    constructArtNetPacket(buffer, &_ledsRingInner[_ledsRingInner.size() - _ledInnerCountEnd], -(_ledInnerCountEnd - _ledPaddingInnerEnd), 3, 0);
    sent = sendto(_sockfd, buffer, ARTNET_FULL_PACKET_SIZE, 0, (struct sockaddr *)&_remote, sizeof(_remote));
    if (sent < 0)
    {
        std::cout << "Failed to send ArtNet packet! Error: " << errno << std::endl;
    }

    constructArtNetPacket(buffer, &_ledsRingOuter[0], (_ledOuterCountStart - _ledPaddingOuterStart), 0, 0);
    sent = sendto(_sockfd, buffer, ARTNET_FULL_PACKET_SIZE, 0, (struct sockaddr *)&_remote, sizeof(_remote));
    if (sent < 0)
    {
        std::cout << "Failed to send ArtNet packet! Error: " << errno << std::endl;
    }

    constructArtNetPacket(buffer, &_ledsRingOuter[_ledsRingOuter.size() - _ledOuterCountEnd], -(_ledOuterCountEnd - _ledPaddingOuterEnd), 2, 0);
    sent = sendto(_sockfd, buffer, ARTNET_FULL_PACKET_SIZE, 0, (struct sockaddr *)&_remote, sizeof(_remote));
    if (sent < 0)
    {
        std::cout << "Failed to send ArtNet packet! Error: " << errno << std::endl;
    }
}
#endif // CONTROL_ARTNET

LedDriver::LedDriver()
{
    _primary =
        {.r = 250,
         .g = 50,
         .b = 50,
         .w = 255};
    _secondary =
        {.r = 50,
         .g = 250,
         .b = 50,
         .w = 255};
    _fill =
        {.r = 250,
         .g = 250,
         .b = 50,
         .w = 255};
#ifdef RENDER_DEBUG
    _ledsRingInner.resize(10);
    _ledsRingOuter.resize(10);
#else
#ifdef CONTROL_ARTNET
    _ledsRingInner.resize(_ledInnerCountStart + _ledInnerCountEnd);
    _ledsRingOuter.resize(_ledOuterCountStart + _ledOuterCountEnd);

    _remote.sin_addr.s_addr = inet_addr(_remoteAddress.c_str());
    _remote.sin_port = htons(ARTNET_PORT);
    _remote.sin_family = AF_INET;
#endif // CONTROL_ARTNET
#endif
}

void LedDriver::finalize()
{
#ifndef RENDER_DEBUG
#ifdef CONTROL_ARTNET
    _sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (_sockfd < 0)
    {
        std::cerr << "Failed to create network socket to send ArtNet packets." << std::endl;
        return;
    }
#endif // CONTROL_ARTNET
#endif // RENDER_DEBUG
    initDark();
}

void LedDriver::advanceStage(AnimStage stage, bool force)
{
    std::lock_guard<std::mutex> guard(_stageMtx);

    if (!force && stage <= _stageData->forStage() && !_configuration.allow_lower_stage_advance)
    {
        std::cout << "Ignoring switch to a lower stage." << std::endl;
        return;
    }

    switch (stage)
    {
    case AnimStage::kDark:
        initDark();
        break;
    case AnimStage::kStarting:
        initStarting();
        break;
    case AnimStage::kIdle:
        initIdle();
        break;
    case AnimStage::kWindup:
        initWindup();
        break;
    case AnimStage::kExplosion:
        initExplosion();
        break;
    case AnimStage::kFade:
        initFade();
        break;
    }
}

void LedDriver::setPulsing(bool pulsing)
{
    _pulsing = pulsing;
}

void LedDriver::setColorScheme(color_t primary, color_t secondary, color_t fill)
{
    this->_primary = primary;
    this->_secondary = secondary;
    this->_fill = fill;
}

bool LedDriver::getPulsing() const
{
    return _pulsing;
}

void LedDriver::applyConfig(const libconfig::Config &config)
{
#ifdef CONTROL_ARTNET
    config.lookupValue("led_driver.artnet.leds.inner_start", _ledInnerCountStart);
    config.lookupValue("led_driver.artnet.leds.inner_end", _ledInnerCountEnd);
    config.lookupValue("led_driver.artnet.leds.outer_start", _ledOuterCountStart);
    config.lookupValue("led_driver.artnet.leds.outer_end", _ledOuterCountEnd);

    config.lookupValue("led_driver.artnet.padding.inner_start", _ledPaddingInnerStart);
    config.lookupValue("led_driver.artnet.padding.inner_end", _ledPaddingInnerEnd);
    config.lookupValue("led_driver.artnet.padding.outer_start", _ledPaddingOuterStart);
    config.lookupValue("led_driver.artnet.padding.outer_end", _ledPaddingOuterEnd);

    _ledsRingInner.resize(_ledInnerCountEnd + _ledInnerCountEnd);
    _ledsRingOuter.resize(_ledOuterCountEnd + _ledOuterCountStart);

    config.lookupValue("led_driver.artnet.controller_ip", _remoteAddress);
    _remote.sin_addr.s_addr = inet_addr(_remoteAddress.c_str());
#endif // CONTROL_ARTNET

    config.lookupValue("led_driver.reset_time", _configuration.reset_time);
    config.lookupValue("led_driver.auto_advance", _configuration.auto_advance);
    config.lookupValue("led_driver.blink_rate", _configuration.blink_rate);
    config.lookupValue("led_driver.idle_speed", _configuration.idle_speed);
    config.lookupValue("led_driver.starting_time", _configuration.starting_time);
    config.lookupValue("led_driver.collision_speed", _configuration.collision_speed);
    config.lookupValue("led_driver.collision_time", _configuration.collision_time);
    config.lookupValue("led_driver.allow_lower_stage_advance", _configuration.allow_lower_stage_advance);

    uint32_t r = _primary.r;
    uint32_t g = _primary.g;
    uint32_t b = _primary.b;
    uint32_t w = _primary.w;
    config.lookupValue("led_driver.colors.primary.r", r);
    config.lookupValue("led_driver.colors.primary.g", g);
    config.lookupValue("led_driver.colors.primary.b", b);
    config.lookupValue("led_driver.colors.primary.w", w);
    _primary.r = static_cast<uint8_t>(std::min(255u, r));
    _primary.g = static_cast<uint8_t>(std::min(255u, g));
    _primary.b = static_cast<uint8_t>(std::min(255u, b));
    _primary.w = static_cast<uint8_t>(std::min(255u, w));

    r = _secondary.r;
    g = _secondary.g;
    b = _secondary.b;
    w = _secondary.w;
    config.lookupValue("led_driver.colors.secondary.r", r);
    config.lookupValue("led_driver.colors.secondary.g", g);
    config.lookupValue("led_driver.colors.secondary.b", b);
    config.lookupValue("led_driver.colors.secondary.w", w);
    _secondary.r = static_cast<uint8_t>(std::min(255u, r));
    _secondary.g = static_cast<uint8_t>(std::min(255u, g));
    _secondary.b = static_cast<uint8_t>(std::min(255u, b));
    _secondary.w = static_cast<uint8_t>(std::min(255u, w));

    r = _fill.r;
    g = _fill.g;
    b = _fill.b;
    w = _fill.w;
    config.lookupValue("led_driver.colors.fill.r", r);
    config.lookupValue("led_driver.colors.fill.g", g);
    config.lookupValue("led_driver.colors.fill.b", b);
    config.lookupValue("led_driver.colors.fill.w", w);
    _fill.r = static_cast<uint8_t>(std::min(255u, r));
    _fill.g = static_cast<uint8_t>(std::min(255u, g));
    _fill.b = static_cast<uint8_t>(std::min(255u, b));
    _fill.w = static_cast<uint8_t>(std::min(255u, w));
}

void LedDriver::update(float deltaTime)
{
    _pulseTime += M_PI * _configuration.blink_rate * deltaTime;
    _pulseValue = (sin(_pulseTime) + 1.0f) / 2.0f;
    {
        std::lock_guard<std::mutex> guard(_stageMtx);
        if (_stageData != _nextStageData)
        {
            std::cout << "Switching to stage: " << _nextStageData->forStage() << std::endl;
            _stageData = _nextStageData;
        }
    }

    switch (_stageData->forStage())
    {
    case AnimStage::kDark:
        updateDark(deltaTime);
        break;
    case AnimStage::kStarting:
        updateStarting(deltaTime);
        break;
    case AnimStage::kIdle:
        updateIdle(deltaTime);
        break;
    case AnimStage::kWindup:
        updateWindup(deltaTime);
        break;
    case AnimStage::kExplosion:
        updateExplosion(deltaTime);
        break;
    case AnimStage::kFade:
        updateFade(deltaTime);
        break;
    }
}

void LedDriver::render()
{
#ifdef RENDER_DEBUG
    for (auto color : _ledsRingInner)
    {
        std::cout << std::setw(8) << std::setfill('0') << std::hex;
        std::cout << RGBW(color) << " ";
    }
    std::cout << std::endl;
    for (auto color : _ledsRingOuter)
    {
        std::cout << std::setw(8) << std::setfill('0') << std::hex;
        std::cout << RGBW(color) << " ";
    }
    std::cout << std::endl
              << std::dec;
#else
#ifdef CONTROL_ARTNET
    sendArtNet();
#endif // CONTROL_ARTNET
#endif
}

void LedDriver::clear()
{
    for (auto &color : _ledsRingInner)
    {
        color.r = color.g = color.b = color.w = 0;
    }
    for (auto &color : _ledsRingOuter)
    {
        color.r = color.g = color.b = color.w = 0;
    }
    render();

#ifdef CONTROL_ARTNET
    if (_sockfd > 0)
    {
        close(_sockfd);
    }
#endif // CONTROL_ARTNET
}

void LedDriver::initDark()
{
    auto dark = std::make_shared<DarkStageData>();
    dark->reset_time = _configuration.reset_time;
    _pulsing = false;
    _nextStageData = std::move(dark);
}

void LedDriver::initStarting()
{
    auto starting = std::make_shared<StartingStageData>();
    starting->target_speed = _configuration.idle_speed;
    starting->particle_accel = _configuration.idle_speed / _configuration.starting_time;
    _nextStageData = std::move(starting);
}

void LedDriver::initIdle()
{
    auto idle = std::make_shared<IdleStageData>();
    idle->particle_speed = _configuration.idle_speed;
    if (_stageData->forStage() == AnimStage::kStarting)
    {
        auto starting = std::static_pointer_cast<StartingStageData>(_stageData);
        idle->particle_position = starting->particle_position;
    }
    idle->auto_advance = _configuration.auto_advance;
    _nextStageData = std::move(idle);
}

void LedDriver::initWindup()
{
    auto windup = std::make_shared<WindupStageData>();
    if (_stageData->forStage() == AnimStage::kIdle)
    {
        auto idle = std::static_pointer_cast<IdleStageData>(_stageData);
        windup->first_particle_position = idle->particle_position;
        windup->first_particle_speed = idle->particle_speed;
    }
    else if (_stageData->forStage() == AnimStage::kStarting)
    {
        auto starting = std::static_pointer_cast<StartingStageData>(_stageData);
        windup->first_particle_position = starting->particle_position;
        windup->first_particle_speed = starting->particle_speed;
    }
    windup->second_particle_speed = windup->first_particle_speed / 2.0f;
    windup->target_speed = _configuration.collision_speed;
    windup->first_particle_accel = _configuration.collision_speed / _configuration.collision_time;
    windup->second_particle_accel = _configuration.collision_speed / _configuration.collision_time;
    _nextStageData = std::move(windup);
}

void LedDriver::initExplosion()
{
    _nextStageData = std::make_shared<ExplosionStageData>();
}

void LedDriver::initFade()
{
    _nextStageData = std::make_shared<FadeStageData>();
}

void LedDriver::updateDark(float deltaTime)
{
    auto data = std::static_pointer_cast<DarkStageData>(_stageData);
    data->update(deltaTime);

    if (data->auto_reset && data->elapsed_time > data->reset_time)
    {
        advanceStage(AnimStage::kStarting);
    }
}

void LedDriver::updateStarting(float deltaTime)
{
    auto data = std::static_pointer_cast<StartingStageData>(_stageData);
    data->update(deltaTime);

    dimLeds(0.8f, 0);

    float last_position = data->particle_position;
    data->particle_position += data->particle_speed * deltaTime;
    if (data->particle_position > 360.0f)
    {
        data->particle_position -= 360.0f;
    }
    drawCWLine(last_position, data->particle_position, false, _primary);

    data->particle_speed += data->particle_accel * deltaTime;
    if (data->particle_speed > data->target_speed)
    {
        advanceStage(AnimStage::kIdle);
    }
}

void LedDriver::updateIdle(float deltaTime)
{
    auto data = std::static_pointer_cast<IdleStageData>(_stageData);
    data->update(deltaTime);

    dimLeds(0.8f, 0);

    float last_position = data->particle_position;
    data->particle_position += data->particle_speed * deltaTime;
    if (data->particle_position > 360.0f)
    {
        data->particle_position -= 360.0f;
    }
    drawCWLine(last_position, data->particle_position, false, _primary);

    if (data->auto_advance && data->elapsed_time > data->advance_time)
    {
        advanceStage(AnimStage::kWindup);
    }
}

void LedDriver::updateWindup(float deltaTime)
{
    auto data = std::static_pointer_cast<WindupStageData>(_stageData);
    data->update(deltaTime);

    dimLeds(0.8f, 0);

    float first_last_position = data->first_particle_position;
    data->first_particle_position += data->first_particle_speed * deltaTime;
    if (data->first_particle_position > 360.f)
    {
        data->first_particle_position -= 360.f;
    }

    float second_last_position = data->second_particle_position;
    data->second_particle_position -= data->second_particle_speed * deltaTime;
    if (data->second_particle_position < 0.f)
    {
        data->second_particle_position += 360.f;
    }

    data->first_particle_speed += data->first_particle_accel * deltaTime;
    if (data->first_particle_speed > data->target_speed)
    {
        data->first_particle_speed = data->target_speed;
    }

    data->second_particle_speed += data->second_particle_accel * deltaTime;
    if (data->second_particle_speed > data->target_speed)
    {
        data->second_particle_speed = data->target_speed;
    }

    if (data->second_particle_speed == data->target_speed
        && (data->first_particle_position - data->second_particle_position) < 5.0f
        && ((data->first_particle_position > 30.f && data->first_particle_position < 180.f - 30.f)
            || (data->first_particle_position < 360.f - 30.f && data->first_particle_position > 180.f + 30.f)))
    {
        advanceStage(AnimStage::kExplosion);
    }
    else
    {
        drawCWLine(first_last_position, data->first_particle_position, false, _primary);
        drawCCWLine(second_last_position, data->second_particle_position, true, _secondary);
    }
}

void LedDriver::updateExplosion(float deltaTime)
{
    auto data = std::static_pointer_cast<ExplosionStageData>(_stageData);
    data->update(deltaTime);

    data->fill_ratio += data->fill_rate * deltaTime;
    if (data->fill_ratio > 1.0f)
    {
        advanceStage(AnimStage::kFade);
    }
    drawFill(data->fill_ratio, false, _fill);
    drawFill(data->fill_ratio, true, _fill);
}

void LedDriver::updateFade(float deltaTime)
{
    auto data = std::static_pointer_cast<FadeStageData>(_stageData);
    data->update(deltaTime);
    dimLeds(0.97f, 0);

    if (data->elapsed_time > data->target_time)
    {
        advanceStage(AnimStage::kDark, true);
    }
}

void LedDriver::drawCWLine(float angleFrom, float angleTo, bool inner, const color_t &color)
{
    color_t realColor = {
        .r = _pulsing ? static_cast<uint8_t>(color.r * _pulseValue) : color.r,
        .g = _pulsing ? static_cast<uint8_t>(color.g * _pulseValue) : color.g,
        .b = _pulsing ? static_cast<uint8_t>(color.b * _pulseValue) : color.b,
        .w = _pulsing ? static_cast<uint8_t>(color.w * _pulseValue) : color.w,
    };

    float ledFrom = partialLedFromAngle(angleFrom, inner);
    float ledTo = partialLedFromAngle(angleTo, inner);

    float ledToI;
    float ledToP = modff32(ledTo, &ledToI);
    ledToP = ledToP * 0.5f;
    float ledFromI;
    modff32(ledFrom, &ledFromI);

    uint32_t iledFrom = static_cast<uint32_t>(ledFromI);
    uint32_t iledTo = static_cast<uint32_t>(ledToI);

    std::vector<color_t> &target = inner ? _ledsRingInner : _ledsRingOuter;

    size_t partialTo = ((iledTo + 1) % target.size());
    target[partialTo].r = static_cast<uint8_t>(realColor.r * ledToP);
    target[partialTo].g = static_cast<uint8_t>(realColor.g * ledToP);
    target[partialTo].b = static_cast<uint8_t>(realColor.b * ledToP);
    target[partialTo].w = static_cast<uint8_t>(realColor.w * ledToP);

    if (angleTo > angleFrom)
    {
        for (uint32_t i = iledFrom; i <= iledTo; ++i)
        {
            target[i].r = realColor.r;
            target[i].g = realColor.g;
            target[i].b = realColor.b;
            target[i].w = realColor.w;
        }
    }
    else
    {
        for (uint32_t i = iledFrom; i < target.size(); ++i)
        {
            target[i].r = realColor.r;
            target[i].g = realColor.g;
            target[i].b = realColor.b;
            target[i].w = realColor.w;
        }
        for (uint32_t i = 0; i <= iledTo; ++i)
        {
            target[i].r = realColor.r;
            target[i].g = realColor.g;
            target[i].b = realColor.b;
            target[i].w = realColor.w;
        }
    }
}

void LedDriver::drawCCWLine(float angleFrom, float angleTo, bool inner, const color_t &color)
{
    color_t realColor = {
        .r = _pulsing ? static_cast<uint8_t>(color.r * _pulseValue) : color.r,
        .g = _pulsing ? static_cast<uint8_t>(color.g * _pulseValue) : color.g,
        .b = _pulsing ? static_cast<uint8_t>(color.b * _pulseValue) : color.b,
        .w = _pulsing ? static_cast<uint8_t>(color.w * _pulseValue) : color.w,
    };

    float ledFrom = partialLedFromAngle(angleFrom, inner);
    float ledTo = partialLedFromAngle(angleTo, inner);

    float ledToI;
    float ledToP = 1.0f - modff32(ledTo, &ledToI);
    ledToP = ledToP * 0.5f;
    float ledFromI;
    modff32(ledFrom, &ledFromI);

    int32_t iledFrom = static_cast<int32_t>(ledFromI);
    int32_t iledTo = static_cast<int32_t>(ledToI);

    std::vector<color_t> &target = inner ? _ledsRingInner : _ledsRingOuter;

    size_t partialTo = ((iledTo - 1 + target.size()) % target.size());
    target[partialTo].r = static_cast<uint8_t>(realColor.r * ledToP);
    target[partialTo].g = static_cast<uint8_t>(realColor.g * ledToP);
    target[partialTo].b = static_cast<uint8_t>(realColor.b * ledToP);
    target[partialTo].w = static_cast<uint8_t>(realColor.w * ledToP);

    if (angleTo < angleFrom)
    {
        for (int32_t i = iledFrom; i >= iledTo; --i)
        {
            target[i].r = realColor.r;
            target[i].g = realColor.g;
            target[i].b = realColor.b;
            target[i].w = realColor.w;
        }
    }
    else
    {
        for (int32_t i = iledFrom; i >= 0; --i)
        {
            target[i].r = realColor.r;
            target[i].g = realColor.g;
            target[i].b = realColor.b;
            target[i].w = realColor.w;
        }
        for (int32_t i = target.size() - 1; i >= iledTo; --i)
        {
            target[i].r = realColor.r;
            target[i].g = realColor.g;
            target[i].b = realColor.b;
            target[i].w = realColor.w;
        }
    }
}
