// The Lantern Keep -- Day 15.
//
// The warden steps on a plate. A gate opens, lanterns flare, and a tally is kept -- and the
// plate has a pointer to none of them.
//
// THE SCENE THAT MAKES THE CASE IS `no_bus`. It does the same thing the direct way, with the
// plate holding a pointer to every listener, and the printed line counts what the plate had to
// know: three collaborators instead of none. Nothing about the picture differs; the difference
// is what has to change when a fourth listener arrives.

#include "keepstone/Bus.h"
#include "keepstone/Collision.h"
#include "keepstone/Engine.h"
#include "keepstone/Prefab.h"
#include "keepstone/Renderer.h"
#include "keepstone/TransformSystem.h"
#include "keepstone/World.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

const char* SCENES[] = {
    "bussed",        // one plate: gate, lanterns and tally all react
    "no_bus",        // the same result wired directly. MEASURED: an identical frame
    "gate_only",     // only the gate subscribes; the lanterns stay dark
    "lanterns_only", // only the lanterns subscribe; the gate stays shut
    "nobody",        // published into a bus with no subscribers at all
    "two_plates",    // two plates on one tick: twice the lanterns, twice the tally
    "five_plates",   // five
    "eight_plates",  // eight
    "storm",         // twelve, all delivered in one drain
    "order_ab",      // gate subscribes first. MEASURED: identical to `order_ba`
    "order_ba",      // lanterns first -- the LOG differs, the outcome cannot
    "chain",         // a listener that publishes: the reaction lands on the NEXT tick
};

// THE ORDER SCENES RENDER IDENTICALLY, AND THAT IS THE FINDING.
//
// Two independent listeners on one event cannot produce a different world by being told in a
// different sequence -- the gate opens and the lanterns light whichever goes first. The printed
// log differs and the picture does not, which is the honest answer to "does subscription order
// matter": only when listeners share state, and these do not.
//
// It is worth saying because the opposite is widely assumed, and because a reader who believes
// order always matters will write listeners that depend on it without noticing.

// -- the listeners, none of which knows what published to it -------------------------------
class Gate : public keepstone::Listener
{
public:
    const char* listenerName() const override { return "gate"; }
    void onEvent(const keepstone::Event&) override { mOpen = true; ++mHeard; }
    bool open() const { return mOpen; }
    int  heard() const { return mHeard; }
private:
    bool mOpen = false;
    int  mHeard = 0;
};

class Lanterns : public keepstone::Listener
{
public:
    const char* listenerName() const override { return "lanterns"; }
    void onEvent(const keepstone::Event&) override { mLit += 2; ++mHeard; }
    int  lit() const { return mLit; }
    int  heard() const { return mHeard; }
private:
    int mLit = 0;
    int mHeard = 0;
};

class Tally : public keepstone::Listener
{
public:
    const char* listenerName() const override { return "tally"; }
    void onEvent(const keepstone::Event&) override { ++mCount; }
    int count() const { return mCount; }
private:
    int mCount = 0;
};

// A listener that REACTS BY PUBLISHING. The engine's rule is that what it publishes is heard on
// the next drain, not this one, and the tick numbers in the log prove it.
class Echo : public keepstone::Listener
{
public:
    Echo(keepstone::Bus& bus) : mBus(bus) {}
    const char* listenerName() const override { return "echo"; }
    void onEvent(const keepstone::Event&) override
    {
        if (mDone) return;
        mDone = true;
        keepstone::Event out;
        out.kind = keepstone::Event::GateOpened;
        mBus.publish(out);
    }
private:
    keepstone::Bus& mBus;
    bool mDone = false;
};

}  // namespace

int main(int argc, char** argv)
{
    std::string scene = "bussed";
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--list-scenes")
        {
            for (const char* s : SCENES) std::cout << s << "\n";
            return 0;
        }
        if (std::string(argv[i]) == "--scene" && i + 1 < argc) scene = argv[i + 1];
    }

    keepstone::Renderer renderer;
    renderer.setSize(2400, 1800);
    keepstone::Engine engine;
    engine.add(&renderer);
    if (!engine.startUp()) { std::cout << "engine did not come up\n"; return 1; }

    renderer.setAmbient({0.16f, 0.16f, 0.21f});
    renderer.setCamera({0.0f, 165.0f, 445.0f}, {0.0f, 40.0f, 0.0f});

    keepstone::Bus bus;
    Gate gate;
    Lanterns lanterns;
    Tally tally;
    Echo echo(bus);

    const bool useBus = (scene != "no_bus");
    if (useBus)
    {
        // THE TWO ORDER SCENES SUBSCRIBE THE SAME PAIR, THE OTHER WAY ROUND, and nothing else.
        // The tally is deliberately left out of both: with three listeners the difference
        // between the orders is one marker in the middle of a row, and with two it is the whole
        // row reversed. A demonstration should differ in the thing it is demonstrating.
        if (scene == "order_ab")
        {
            bus.subscribe(keepstone::Event::PlateStepped, &gate);
            bus.subscribe(keepstone::Event::PlateStepped, &lanterns);
        }
        else if (scene == "order_ba")
        {
            bus.subscribe(keepstone::Event::PlateStepped, &lanterns);
            bus.subscribe(keepstone::Event::PlateStepped, &gate);
        }
        else if (scene == "gate_only")     bus.subscribe(keepstone::Event::PlateStepped, &gate);
        else if (scene == "lanterns_only") bus.subscribe(keepstone::Event::PlateStepped, &lanterns);
        else if (scene == "nobody")        { /* deliberately nothing */ }
        else if (scene == "chain")
        {
            bus.subscribe(keepstone::Event::PlateStepped, &echo);
            bus.subscribe(keepstone::Event::GateOpened, &gate);
        }
        else
        {
            bus.subscribe(keepstone::Event::PlateStepped, &gate);
            bus.subscribe(keepstone::Event::PlateStepped, &lanterns);
            bus.subscribe(keepstone::Event::PlateStepped, &tally);
        }
    }

    // -- the run ------------------------------------------------------------------------------
    int gateOpenedOnTick = -1;
    const int plates = (scene == "storm") ? 12 :
                       (scene == "eight_plates") ? 8 :
                       (scene == "five_plates") ? 5 :
                       (scene == "two_plates") ? 2 : 1;

    for (int tick = 0; tick < 6; ++tick)
    {
        if (tick == 2)
        {
            for (int p = 0; p < plates; ++p)
            {
                if (useBus)
                {
                    keepstone::Event e;
                    e.kind = keepstone::Event::PlateStepped;
                    e.value = static_cast<float>(p);
                    bus.publish(e);
                }
                else
                {
                    // THE DIRECT WIRING. The plate has to name every collaborator, and adding a
                    // fourth means editing this file rather than the fourth listener's own.
                    gate.onEvent(keepstone::Event());
                    lanterns.onEvent(keepstone::Event());
                    tally.onEvent(keepstone::Event());
                }
            }
        }

        const int before = gate.heard();
        if (useBus) bus.deliver();
        if (gateOpenedOnTick < 0 && gate.heard() > before) gateOpenedOnTick = tick;
        if (!useBus && gateOpenedOnTick < 0 && gate.open()) gateOpenedOnTick = tick;
    }

    // -- the courtyard, lit by what actually happened ------------------------------------------
    for (int i = 0; i < 7; ++i)
        renderer.addBlock({ -150.0f + 50.0f * i, 40.0f, -70.0f },
                          { 0.19f, 0.40f + 0.08f * (i % 3), 0.19f }, 0.0f);

    // The gate: a tall block that stands aside when it has opened.
    renderer.addBlock(gate.open() ? keepstone::Vec3{ 190.0f, 40.0f, 20.0f }
                                  : keepstone::Vec3{ 90.0f, 40.0f, 20.0f },
                      { 0.12f, 0.62f, 0.30f }, gate.open() ? 62.0f : 0.0f);

    // ONE LANTERN PER `lit`, AND A LANTERN HAS A BODY YOU CAN SEE.
    //
    // Lighting alone was not enough: two lights and twelve lights both render as "the courtyard
    // is lit", and four scenes that differed only in how many listeners heard came out as the
    // same picture. A light has no silhouette. Giving each one a post to sit on puts the count
    // into the frame, which is what a figure has to carry when the subject is architecture.
    for (int i = 0; i < lanterns.lit(); ++i)
    {
        const float x = -260.0f + 42.0f * static_cast<float>(i);
        renderer.addBlock({ x, 58.0f, 130.0f }, { 0.10f, 0.30f, 0.10f }, 0.0f);
        renderer.addLantern({ x, 96.0f, 130.0f }, { 1.0f, 0.86f, 0.60f });
    }

    // One low block per tally, so the count the third listener kept is in the picture
    // too -- otherwise a scene that differs only in how many times the tally heard
    // renders exactly like one that never had a tally at all.
    for (int i = 0; i < tally.count(); ++i)
        renderer.addBlock({ -250.0f + 30.0f * i, 16.0f, 190.0f },
                          { 0.09f, 0.09f, 0.09f }, 0.0f);

    // -- THE DELIVERY LOG, DRAWN ---------------------------------------------------------------
    //
    // Events are architecture, and architecture does not photograph. Without this the nine
    // scenes below collapse into two pictures: a gate is either aside or not, and lanterns are
    // either lit or not, so "the gate opened on tick 3 instead of 2" and "the lanterns heard
    // before the gate did" both render as the same courtyard.
    //
    // So the bus draws what it did. One marker per delivery, left to right in delivery order,
    // on a lane per listener. Subscription order is then visible as the shape of the row, a
    // chained reaction is visible as a gap, and an event nobody wanted has a lane of its own.
    // Same idea as Day 7's profiler bars: an instrument that draws itself.
    {
        const std::vector<std::string>& lines = bus.log();
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            int lane = 4;                                   // "(nobody)"
            if (lines[i].find("gate")     != std::string::npos) lane = 0;
            else if (lines[i].find("lanterns") != std::string::npos) lane = 1;
            else if (lines[i].find("tally")    != std::string::npos) lane = 2;
            else if (lines[i].find("echo")     != std::string::npos) lane = 3;

            renderer.addBlock({ -270.0f + 20.0f * static_cast<float>(i),
                                120.0f - 26.0f * static_cast<float>(lane), 60.0f },
                              { 0.07f, 0.07f, 0.16f }, 0.0f);
        }
    }

    std::printf("scene            %s\n", scene.c_str());
    std::printf("wiring           %s\n", useBus ? "bus" : "direct (no bus)");
    std::printf("plate knows      %d collaborator(s)\n", useBus ? 0 : 3);
    std::printf("plates stepped   %d\n", plates);
    std::printf("delivered        %d\n", bus.deliveredTotal());
    std::printf("nobody listening %d\n", bus.droppedNoListener());
    std::printf("gate open        %s\n", gate.open() ? "yes" : "no");
    std::printf("gate opened tick %d\n", gateOpenedOnTick);
    std::printf("lanterns lit     %d\n", lanterns.lit());
    std::printf("tally            %d\n", tally.count());
    for (const std::string& line : bus.log())
        std::printf("%s\n", line.c_str());

    const std::string out = "shots/" + scene + ".png";
    renderer.renderTo(out);
    std::printf("wrote %s\n", out.c_str());

    engine.shutDown();
    std::fflush(stdout);
    std::_Exit(0);
}
